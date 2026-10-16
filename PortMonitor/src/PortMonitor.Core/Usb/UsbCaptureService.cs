using System.Diagnostics;
using System.Text.RegularExpressions;

namespace PortMonitor.Core.Usb;

/// <summary>
/// USB capture via the user-installed, signed USBPcap driver.
/// Live: launches USBPcapCMD.exe writing a pcap stream to stdout (no driver of our own).
/// Offline: reads .pcap / .pcapng files captured by USBPcap or Wireshark.
/// </summary>
public sealed class UsbCaptureService : IAsyncDisposable
{
    public const string DefaultCmdPath = @"C:\Program Files\USBPcap\USBPcapCMD.exe";

    private Process? _proc;
    private CancellationTokenSource? _cts;
    private Task? _reader;
    private long _index;

    public event Action<UsbPacket>? PacketCaptured;
    public event Action<string>? Error;
    public event Action? Stopped;

    public bool IsRunning => _proc is { HasExited: false };

    /// <summary>Lists USBPcap interfaces via --extcap-interfaces; falls back to \\.\USBPcap1..10.</summary>
    public static async Task<IReadOnlyList<string>> ListInterfacesAsync(string cmdPath)
    {
        var list = new List<string>();
        if (OperatingSystem.IsWindows() && File.Exists(cmdPath))
        {
            try
            {
                var psi = new ProcessStartInfo(cmdPath, "--extcap-interfaces")
                {
                    RedirectStandardOutput = true, UseShellExecute = false, CreateNoWindow = true
                };
                using var p = Process.Start(psi)!;
                var text = await p.StandardOutput.ReadToEndAsync();
                await p.WaitForExitAsync();
                foreach (Match m in Regex.Matches(text, @"value=(\\\\\.\\USBPcap\d+)"))
                    if (!list.Contains(m.Groups[1].Value)) list.Add(m.Groups[1].Value);
            }
            catch { /* fall through */ }
        }
        if (list.Count == 0)
            for (int i = 1; i <= 10; i++) list.Add($@"\\.\USBPcap{i}");
        return list;
    }

    public static bool IsAdministrator()
    {
        if (!OperatingSystem.IsWindows()) return false;
        try
        {
            using var id = System.Security.Principal.WindowsIdentity.GetCurrent();
            return new System.Security.Principal.WindowsPrincipal(id)
                .IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator);
        }
        catch { return false; }
    }

    /// <summary>Precondition check for live capture; returns a user-facing message or null if OK.</summary>
    public static string? CheckLivePrerequisites(string cmdPath)
    {
        if (!OperatingSystem.IsWindows())
            return "实时 USB 抓包仅支持 Windows（需安装 USBPcap）。当前系统可用「打开 pcap 文件」离线分析。";
        if (!File.Exists(cmdPath))
            return $"未找到 USBPcapCMD.exe：{cmdPath}\n请先安装 USBPcap（https://desowin.org/usbpcap/），安装后需重启，或修改路径。";
        if (!IsAdministrator())
            return "需要以管理员身份运行 PortMonitor 才能通过 USBPcap 抓包（右键 → 以管理员身份运行）。";
        return null;
    }

    public void StartLive(string cmdPath, string iface, int? deviceAddress)
    {
        if (IsRunning) throw new InvalidOperationException("抓包已在运行");
        var err = CheckLivePrerequisites(cmdPath);
        if (err is not null) throw new InvalidOperationException(err);

        string filter = deviceAddress is { } d ? $"--devices {d}" : "-A";
        var psi = new ProcessStartInfo(cmdPath, $"-d \"{iface}\" -o - {filter}")
        {
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };
        _proc = Process.Start(psi) ?? throw new InvalidOperationException("无法启动 USBPcapCMD");
        _cts = new CancellationTokenSource();
        var proc = _proc;
        var ct = _cts.Token;
        _ = Task.Run(async () =>
        {
            try
            {
                var e = await proc.StandardError.ReadToEndAsync(ct);
                if (!string.IsNullOrWhiteSpace(e)) Error?.Invoke("USBPcapCMD: " + e.Trim());
            }
            catch { }
        });
        _reader = Task.Run(() =>
        {
            try { Consume(proc.StandardOutput.BaseStream, ct); }
            catch (Exception ex) when (!ct.IsCancellationRequested) { Error?.Invoke("读取抓包流失败：" + ex.Message); }
            finally { Stopped?.Invoke(); }
        });
    }

    /// <summary>Synchronously parses a capture file, raising PacketCaptured for every USBPcap record.</summary>
    public int LoadFile(string path)
    {
        using var fs = File.OpenRead(path);
        return Consume(fs, CancellationToken.None);
    }

    private int Consume(Stream s, CancellationToken ct)
    {
        int n = 0, skipped = 0;
        foreach (var rec in PcapReader.Read(s, ct))
        {
            if (rec.LinkType != UsbPcapHeader.LinkType) { skipped++; continue; }
            var pkt = UsbPcapHeader.Parse(rec.Data, rec.Timestamp, Interlocked.Increment(ref _index));
            if (pkt is null) { skipped++; continue; }
            PacketCaptured?.Invoke(pkt);
            n++;
        }
        if (n == 0 && skipped > 0)
            Error?.Invoke($"文件中没有 USBPcap 记录（链路类型不是 {UsbPcapHeader.LinkType}），已跳过 {skipped} 条");
        return n;
    }

    public async Task StopAsync()
    {
        _cts?.Cancel();
        try { if (_proc is { HasExited: false }) _proc.Kill(entireProcessTree: true); } catch { }
        if (_reader is not null) { try { await _reader; } catch { } }
        _proc?.Dispose();
        _proc = null; _reader = null;
        _cts?.Dispose(); _cts = null;
    }

    public void ResetIndex() => _index = 0;

    public ValueTask DisposeAsync() => new(StopAsync());
}
