using System.Text;
using System.Text.Json;
using SscomAvalonia.Models;
using SscomAvalonia.Services;

static void Pass(string name, string note = "") => Console.WriteLine($"PASS|{name}|{note}");
static void Fail(string name, string note) => Console.WriteLine($"FAIL|{name}|{note}");
static void Block(string name, string note) => Console.WriteLine($"BLOCKED|{name}|{note}");

int fails = 0;

// --- HexUtils ---
{
    var hex = HexUtils.ToHexDisplay(new byte[] { 0x0A, 0xFF });
    if (hex == "0A FF") Pass("HexUtils.ToHexDisplay"); else { Fail("HexUtils.ToHexDisplay", hex); fails++; }

    var bytes = HexUtils.FromHexText("0a ff AB", out var ok);
    if (ok && bytes.Length == 3 && bytes[0] == 0x0A && bytes[2] == 0xAB) Pass("HexUtils.FromHexText");
    else { Fail("HexUtils.FromHexText", $"ok={ok} len={bytes.Length}"); fails++; }

    HexUtils.FromHexText("ZZ", out var bad);
    if (!bad) Pass("HexUtils.FromHexText invalid"); else { Fail("HexUtils.FromHexText invalid", "should fail"); fails++; }

    var ascii = HexUtils.ToAsciiDisplay(new byte[] { (byte)'A', 0x00, (byte)'\n' });
    if (ascii == "A.\n") Pass("HexUtils.ToAsciiDisplay"); else { Fail("HexUtils.ToAsciiDisplay", ascii); fails++; }
}

// --- SettingsStore ---
{
    var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "sscom-avalonia");
    var expected = Path.Combine(dir, "settings.json");
    var s = new AppSettings
    {
        PortName = "/tmp/sscom-verify-port",
        BaudRate = 57600,
        HexDisplay = true,
        ChecksumMode = 1,
        ExtensionVisible = true,
        SendDraft = "verify-draft",
        Presets = Enumerable.Range(0, 32).Select(i => i == 0 ? "p0" : "").ToList(),
        PresetHexFlags = Enumerable.Range(0, 32).Select(i => i == 0).ToList()
    };
    SettingsStore.Save(s);
    if (!File.Exists(expected)) { Fail("SettingsStore path", expected); fails++; }
    else
    {
        var homeConfig = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".config", "sscom-avalonia", "settings.json");
        var note = $"path={expected}; ~/.config exists={File.Exists(homeConfig)}";
        Pass("SettingsStore.Save", note);
        var loaded = SettingsStore.Load();
        if (loaded.BaudRate == 57600 && loaded.HexDisplay && loaded.SendDraft == "verify-draft" && loaded.Presets[0] == "p0")
            Pass("SettingsStore.Load roundtrip");
        else { Fail("SettingsStore.Load roundtrip", JsonSerializer.Serialize(loaded)); fails++; }
    }
}

// --- SerialService with socat PTY ---
string? ptyA = "/tmp/sscom-ptyA";
string? ptyB = "/tmp/sscom-ptyB";
var socat = System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
{
    FileName = "socat",
    Arguments = $"-d -d pty,raw,echo=0,link={ptyA} pty,raw,echo=0,link={ptyB}",
    RedirectStandardError = true,
    RedirectStandardOutput = true,
    UseShellExecute = false
});
Thread.Sleep(400);

try
{
    // Open fail path
    using (var bad = new SerialService())
    {
        if (!bad.Open("/dev/ttyS0", 115200, 8, 1, 0, 0, false, false, out var err))
            Pass("SerialService.Open fail UI path", err);
        else { Fail("SerialService.Open fail UI path", "unexpected success"); fails++; }
    }

    using var svc = new SerialService();
    var received = new List<byte>();
    var got = new ManualResetEventSlim(false);
    svc.DataReceived += d => { lock (received) { received.AddRange(d); } got.Set(); };

    if (!svc.Open(ptyA!, 115200, 8, 1, 0, 0, false, false, out var openErr))
    {
        Fail("SerialService.Open PTY", openErr);
        fails++;
    }
    else
    {
        Pass("SerialService.Open PTY", ptyA);
        // write from peer B
        using var peer = new System.IO.Ports.SerialPort(ptyB!) { BaudRate = 115200, ReadTimeout = 1000, WriteTimeout = 1000 };
        peer.Open();
        var msg = Encoding.ASCII.GetBytes("RX-TEST-123");
        peer.Write(msg, 0, msg.Length);
        if (!got.Wait(2000))
        {
            // DataReceived event sometimes flaky on Linux; poll once
            Thread.Sleep(100);
            Fail("SerialService.DataReceived", "timeout waiting for event (Linux DataReceived may be flaky)");
            fails++;
        }
        else
        {
            string gotText;
            lock (received) gotText = Encoding.ASCII.GetString(received.ToArray());
            if (gotText.Contains("RX-TEST-123")) Pass("SerialService.DataReceived", gotText);
            else { Fail("SerialService.DataReceived", gotText); fails++; }
        }

        var tx = Encoding.ASCII.GetBytes("TX-HELLO");
        var n = svc.Write(tx);
        if (n == tx.Length)
        {
            Thread.Sleep(50);
            var buf = new byte[64];
            var read = peer.Read(buf, 0, buf.Length);
            var t = Encoding.ASCII.GetString(buf, 0, read);
            if (t.Contains("TX-HELLO")) Pass("SerialService.Write", t);
            else { Fail("SerialService.Write", t); fails++; }
        }
        else { Fail("SerialService.Write", $"n={n}"); fails++; }

        try
        {
            svc.SetRts(true);
            svc.SetDtr(true);
            Pass("SerialService.RTS/DTR setters", "setters called (PTY may ignore)");
        }
        catch (Exception ex)
        {
            // Should not throw after SerialService harden
            Fail("SerialService.RTS/DTR setters", ex.Message);
            fails++;
        }
        svc.Close();
        if (!svc.IsOpen) Pass("SerialService.Close"); else { Fail("SerialService.Close", "still open"); fails++; }
    }
}
finally
{
    try { if (socat != null && !socat.HasExited) { socat.Kill(); socat.WaitForExit(1000); } } catch { }
}

Console.WriteLine($"SUMMARY|fails={fails}");
return fails == 0 ? 0 : 1;
