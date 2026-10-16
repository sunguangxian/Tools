using System.Collections.ObjectModel;
using Avalonia.Media;
using Avalonia.Media.Immutable;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using PortMonitor.Core.Models;
using PortMonitor.Core.Usb;

namespace PortMonitor.App.ViewModels;

public sealed class UsbTransferRow(UsbPacket p)
{
    public long Index { get; } = p.Index;
    public string Time { get; } = p.Timestamp.ToLocalTime().ToString("HH:mm:ss.ffffff");
    public string Address { get; } = p.Address;
    public string Direction { get; } = $"{p.DirectionLabel} {p.StageLabel}";
    public string Type { get; } = p.TransferLabel;
    public int Length { get; } = p.Data.Length;
    public string Status { get; } = p.StatusLabel;
    // Immutable brushes: rows are built on the capture thread.
    private static readonly IBrush Ok = new ImmutableSolidColorBrush(Color.Parse("#7F849C"));
    private static readonly IBrush Bad = new ImmutableSolidColorBrush(Color.Parse("#F38BA8"));
    public IBrush StatusBrush { get; } = p.StatusOk ? Ok : Bad;
    public string Hex { get; } = SerialEvent.ToHex(p.Data.AsSpan(0, Math.Min(64, p.Data.Length))) + (p.Data.Length > 64 ? " …" : "");
}

public sealed class ScsiRow(ScsiCommand c)
{
    public ScsiCommand Command { get; } = c;
    public long Index { get; } = c.Index;
    public string Time { get; } = c.Timestamp.ToLocalTime().ToString("HH:mm:ss.ffffff");
    public string Device { get; } = c.DeviceLabel;
    public string Tag { get; } = $"0x{c.Tag:X}";
    public byte Lun { get; } = c.Lun;
    public string Opcode { get; } = c.OpcodeName;
    public string Lba { get; } = c.Info.Lba?.ToString() ?? "";
    public string Size { get; } = c.Info.Blocks is { } b ? $"{b} 块" : (c.ExpectedLength > 0 ? $"{c.ExpectedLength} B" : "");
    public string Status { get; } = c.StatusLabel;
    private static readonly IBrush Passed = new ImmutableSolidColorBrush(Color.Parse("#A6E3A1"));
    private static readonly IBrush Failed = new ImmutableSolidColorBrush(Color.Parse("#F38BA8"));
    private static readonly IBrush Phase = new ImmutableSolidColorBrush(Color.Parse("#FAB387"));
    private static readonly IBrush Pending = new ImmutableSolidColorBrush(Color.Parse("#6C7086"));
    public IBrush StatusBrush { get; } = c.Status switch
    {
        CswStatus.Passed => Passed, CswStatus.Failed => Failed, CswStatus.PhaseError => Phase, _ => Pending
    };
    public string Residue { get; } = c.Residue.ToString();
    public string Duration { get; } = c.DurationMs is { } d ? d.ToString("0.###") : "";
    public string Summary { get; } = c.Summary;
}

/// <summary>USB / SCSI tab: USBPcap live capture or offline pcap, BOT + SCSI decode.</summary>
public partial class UsbMonitorViewModel : ViewModelBase, IAsyncDisposable
{
    private const int UiCap = 20_000;
    private readonly UsbCaptureService _capture = new();
    private readonly MassStorageDecoder _msc = new();
    private readonly object _gate = new();
    private readonly List<UsbTransferRow> _pendingRaw = new();
    private readonly List<ScsiRow> _pendingScsi = new();
    private readonly List<ScsiCommand> _allScsi = new();
    private readonly DispatcherTimer _timer;
    private long _packets;

    public UsbMonitorViewModel()
    {
        _capture.PacketCaptured += OnPacket;
        _capture.Error += msg => Dispatcher.UIThread.Post(() => Message = msg);
        _capture.Stopped += () => Dispatcher.UIThread.Post(() =>
        {
            IsCapturing = false;
            StartCommand.NotifyCanExecuteChanged();
            StopCommand.NotifyCanExecuteChanged();
        });
        CmdPath = UsbCaptureService.DefaultCmdPath;
        Message = UsbCaptureService.CheckLivePrerequisites(CmdPath) ?? "USBPcap 就绪，选择接口后点「开始抓包」。";
        _ = RefreshInterfacesAsync();
        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
        _timer.Tick += (_, _) => Flush();
        _timer.Start();
    }

    public ObservableCollection<UsbTransferRow> Transfers { get; } = new();
    public ObservableCollection<ScsiRow> ScsiCommands { get; } = new();
    public ObservableCollection<string> Interfaces { get; } = new();
    public ObservableCollection<UsbTreeNode> DeviceTree { get; } = new();

    [ObservableProperty] private string _cmdPath = "";
    [ObservableProperty] private string? _selectedInterface;
    [ObservableProperty] private string? _deviceFilter;
    [ObservableProperty] [NotifyCanExecuteChangedFor(nameof(StartCommand), nameof(StopCommand))] private bool _isCapturing;
    [ObservableProperty] private string? _message;
    [ObservableProperty] private string _stats = "USB 包 0 | SCSI 命令 0";
    [ObservableProperty] private bool _onlyMassStorage;

    private int? DeviceAddress => int.TryParse(DeviceFilter?.Trim(), out var d) && d > 0 ? d : null;

    [RelayCommand]
    private async Task RefreshInterfacesAsync()
    {
        var list = await UsbCaptureService.ListInterfacesAsync(CmdPath);
        Interfaces.Clear();
        foreach (var i in list) Interfaces.Add(i);
        SelectedInterface ??= Interfaces.FirstOrDefault();
    }

    [RelayCommand(CanExecute = nameof(CanStart))]
    private void Start()
    {
        try
        {
            if (string.IsNullOrWhiteSpace(SelectedInterface)) { Message = "请选择 USBPcap 接口"; return; }
            _msc.Reset();
            _capture.StartLive(CmdPath, SelectedInterface!, DeviceAddress);
            IsCapturing = true;
            Message = $"正在抓包 {SelectedInterface}{(DeviceAddress is { } d ? $"（设备 {d}）" : "（全部设备）")}…";
        }
        catch (Exception ex) { Message = ex.Message; }
    }
    private bool CanStart() => !IsCapturing;

    [RelayCommand(CanExecute = nameof(IsCapturing))]
    private async Task StopAsync()
    {
        await _capture.StopAsync();
        IsCapturing = false;
        Message = "抓包已停止";
    }

    /// <summary>Offline analysis of a USBPcap .pcap/.pcapng file.</summary>
    public async Task LoadFileAsync(string path)
    {
        if (IsCapturing) await StopAsync();
        Clear();
        try
        {
            int n = await Task.Run(() => _capture.LoadFile(path));
            Flush();
            Message = $"已载入 {Path.GetFileName(path)}：{n} 个 USB 包，{_allScsi.Count} 条 SCSI 命令";
        }
        catch (Exception ex) { Message = $"打开失败：{ex.Message}"; }
    }

    [RelayCommand]
    private void Clear()
    {
        lock (_gate) { _pendingRaw.Clear(); _pendingScsi.Clear(); _allScsi.Clear(); }
        Transfers.Clear(); ScsiCommands.Clear(); DeviceTree.Clear();
        _msc.Reset(); _capture.ResetIndex();
        _packets = 0;
        UpdateStats();
    }

    [RelayCommand]
    private async Task ExportScsiCsvAsync()
    {
        try
        {
            List<ScsiCommand> snap;
            lock (_gate) snap = _allScsi.ToList();
            var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "PortMonitor");
            Directory.CreateDirectory(dir);
            var path = Path.Combine(dir, $"scsi-{DateTime.Now:yyyyMMdd-HHmmss}.csv");
            await Task.Run(() => ScsiCsvExporter.Write(path, snap));
            Message = $"已导出 {snap.Count} 条 SCSI 命令 → {path}";
        }
        catch (Exception ex) { Message = ex.Message; }
    }

    private void OnPacket(UsbPacket p)
    {
        if (DeviceAddress is { } d && p.Device != d) return;
        Interlocked.Increment(ref _packets);
        LearnDevice(p);
        var cmd = _msc.Push(p);
        lock (_gate)
        {
            if (!OnlyMassStorage || p.Transfer == UsbTransferType.Bulk)
                _pendingRaw.Add(new UsbTransferRow(p));
            if (cmd is not null) { _pendingScsi.Add(new ScsiRow(cmd)); _allScsi.Add(cmd); }
        }
    }

    private void Flush()
    {
        List<UsbTransferRow> raw; List<ScsiRow> scsi;
        lock (_gate)
        {
            if (_pendingRaw.Count == 0 && _pendingScsi.Count == 0) { UpdateStats(); return; }
            raw = _pendingRaw.ToList(); _pendingRaw.Clear();
            scsi = _pendingScsi.ToList(); _pendingScsi.Clear();
        }
        foreach (var r in raw) Transfers.Add(r);
        while (Transfers.Count > UiCap) Transfers.RemoveAt(0);
        foreach (var s in scsi) ScsiCommands.Add(s);
        while (ScsiCommands.Count > UiCap) ScsiCommands.RemoveAt(0);
        UpdateStats();
    }


    private void LearnDevice(UsbPacket p)
    {
        string busDev = $"{p.Bus}.{p.Device}";
        var root = DeviceTree.FirstOrDefault(n => n.Title.StartsWith(busDev, StringComparison.Ordinal));
        if (root is null)
        {
            root = new UsbTreeNode { Title = $"{busDev}  ({p.TransferLabel})" };
            // Must update collection on UI thread — LearnDevice may run on capture thread.
            Dispatcher.UIThread.Post(() =>
            {
                if (DeviceTree.All(n => !n.Title.StartsWith(busDev, StringComparison.Ordinal)))
                    DeviceTree.Add(root);
            });
            return;
        }
        string ep = $"EP{p.EndpointNumber} {p.DirectionLabel}";
        if (root.Children.All(c => c.Title != ep))
            Dispatcher.UIThread.Post(() =>
            {
                if (root.Children.All(c => c.Title != ep))
                    root.Children.Add(new UsbTreeNode { Title = ep });
            });
    }

    private void UpdateStats()
    {
        int fail = ScsiCommands.Count(r => r.Command.Status != CswStatus.Passed);
        Stats = $"USB 包 {Interlocked.Read(ref _packets)} | SCSI 命令 {ScsiCommands.Count}（失败 {fail}）";
    }

    public async ValueTask DisposeAsync()
    {
        _timer.Stop();
        await _capture.DisposeAsync();
    }
}
