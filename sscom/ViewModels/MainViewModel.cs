using System.Collections.ObjectModel;
using System.Text;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SscomAvalonia.Models;
using SscomAvalonia.Services;

namespace SscomAvalonia.ViewModels;

public partial class MainViewModel : ViewModelBase, IDisposable
{
    private readonly SerialService _serial = new();
    private readonly DispatcherTimer _autoTimer;
    private FileStream? _saveFile;
    private FileStream? _recvFile;
    private bool _stopFlag;
    private long _rxBytes;
    private long _txBytes;
    private StringBuilder _receiveBuffer = new();

    public ObservableCollection<string> Ports { get; } = new();
    public ObservableCollection<string> BaudRates { get; } = new(
        new[] { "1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600" });
    public ObservableCollection<string> DataBitsOptions { get; } = new(new[] { "5", "6", "7", "8" });
    public ObservableCollection<string> StopBitsOptions { get; } = new(new[] { "1", "1.5", "2" });
    public ObservableCollection<string> ParityOptions { get; } = new(new[] { "None", "Odd", "Even", "Mark", "Space" });
    public ObservableCollection<string> FlowOptions { get; } = new(new[] { "None", "Hardware", "Software" });
    public ObservableCollection<string> ChecksumOptions { get; } = new(new[] { "None", "XOR", "Sum8" });
    public ObservableCollection<PresetItemViewModel> Presets { get; } = new();

    [ObservableProperty] private string? _selectedPort;
    [ObservableProperty] private string _selectedBaud = "115200";
    [ObservableProperty] private string _selectedDataBits = "8";
    [ObservableProperty] private string _selectedStopBits = "1";
    [ObservableProperty] private string _selectedParity = "None";
    [ObservableProperty] private string _selectedFlow = "None";
    [ObservableProperty] private string _selectedChecksum = "None";

    [ObservableProperty] private bool _rts;
    [ObservableProperty] private bool _dtr;
    [ObservableProperty] private bool _hexDisplay;
    [ObservableProperty] private bool _hexSend;
    [ObservableProperty] private bool _showTimestamp;
    [ObservableProperty] private bool _appendNewline;
    [ObservableProperty] private bool _autoSend;
    [ObservableProperty] private int _autoSendIntervalMs = 1000;
    [ObservableProperty] private int _packetTimeoutMs = 20;
    [ObservableProperty] private bool _saveData;
    [ObservableProperty] private bool _recvToFile;
    [ObservableProperty] private bool _stayOnTop;
    [ObservableProperty] private bool _saveParamsOnExit = true;
    [ObservableProperty] private bool _englishLabels;
    [ObservableProperty] private bool _extensionVisible;
    [ObservableProperty] private bool _isConnected;
    [ObservableProperty] private bool _moreSerialVisible;

    [ObservableProperty] private string _receiveText = "";
    [ObservableProperty] private string _sendText = "abcdefg";
    [ObservableProperty] private string _statusLeft = "sscom-avalonia";
    [ObservableProperty] private string _statusCounts = "S:0 R:0";
    [ObservableProperty] private string _statusInfo = "未打开";
    [ObservableProperty] private string _openCloseLabel = "打开串口";
    [ObservableProperty] private string _extButtonLabel = "扩展 >>";
    [ObservableProperty] private string _sendButtonLabel = "发送";
    [ObservableProperty] private string _ledColor = "#C42B1C";
    [ObservableProperty] private string _filePath = "";
    [ObservableProperty] private int _csFrom = 1;

    public event Action? RequestStayOnTopChanged;
    public event Action<string, string>? RequestMessage;
    public event Func<string?, Task<string?>>? RequestOpenFile;
    public event Func<string?, Task<string?>>? RequestSaveFile;

    public MainViewModel()
    {
        for (int i = 0; i < SettingsStore.PresetCount; i++)
            Presets.Add(new PresetItemViewModel(i, OnPresetSend));

        _serial.DataReceived += OnSerialData;
        _serial.ErrorOccurred += msg =>
            Dispatcher.UIThread.Post(() => RequestMessage?.Invoke("串口错误", msg));

        _autoTimer = new DispatcherTimer();
        _autoTimer.Tick += (_, _) => Send();

        LoadSettings();
        RefreshPorts();
        ShowWelcomeText();
        UpdateCounts();
        UpdateConnectedUi(false);
    }

    partial void OnAutoSendChanged(bool value)
    {
        if (value)
        {
            if (!IsConnected)
            {
                AutoSend = false;
                RequestMessage?.Invoke("提示", "请先打开串口");
                return;
            }
            _autoTimer.Interval = TimeSpan.FromMilliseconds(Math.Max(10, AutoSendIntervalMs));
            _autoTimer.Start();
        }
        else
        {
            _autoTimer.Stop();
        }
    }

    partial void OnStayOnTopChanged(bool value) => RequestStayOnTopChanged?.Invoke();

    partial void OnEnglishLabelsChanged(bool value)
    {
        SendButtonLabel = value ? "Send" : "发送";
        OpenCloseLabel = IsConnected
            ? (value ? "Close" : "关闭串口")
            : (value ? "Open" : "打开串口");
    }

    partial void OnExtensionVisibleChanged(bool value)
    {
        ExtButtonLabel = value ? "隐藏" : "扩展 >>";
    }

    partial void OnRtsChanged(bool value)
    {
        if (IsConnected) _serial.SetRts(value);
    }

    partial void OnDtrChanged(bool value)
    {
        if (IsConnected) _serial.SetDtr(value);
    }

    partial void OnAutoSendIntervalMsChanged(int value)
    {
        if (_autoTimer.IsEnabled)
            _autoTimer.Interval = TimeSpan.FromMilliseconds(Math.Max(10, value));
    }

    [RelayCommand]
    private void RefreshPorts()
    {
        var list = SerialService.AvailablePorts().ToList();
        // Always show some placeholders on Linux when none present
        if (list.Count == 0)
        {
            // Linux placeholders with /dev prefix (Open will still fail without device)
            list.Add("/dev/ttyS0");
            list.Add("/dev/ttyUSB0");
            list.Add("/dev/ttyACM0");
        }
        var keep = SelectedPort;
        Ports.Clear();
        foreach (var p in list) Ports.Add(p);
        if (!string.IsNullOrEmpty(keep) && Ports.Contains(keep))
            SelectedPort = keep;
        else if (Ports.Count > 0)
            SelectedPort = Ports[0];
    }

    [RelayCommand]
    private void ToggleConnect()
    {
        if (IsConnected)
        {
            _serial.Close();
            _autoTimer.Stop();
            AutoSend = false;
            UpdateConnectedUi(false);
            return;
        }

        if (string.IsNullOrWhiteSpace(SelectedPort))
        {
            RequestMessage?.Invoke("提示", "请选择端口号");
            return;
        }

        if (!int.TryParse(SelectedBaud, out var baud)) baud = 115200;
        if (!int.TryParse(SelectedDataBits, out var dataBits)) dataBits = 8;
        var stopBits = SelectedStopBits switch { "1.5" => 15, "2" => 2, _ => 1 };
        var parity = SelectedParity switch
        {
            "Odd" => 1, "Even" => 2, "Mark" => 3, "Space" => 4, _ => 0
        };
        var flow = SelectedFlow switch { "Hardware" => 1, "Software" => 2, _ => 0 };

        if (!_serial.Open(SelectedPort, baud, dataBits, stopBits, parity, flow, Dtr, Rts, out var err))
        {
            RequestMessage?.Invoke("打开失败", err);
            return;
        }
        UpdateConnectedUi(true);
    }

    [RelayCommand]
    private void Send()
    {
        if (!IsConnected)
        {
            RequestMessage?.Invoke("提示", "请先打开串口");
            return;
        }
        _stopFlag = false;
        var payload = ComposeSendPayload(SendText, HexSend, out var ok);
        if (!ok)
        {
            RequestMessage?.Invoke("HEX错误", "HEX 格式不正确");
            return;
        }
        if (Transmit(payload) < 0)
            RequestMessage?.Invoke("发送失败", "写入失败");
    }

    [RelayCommand]
    private void ClearReceive()
    {
        ReceiveText = "";
        _receiveBuffer.Clear();
        _rxBytes = 0;
        UpdateCounts();
        ShowWelcomeText();
    }

    [RelayCommand]
    private void ClearSend() => SendText = "";

    [RelayCommand]
    private void Stop()
    {
        _stopFlag = true;
        _autoTimer.Stop();
        AutoSend = false;
    }

    [RelayCommand]
    private void ToggleExtension() => ExtensionVisible = !ExtensionVisible;

    [RelayCommand]
    private void ToggleMoreSerial() => MoreSerialVisible = !MoreSerialVisible;

    [RelayCommand]
    private void SaveParams()
    {
        PersistSettings();
        RequestMessage?.Invoke("提示", "参数已保存");
    }

    [RelayCommand]
    private async Task OpenFileToSend()
    {
        var path = RequestOpenFile != null ? await RequestOpenFile("选择要载入发送区的文件") : null;
        if (string.IsNullOrEmpty(path) || !File.Exists(path)) return;
        try
        {
            SendText = await File.ReadAllTextAsync(path);
            FilePath = path;
        }
        catch (Exception ex)
        {
            RequestMessage?.Invoke("打开失败", ex.Message);
        }
    }

    [RelayCommand]
    private async Task SendFile()
    {
        if (!IsConnected)
        {
            RequestMessage?.Invoke("提示", "请先打开串口");
            return;
        }
        var path = FilePath;
        if (string.IsNullOrEmpty(path) || !File.Exists(path))
        {
            path = RequestOpenFile != null ? await RequestOpenFile("选择要发送的文件") : null;
            if (string.IsNullOrEmpty(path)) return;
            FilePath = path;
        }
        try
        {
            _stopFlag = false;
            var data = await File.ReadAllBytesAsync(path);
            Transmit(data);
        }
        catch (Exception ex)
        {
            RequestMessage?.Invoke("发送文件失败", ex.Message);
        }
    }

    partial void OnRecvToFileChanged(bool value)
    {
        _ = HandleRecvToFileChanged(value);
    }

    private async Task HandleRecvToFileChanged(bool on)
    {
        if (!on)
        {
            _recvFile?.Dispose();
            _recvFile = null;
            return;
        }
        var path = RequestSaveFile != null ? await RequestSaveFile("接收数据到文件") : null;
        if (string.IsNullOrEmpty(path))
        {
            RecvToFile = false;
            return;
        }
        try
        {
            _recvFile = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.Read);
        }
        catch (Exception ex)
        {
            RecvToFile = false;
            RequestMessage?.Invoke("保存失败", ex.Message);
        }
    }

    partial void OnSaveDataChanged(bool value)
    {
        _ = HandleSaveDataChanged(value);
    }

    private async Task HandleSaveDataChanged(bool on)
    {
        if (!on)
        {
            _saveFile?.Dispose();
            _saveFile = null;
            return;
        }
        var path = RequestSaveFile != null ? await RequestSaveFile("保存接收数据") : null;
        if (string.IsNullOrEmpty(path))
        {
            SaveData = false;
            return;
        }
        try
        {
            _saveFile = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.Read);
        }
        catch (Exception ex)
        {
            SaveData = false;
            RequestMessage?.Invoke("保存失败", ex.Message);
        }
    }

    private void OnPresetSend(PresetItemViewModel item)
    {
        if (!IsConnected)
        {
            RequestMessage?.Invoke("提示", "请先打开串口");
            return;
        }
        _stopFlag = false;
        var payload = ComposeSendPayload(item.Text, item.Hex, out var ok);
        if (!ok)
        {
            RequestMessage?.Invoke("HEX错误", "HEX 格式不正确");
            return;
        }
        Transmit(payload);
    }

    private void OnSerialData(byte[] data)
    {
        Dispatcher.UIThread.Post(() => AppendReceive(data));
    }

    private void AppendReceive(byte[] data)
    {
        if (data.Length == 0) return;
        _rxBytes += data.Length;
        string chunk = HexDisplay ? HexUtils.ToHexDisplay(data) + " " : HexUtils.ToAsciiDisplay(data);
        if (ShowTimestamp)
        {
            var ts = DateTime.Now.ToString("[HH:mm:ss.fff] ");
            _receiveBuffer.AppendLine(ts + chunk);
        }
        else
        {
            _receiveBuffer.Append(chunk);
        }
        ReceiveText = _receiveBuffer.ToString();
        if (_saveFile != null)
        {
            try { _saveFile.Write(data, 0, data.Length); _saveFile.Flush(); }
            catch { /* ignore */ }
        }
        if (_recvFile != null)
        {
            try { _recvFile.Write(data, 0, data.Length); _recvFile.Flush(); }
            catch { /* ignore */ }
        }
        UpdateCounts();
    }

    private byte[] ComposeSendPayload(string text, bool asHex, out bool ok)
    {
        byte[] payload;
        if (asHex)
        {
            payload = HexUtils.FromHexText(text, out ok);
            if (!ok) return Array.Empty<byte>();
        }
        else
        {
            payload = Encoding.UTF8.GetBytes(text ?? "");
            ok = true;
        }
        if (AppendNewline)
        {
            var withNl = new byte[payload.Length + 2];
            Buffer.BlockCopy(payload, 0, withNl, 0, payload.Length);
            withNl[^2] = (byte)'\r';
            withNl[^1] = (byte)'\n';
            payload = withNl;
        }
        return MaybeAppendChecksum(payload);
    }

    private byte[] MaybeAppendChecksum(byte[] data)
    {
        var mode = SelectedChecksum switch { "XOR" => 1, "Sum8" => 2, _ => 0 };
        if (mode == 0 || data.Length == 0) return data;
        // "第 n 字节至末尾" — 1-based start index into payload
        var from = Math.Clamp(CsFrom, 1, data.Length) - 1;
        byte cs = 0;
        if (mode == 1)
        {
            for (int i = from; i < data.Length; i++) cs ^= data[i];
        }
        else
        {
            int s = 0;
            for (int i = from; i < data.Length; i++) s = (s + data[i]) & 0xFF;
            cs = (byte)s;
        }
        var outb = new byte[data.Length + 1];
        Buffer.BlockCopy(data, 0, outb, 0, data.Length);
        outb[^1] = cs;
        return outb;
    }

    private int Transmit(byte[] data)
    {
        if (data.Length == 0 || _stopFlag) return 0;
        if (!_serial.IsOpen) return -1;
        var n = _serial.Write(data);
        if (n > 0)
        {
            _txBytes += n;
            UpdateCounts();
        }
        return n;
    }

    private void UpdateConnectedUi(bool connected)
    {
        IsConnected = connected;
        LedColor = connected ? "#2E7D32" : "#C42B1C";
        OpenCloseLabel = connected
            ? (EnglishLabels ? "Close" : "关闭串口")
            : (EnglishLabels ? "Open" : "打开串口");
        StatusInfo = connected
            ? $"{SelectedPort} 已打开  {SelectedBaud}bps, {SelectedDataBits}, {SelectedStopBits}, {SelectedParity}"
            : "未打开";
    }

    private void UpdateCounts()
    {
        StatusCounts = $"S:{_txBytes} R:{_rxBytes}";
    }

    private void ShowWelcomeText()
    {
        const string welcome =
            "欢迎使用 sscom-avalonia（开源串口调试助手）\n" +
            "界面布局参考 SSCOM V5.13，主题为 ProfessionalDark（PortMonitor/端口监视同款 VS 深色）。仅支持串口。\n" +
            "\n" +
            "使用提示：\n" +
            "1. 在「端口号」选择串口，设置波特率后点「打开串口」；\n" +
            "2. 勾选 HEX显示 / HEX发送 / 定时发送 / 加回车换行等选项；\n" +
            "3. 点「扩展 >>」在接收区内分栏显示多字符串（窗口大小不变，按钮变为「隐藏」）；\n" +
            "4. 更多串口参数请点「更多串口设置」。\n" +
            "\n" +
            "本软件为原创开源实现。\n";
        ReceiveText = welcome;
        _receiveBuffer = new StringBuilder(welcome);
    }

    private void LoadSettings()
    {
        var s = SettingsStore.Load();
        SelectedBaud = s.BaudRate.ToString();
        SelectedDataBits = s.DataBits.ToString();
        SelectedStopBits = s.StopBits switch { 15 => "1.5", 2 => "2", _ => "1" };
        SelectedParity = s.Parity switch
        {
            1 => "Odd", 2 => "Even", 3 => "Mark", 4 => "Space", _ => "None"
        };
        SelectedFlow = s.FlowControl switch { 1 => "Hardware", 2 => "Software", _ => "None" };
        SelectedChecksum = s.ChecksumMode switch { 1 => "XOR", 2 => "Sum8", _ => "None" };
        Rts = s.Rts;
        Dtr = s.Dtr;
        HexDisplay = s.HexDisplay;
        HexSend = s.HexSend;
        ShowTimestamp = s.ShowTimestamp;
        AppendNewline = s.AppendNewline;
        AutoSendIntervalMs = s.AutoSendIntervalMs;
        PacketTimeoutMs = s.PacketTimeoutMs;
        StayOnTop = s.StayOnTop;
        SaveParamsOnExit = s.SaveParamsOnExit;
        ExtensionVisible = s.ExtensionVisible;
        if (!string.IsNullOrEmpty(s.SendDraft)) SendText = s.SendDraft!;
        if (!string.IsNullOrEmpty(s.PortName)) SelectedPort = s.PortName;
        for (int i = 0; i < Presets.Count; i++)
        {
            Presets[i].Text = i < s.Presets.Count ? s.Presets[i] : "";
            Presets[i].Hex = i < s.PresetHexFlags.Count && s.PresetHexFlags[i];
        }
    }

    public void PersistSettings()
    {
        if (!int.TryParse(SelectedBaud, out var baud)) baud = 115200;
        if (!int.TryParse(SelectedDataBits, out var dataBits)) dataBits = 8;
        var s = new AppSettings
        {
            PortName = SelectedPort ?? "",
            BaudRate = baud,
            DataBits = dataBits,
            StopBits = SelectedStopBits switch { "1.5" => 15, "2" => 2, _ => 1 },
            Parity = SelectedParity switch
            {
                "Odd" => 1, "Even" => 2, "Mark" => 3, "Space" => 4, _ => 0
            },
            FlowControl = SelectedFlow switch { "Hardware" => 1, "Software" => 2, _ => 0 },
            Dtr = Dtr,
            Rts = Rts,
            HexDisplay = HexDisplay,
            HexSend = HexSend,
            ShowTimestamp = ShowTimestamp,
            AppendNewline = AppendNewline,
            AutoSend = false,
            AutoSendIntervalMs = AutoSendIntervalMs,
            PacketTimeoutMs = PacketTimeoutMs,
            ChecksumMode = SelectedChecksum switch { "XOR" => 1, "Sum8" => 2, _ => 0 },
            StayOnTop = StayOnTop,
            SaveParamsOnExit = SaveParamsOnExit,
            ExtensionVisible = ExtensionVisible,
            SendDraft = SendText,
            Presets = Presets.Select(p => p.Text).ToList(),
            PresetHexFlags = Presets.Select(p => p.Hex).ToList()
        };
        SettingsStore.Save(s);
    }

    public void OnClosing()
    {
        if (SaveParamsOnExit) PersistSettings();
        _serial.Close();
        _saveFile?.Dispose();
    }

    public void Dispose()
    {
        OnClosing();
        _serial.Dispose();
        _autoTimer.Stop();
    }
}
