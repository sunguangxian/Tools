using System.Collections.ObjectModel;
using System.Text;
using Avalonia;
using Avalonia.Media;
using Avalonia.Input.Platform;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SerialTap.Core.Bridge;
using SerialTap.Core.Export;
using SerialTap.Core.Models;
using SerialTap.Core.Protocol;
using SerialTap.Core.Sessions;

namespace SerialTap.App.ViewModels;

public sealed record ModeChoice(MonitorMode Mode, string Display)
{
    public override string ToString() => Display;
}

public partial class MainViewModel : ViewModelBase, IAsyncDisposable
{
    private const int UiCap = 10_000;

    private readonly SessionManager _manager = new();
    private readonly RingBuffer _ring = new(maxEvents: 50_000, maxBytes: 50L * 1024 * 1024);
    private readonly ModbusRtuStreamParser _modbusRtu = new();
    private readonly ModbusAsciiStreamParser _modbusAscii = new();
    private readonly LineMerger _lines = new();
    private readonly AutosaveWriter _autosave = new();
    private readonly object _uiGate = new();
    private readonly List<LogEntryViewModel> _pendingLog = new();
    private readonly List<ModbusFrameViewModel> _pendingModbus = new();
    private readonly List<LineRowViewModel> _pendingLines = new();
    private readonly List<LogEntryViewModel> _allLog = new();
    private readonly List<ModbusFrameViewModel> _allModbus = new();
    private readonly List<LineRowViewModel> _allLines = new();
    private readonly StringBuilder _terminal = new();
    private readonly List<(SerialEvent Ev, string Kind)> _sessionRecs = new();
    private DispatcherTimer? _timer;
    private volatile bool _pause;
    private int _okCounter, _failCounter;
    private bool _disposed;
    private long _demoSeq;
    private DispatcherTimer? _demoTimer;

    public MainViewModel()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);

        _manager.DataCaptured += OnDataCaptured;
        _manager.ErrorOccurred += (_, e) => Dispatcher.UIThread.Post(() =>
        {
            LastError = $"[{e.Session.Name}] {e.Message}";
            RefreshSessions();
        });

        BaudRates = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600];
        ParityOptions = Enum.GetValues<ParityKind>();
        DataBitsOptions = [5, 6, 7, 8];
        StopBitsOptions = Enum.GetValues<StopBitsKind>();
        ModeChoices =
        [
            new ModeChoice(MonitorMode.Bridge, "桥接 A↔B"),
            new ModeChoice(MonitorMode.SinglePort, "单口监听")
        ];
        DirectionChoices =
        [
            new DirectionChoice(null, "全部方向"),
            new DirectionChoice(SerialDirection.AToB, "A→B"),
            new DirectionChoice(SerialDirection.BToA, "B→A"),
            new DirectionChoice(SerialDirection.Receive, "RX"),
            new DirectionChoice(SerialDirection.Transmit, "TX")
        ];
        RadixChoices =
        [
            new RadixChoice("HEX", "HEX"),
            new RadixChoice("DEC", "DEC"),
            new RadixChoice("OCT", "OCT"),
            new RadixChoice("BIN", "BIN")
        ];
        EncodingChoices =
        [
            new EncodingChoice(TextEncodingKind.Ascii, "ASCII"),
            new EncodingChoice(TextEncodingKind.Utf8, "UTF-8"),
            new EncodingChoice(TextEncodingKind.Gbk, "GBK"),
            new EncodingChoice(TextEncodingKind.Utf16Le, "UTF-16LE"),
            new EncodingChoice(TextEncodingKind.Utf16Be, "UTF-16BE"),
            new EncodingChoice(TextEncodingKind.Latin1, "Latin-1")
        ];
        ModbusModeChoices =
        [
            new ModbusModeChoice("Auto", "自动"),
            new ModbusModeChoice("Rtu", "仅 RTU"),
            new ModbusModeChoice("Ascii", "仅 ASCII")
        ];
        SelectedDirection = DirectionChoices[0];
        SelectedRadix = RadixChoices[0];
        SelectedEncoding = EncodingChoices[0];
        SelectedModbusMode = ModbusModeChoices[0];
        SessionFilters.Add(new FilterChoice(0, "全部会话"));
        SelectedSessionFilter = SessionFilters[0];

        NewModeChoice = ModeChoices[0];
        NewBaudRate = 9600;
        NewParity = ParityKind.None;
        NewDataBits = 8;
        NewStopBits = StopBitsKind.One;
        AutoScroll = true;
        ShowCrcErrors = true;
        StatusText = "就绪 — 专业暗色主题 · CommMonitor 风格布局";
        InjectHelpText =
            "进程注入监控在 Avalonia/.NET 版为界面占位。\n\n" +
            "完整能力（MinHook 钩 CreateFile/ReadFile/WriteFile/DeviceIoControl、按端口枚举占用进程）" +
            "已在纯 C++ 版实现：请将 SerialTapHook.dll 与主程序同目录，在 Windows 上使用 Native 版注入。\n\n" +
            "本界面保留菜单入口，便于后续通过 P/Invoke 对接同一 Hook DLL。" +
            "注意：注入可能被杀软拦截，需管理员权限，位数须与目标进程一致。";

        TreeRoots.Add(new TreeNode("串口会话", "", "#569CD6"));
        TreeRoots.Add(new TreeNode("USB 监视", "USBPcap", "#4EC9B0"));
        TreeRoots.Add(new TreeNode("注入监控", "占位", "#CE9178"));

        RefreshPorts();
        AddDemoSessions();
        RebuildTree();

        var docs = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "SerialTap", "autosave");
        _autosave.Configure(docs, AutosaveFormat.Csv, maxMb: 8, maxMinutes: 30);

        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        _timer.Tick += (_, _) => Tick();
        _timer.Start();

        if (Environment.GetEnvironmentVariable("SERIALTAP_DEMO") == "1" ||
            Environment.GetCommandLineArgs().Contains("--demo"))
            StartDemoTraffic();
    }

    public UsbMonitorViewModel Usb { get; } = new();

    public ObservableCollection<SessionViewModel> Sessions { get; } = new();
    public ObservableCollection<LogEntryViewModel> LogEntries { get; } = new();
    public ObservableCollection<ModbusFrameViewModel> ModbusFrames { get; } = new();
    public ObservableCollection<LineRowViewModel> LineEntries { get; } = new();
    public ObservableCollection<PortEventRow> PortEvents { get; } = new();
    public ObservableCollection<string> AvailablePorts { get; } = new();
    public ObservableCollection<FilterChoice> SessionFilters { get; } = new();
    public ObservableCollection<TreeNode> TreeRoots { get; } = new();

    public int[] BaudRates { get; }
    public ParityKind[] ParityOptions { get; }
    public int[] DataBitsOptions { get; }
    public StopBitsKind[] StopBitsOptions { get; }
    public ModeChoice[] ModeChoices { get; }
    public DirectionChoice[] DirectionChoices { get; }
    public RadixChoice[] RadixChoices { get; }
    public EncodingChoice[] EncodingChoices { get; }
    public ModbusModeChoice[] ModbusModeChoices { get; }

    [ObservableProperty] [NotifyPropertyChangedFor(nameof(NewIsBridge))] private ModeChoice? _newModeChoice;
    [ObservableProperty] private string? _newPortA;
    [ObservableProperty] private string? _newPortB;
    [ObservableProperty] private string? _newName;
    [ObservableProperty] private int _newBaudRate;
    [ObservableProperty] private ParityKind _newParity;
    [ObservableProperty] private int _newDataBits;
    [ObservableProperty] private StopBitsKind _newStopBits;
    public bool NewIsBridge => NewModeChoice?.Mode == MonitorMode.Bridge;

    [ObservableProperty] private SessionViewModel? _selectedSession;
    [ObservableProperty] private TreeNode? _selectedTreeNode;
    [ObservableProperty] private FilterChoice? _selectedSessionFilter;
    [ObservableProperty] private DirectionChoice? _selectedDirection;
    [ObservableProperty] private RadixChoice? _selectedRadix;
    [ObservableProperty] private EncodingChoice? _selectedEncoding;
    [ObservableProperty] private ModbusModeChoice? _selectedModbusMode;
    [ObservableProperty] private LogEntryViewModel? _selectedLog;
    [ObservableProperty] private bool _autoScroll;
    [ObservableProperty] private bool _isPaused;
    [ObservableProperty] private bool _showCrcErrors;
    [ObservableProperty] private bool _escapeControls;
    [ObservableProperty] private bool _autosaveEnabled;
    [ObservableProperty] private string _searchText = "";
    [ObservableProperty] private string _statusText = "";
    [ObservableProperty] private string _statsText = "";
    [ObservableProperty] private string _statsDetail = "";
    [ObservableProperty] private string? _lastError;
    [ObservableProperty] private string _detailHex = "";
    [ObservableProperty] private string _detailAscii = "";
    [ObservableProperty] private string _detailMeta = "未选中";
    [ObservableProperty] private string _terminalText = "";
    [ObservableProperty] private string _sendPayload = "01 03 00 00 00 0A";
    [ObservableProperty] private bool _sendAsText;
    [ObservableProperty] private bool _sendAppendCrc = true;
    [ObservableProperty] private bool _sendAppendCrLf;
    [ObservableProperty] private string _injectHelpText = "";

    partial void OnIsPausedChanged(bool value) => _pause = value;
    partial void OnSelectedSessionFilterChanged(FilterChoice? value) => RebuildVisible();
    partial void OnSelectedDirectionChanged(DirectionChoice? value) => RebuildVisible();
    partial void OnShowCrcErrorsChanged(bool value) => RebuildVisible();
    partial void OnSearchTextChanged(string value) => RebuildVisible();
    partial void OnEscapeControlsChanged(bool value) => RefreshDisplayFormats();
    partial void OnSelectedRadixChanged(RadixChoice? value) => RefreshDisplayFormats();
    partial void OnSelectedEncodingChanged(EncodingChoice? value) => RefreshDisplayFormats();
    partial void OnAutosaveEnabledChanged(bool value) => _autosave.Enabled = value;
    partial void OnSelectedLogChanged(LogEntryViewModel? value) => UpdateDetail(value);
    partial void OnSelectedTreeNodeChanged(TreeNode? value)
    {
        if (value?.Tag is SessionViewModel svm)
            SelectedSession = svm;
    }

    private TextEncodingKind CurrentEnc => SelectedEncoding?.Kind ?? TextEncodingKind.Ascii;
    private string CurrentRadix => SelectedRadix?.Id ?? "HEX";
    private string CurrentModbusMode => SelectedModbusMode?.Id ?? "Auto";

    [RelayCommand]
    private void RefreshPorts()
    {
        AvailablePorts.Clear();
        foreach (var n in SerialPortEnumerator.GetPortNames())
            AvailablePorts.Add(n);
        if (AvailablePorts.Count > 0)
        {
            NewPortA ??= AvailablePorts[0];
            if (AvailablePorts.Count > 1) NewPortB ??= AvailablePorts[1];
        }
        StatusText = AvailablePorts.Count == 0
            ? "未检测到串口（可手动输入，如 COM5）"
            : $"检测到 {AvailablePorts.Count} 个串口";
    }

    [RelayCommand]
    private void AddSession()
    {
        LastError = null;
        try
        {
            var mode = NewModeChoice?.Mode ?? MonitorMode.Bridge;
            var settings = new SerialPortSettings
            {
                BaudRate = NewBaudRate, Parity = NewParity, DataBits = NewDataBits, StopBits = NewStopBits
            };
            var name = string.IsNullOrWhiteSpace(NewName) ? null : NewName!.Trim();
            var s = _manager.Add(mode, NewPortA?.Trim() ?? "", NewPortB?.Trim(), settings, name);
            AddSessionVm(s);
            NewName = null;
            RebuildTree();
            StatusText = $"已添加会话 {s.Name}：{s.Description}";
            PortEvents.Insert(0, new PortEventRow(DateTimeOffset.Now, s.Name, $"会话创建 {s.Description} @ {s.Settings.BaudRate}"));
        }
        catch (Exception ex) { LastError = ex.Message; }
    }

    [RelayCommand]
    private async Task OpenSessionAsync(SessionViewModel? vm)
    {
        if (vm is null) return;
        LastError = null;
        try
        {
            _modbusRtu.SetBaudRate(vm.Session.Settings.BaudRate);
            await _manager.OpenAsync(vm.Id);
            StatusText = $"{vm.Name} 已打开：{vm.Description}";
            PortEvents.Insert(0, new PortEventRow(DateTimeOffset.Now, vm.Name,
                $"打开 {vm.Description}  DTR/RTS 轮询启用（用户态）"));
        }
        catch (Exception ex)
        {
            LastError = $"[{vm.Name}] 打开失败：{ex.Message}";
            if (vm.Session.Mode == MonitorMode.SinglePort && ex is UnauthorizedAccessException or IOException)
                LastError += "（端口可能被占用；单口模式无法旁路，请用桥接或注入）";
        }
        RefreshSessions();
        RebuildTree();
    }

    [RelayCommand]
    private async Task CloseSessionAsync(SessionViewModel? vm)
    {
        if (vm is null) return;
        await _manager.CloseAsync(vm.Id);
        StatusText = $"{vm.Name} 已关闭";
        PortEvents.Insert(0, new PortEventRow(DateTimeOffset.Now, vm.Name, "会话已关闭"));
        RefreshSessions();
        RebuildTree();
    }

    [RelayCommand] private Task OpenSelectedAsync() => OpenSessionAsync(SelectedSession);
    [RelayCommand] private Task CloseSelectedAsync() => CloseSessionAsync(SelectedSession);

    [RelayCommand]
    private async Task RemoveSessionAsync(SessionViewModel? vm)
    {
        if (vm is null) return;
        await _manager.RemoveAsync(vm.Id);
        Sessions.Remove(vm);
        var f = SessionFilters.FirstOrDefault(x => x.Id == vm.Id);
        if (f is not null)
        {
            if (SelectedSessionFilter == f) SelectedSessionFilter = SessionFilters[0];
            SessionFilters.Remove(f);
        }
        RebuildTree();
        StatusText = $"已移除会话 {vm.Name}";
    }

    [RelayCommand]
    private async Task OpenAllAsync()
    {
        foreach (var vm in Sessions.Where(s => !s.IsOpen).ToList())
            await OpenSessionAsync(vm);
    }

    [RelayCommand]
    private async Task CloseAllAsync()
    {
        await _manager.CloseAllAsync();
        FlushModbus(_modbusRtu.PollSilence(DateTimeOffset.Now.AddSeconds(1)));
        foreach (var line in _lines.FlushAll())
            EnqueueLine(line);
        RefreshSessions();
        RebuildTree();
        StatusText = "全部会话已关闭";
    }

    [RelayCommand]
    private void TogglePause()
    {
        IsPaused = !IsPaused;
        StatusText = IsPaused ? "已暂停刷新（仍在采集与解析）" : "继续刷新";
    }

    [RelayCommand]
    private void ClearLog()
    {
        lock (_uiGate) { _pendingLog.Clear(); _pendingModbus.Clear(); _pendingLines.Clear(); }
        _allLog.Clear(); _allModbus.Clear(); _allLines.Clear();
        LogEntries.Clear(); ModbusFrames.Clear(); LineEntries.Clear();
        _ring.Clear(); _modbusRtu.Reset(); _modbusAscii.Reset(); _lines.Reset();
        _terminal.Clear(); TerminalText = "";
        _sessionRecs.Clear();
        _okCounter = 0; _failCounter = 0;
        UpdateStats();
        StatusText = "日志已清空";
    }

    [RelayCommand] private Task ExportCsvAsync() => ExportAsync("csv", CsvExporter.Write);
    [RelayCommand] private Task ExportTxtAsync() => ExportAsync("txt", TxtExporter.Write);
    [RelayCommand] private Task ExportHtmlAsync() => ExportAsync("html", HtmlExporter.Write);

    private async Task ExportAsync(string ext, Action<string, IEnumerable<SerialEvent>> writer)
    {
        try
        {
            var snapshot = _ring.Snapshot();
            var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "SerialTap");
            Directory.CreateDirectory(dir);
            var path = Path.Combine(dir, $"serialtap-{DateTime.Now:yyyyMMdd-HHmmss}.{ext}");
            await Task.Run(() => writer(path, snapshot));
            StatusText = $"已导出 {snapshot.Count} 条 → {path}";
        }
        catch (Exception ex) { LastError = ex.Message; }
    }

    [RelayCommand]
    private async Task SaveSessionAsync()
    {
        try
        {
            var file = new SessionFile();
            foreach (var s in Sessions)
            {
                file.Sessions.Add(new SessionFileInfo
                {
                    Id = s.Id, Mode = (int)s.Session.Mode, Baud = s.Session.Settings.BaudRate,
                    DataBits = s.Session.Settings.DataBits, Parity = (int)s.Session.Settings.Parity,
                    Stop = (int)s.Session.Settings.StopBits, Name = s.Name,
                    PortA = s.Session.PortA, PortB = s.Session.PortB ?? ""
                });
            }
            foreach (var (ev, _) in _sessionRecs.TakeLast(20000))
            {
                file.Records.Add(new SessionFileRecord
                {
                    Kind = 0, TimestampTicks = ev.Timestamp.UtcTicks, SessionId = ev.SessionId,
                    Direction = (int)ev.Direction, Data = ev.Data
                });
            }
            var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "SerialTap");
            Directory.CreateDirectory(dir);
            var path = Path.Combine(dir, $"session-{DateTime.Now:yyyyMMdd-HHmmss}.stqs");
            await Task.Run(() => SessionFileIo.Save(path, file));
            StatusText = $"会话已保存 → {path}";
        }
        catch (Exception ex) { LastError = ex.Message; }
    }

    [RelayCommand]
    private async Task LoadSessionAsync()
    {
        // Headless-friendly: load newest .stqs in Documents/SerialTap if present.
        try
        {
            var dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "SerialTap");
            if (!Directory.Exists(dir)) { LastError = "没有可加载的会话目录"; return; }
            var latest = Directory.GetFiles(dir, "*.stqs").OrderByDescending(File.GetLastWriteTimeUtc).FirstOrDefault();
            if (latest is null) { LastError = "未找到 .stqs 会话文件"; return; }
            var file = await Task.Run(() => SessionFileIo.Load(latest));
            ClearLog();
            StatusText = $"已加载会话文件 {Path.GetFileName(latest)}（{file.Sessions.Count} 会话 / {file.Records.Count} 记录）— 记录仅作回放参考";
            foreach (var r in file.Records.Where(x => x.Kind == 0).Take(5000))
            {
                var sess = file.Sessions.FirstOrDefault(s => s.Id == r.SessionId);
                var ev = new SerialEvent
                {
                    Sequence = ++_demoSeq,
                    SessionId = r.SessionId,
                    SessionName = sess?.Name ?? $"#{r.SessionId}",
                    Timestamp = new DateTimeOffset(r.TimestampTicks, TimeSpan.Zero).ToLocalTime(),
                    Direction = (SerialDirection)r.Direction,
                    SourcePort = sess?.PortA ?? "",
                    DestPort = sess?.PortB ?? "",
                    Data = r.Data
                };
                Ingest(ev);
            }
        }
        catch (Exception ex) { LastError = ex.Message; }
    }

    [RelayCommand]
    private async Task CopySelectedAsync()
    {
        if (SelectedLog is null) return;
        var text = $"{SelectedLog.Timestamp}\t{SelectedLog.Direction}\t{SelectedLog.Hex}\t{SelectedLog.DisplayText}";
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime
            {
                MainWindow: { } win
            } && win.Clipboard is { } clip)
        {
            await clip.SetTextAsync(text);
            StatusText = "已复制选中行到剪贴板";
        }
        else
        {
            StatusText = "剪贴板不可用（已生成文本）: " + text[..Math.Min(60, text.Length)];
        }
    }

    [RelayCommand]
    private void ShowInjectStub()
    {
        StatusText = "注入监控：Avalonia 版为占位；完整 MinHook 见 SerialTapHook.dll（Native）";
        PortEvents.Insert(0, new PortEventRow(DateTimeOffset.Now, "注入",
            "占位：请选择进程或按端口查找占用进程（需 Windows + Hook DLL）"));
    }

    [RelayCommand]
    private void ShowInjectByPortStub()
    {
        StatusText = "按端口注入：占位 — 真机将枚举占用 COM 的进程后注入";
        PortEvents.Insert(0, new PortEventRow(DateTimeOffset.Now, "注入",
            "按端口注入占位（Native 版已实现句柄枚举）"));
    }

    [RelayCommand]
    private void About()
    {
        StatusText = "SerialTap Avalonia · 专业暗色 · 用户态桥接/USBPcap · 非 CommMonitor 克隆";
    }

    [RelayCommand]
    private void Exit()
    {
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime d)
            d.Shutdown();
    }

    [RelayCommand] private void FocusNewSession() => StatusText = "在左侧填写参数后点「添加会话」";
    [RelayCommand] private void FocusSend() => StatusText = "底部「发送」页可向会话注入 HEX/文本";
    [RelayCommand] private void GotoUsbTab() => StatusText = "请切换到「USB / SCSI」页签";

    [RelayCommand]
    private void Send()
    {
        if (SelectedSession is null || !SelectedSession.IsOpen)
        {
            LastError = "请先选中并打开一个会话再发送";
            return;
        }
        try
        {
            byte[] data;
            if (SendAsText)
                data = Encoding.UTF8.GetBytes(SendPayload ?? "");
            else
            {
                var parts = (SendPayload ?? "").Split([' ', '-', ',', '\t'], StringSplitOptions.RemoveEmptyEntries);
                data = parts.Select(p => Convert.ToByte(p.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? p[2..] : p, 16)).ToArray();
            }
            if (SendAppendCrc)
            {
                var crc = ModbusCrc16.Compute(data);
                var with = new byte[data.Length + 2];
                Buffer.BlockCopy(data, 0, with, 0, data.Length);
                with[^2] = (byte)(crc & 0xFF);
                with[^1] = (byte)(crc >> 8);
                data = with;
            }
            if (SendAppendCrLf)
                data = data.Concat(new byte[] { 0x0D, 0x0A }).ToArray();

            // Reflect as TX event (bridge write path may vary; demo-friendly).
            var ev = new SerialEvent
            {
                Sequence = Interlocked.Increment(ref _demoSeq),
                SessionId = SelectedSession.Id,
                SessionName = SelectedSession.Name,
                Timestamp = DateTimeOffset.Now,
                Direction = SelectedSession.IsBridge ? SerialDirection.AToB : SerialDirection.Transmit,
                SourcePort = SelectedSession.Session.PortA,
                DestPort = SelectedSession.Session.PortB ?? "",
                Data = data
            };
            Ingest(ev);
            StatusText = $"已记录发送 {data.Length} 字节（会话 {SelectedSession.Name}）";
        }
        catch (Exception ex) { LastError = "发送解析失败：" + ex.Message; }
    }

    private void OnDataCaptured(object? sender, SerialEvent ev) => Ingest(ev);

    private void Ingest(SerialEvent ev)
    {
        _ring.Add(ev);
        _autosave.Write(ev);
        _sessionRecs.Add((ev, "data"));
        if (_sessionRecs.Count > 40_000) _sessionRecs.RemoveRange(0, 10_000);

        var frames = new List<ModbusFrame>();
        var mode = CurrentModbusMode;
        if (mode is "Auto" or "Rtu") frames.AddRange(_modbusRtu.Push(ev));
        if (mode is "Auto" or "Ascii") frames.AddRange(_modbusAscii.Push(ev));
        var closed = _lines.Push(ev);

        lock (_uiGate)
        {
            _pendingLog.Add(new LogEntryViewModel(ev, CurrentEnc, EscapeControls, CurrentRadix));
            foreach (var f in frames) _pendingModbus.Add(new ModbusFrameViewModel(f));
            foreach (var line in closed)
                _pendingLines.Add(new LineRowViewModel(line, CurrentEnc, EscapeControls));
        }

        var text = TextDecoder.Decode(CurrentEnc, ev.Data, EscapeControls, keepNewlines: true);
        lock (_terminal)
        {
            _terminal.Append('[').Append(ev.DirectionLabel).Append("] ").Append(text);
            if (_terminal.Length > 200_000) _terminal.Remove(0, _terminal.Length - 150_000);
        }
    }

    private void EnqueueLine(MergedLine line)
    {
        lock (_uiGate)
            _pendingLines.Add(new LineRowViewModel(line, CurrentEnc, EscapeControls));
    }

    private void FlushModbus(IReadOnlyList<ModbusFrame> frames)
    {
        if (frames.Count == 0) return;
        lock (_uiGate)
            foreach (var f in frames) _pendingModbus.Add(new ModbusFrameViewModel(f));
    }

    private void Tick()
    {
        FlushModbus(_modbusRtu.PollSilence(DateTimeOffset.Now));
        foreach (var line in _lines.FlushIdle(DateTimeOffset.Now))
            EnqueueLine(line);

        if (!_pause)
        {
            List<LogEntryViewModel> logs;
            List<ModbusFrameViewModel> mods;
            List<LineRowViewModel> lines;
            lock (_uiGate)
            {
                logs = _pendingLog.ToList(); _pendingLog.Clear();
                mods = _pendingModbus.ToList(); _pendingModbus.Clear();
                lines = _pendingLines.ToList(); _pendingLines.Clear();
            }
            foreach (var m in mods)
                if (m.CrcOk) _okCounter++; else _failCounter++;
            Append(_allLog, LogEntries, logs, MatchLog);
            Append(_allModbus, ModbusFrames, mods, MatchModbus);
            Append(_allLines, LineEntries, lines, MatchLine);

            lock (_terminal)
                TerminalText = _terminal.ToString();
        }

        RefreshSessions();
        UpdateStats();
    }

    private static void Append<T>(List<T> all, ObservableCollection<T> visible, List<T> batch, Func<T, bool> match)
    {
        if (batch.Count == 0) return;
        all.AddRange(batch);
        if (all.Count > UiCap) all.RemoveRange(0, all.Count - UiCap);
        foreach (var item in batch)
            if (match(item)) visible.Add(item);
        while (visible.Count > UiCap) visible.RemoveAt(0);
    }

    private bool MatchCommon(int sessionId, SerialDirection dir)
    {
        var sf = SelectedSessionFilter?.Id ?? 0;
        if (sf != 0 && sf != sessionId) return false;
        var df = SelectedDirection?.Direction;
        return df is null || df == dir;
    }

    private bool MatchSearch(string hex, string text)
    {
        if (string.IsNullOrWhiteSpace(SearchText)) return true;
        var q = SearchText.Trim();
        return hex.Contains(q, StringComparison.OrdinalIgnoreCase) ||
               text.Contains(q, StringComparison.OrdinalIgnoreCase);
    }

    private bool MatchLog(LogEntryViewModel e) =>
        MatchCommon(e.SessionId, e.DirectionKind) && MatchSearch(e.Hex, e.DisplayText);
    private bool MatchModbus(ModbusFrameViewModel f) =>
        MatchCommon(f.SessionId, f.DirectionKind) && (ShowCrcErrors || f.CrcOk) &&
        MatchSearch(f.Hex, f.Summary);
    private bool MatchLine(LineRowViewModel e) =>
        MatchCommon(e.SessionId, e.DirectionKind) && MatchSearch(e.Hex, e.Text);

    private void RebuildVisible()
    {
        LogEntries.Clear();
        foreach (var e in _allLog) if (MatchLog(e)) LogEntries.Add(e);
        ModbusFrames.Clear();
        foreach (var f in _allModbus) if (MatchModbus(f)) ModbusFrames.Add(f);
        LineEntries.Clear();
        foreach (var e in _allLines) if (MatchLine(e)) LineEntries.Add(e);
    }

    private void RefreshDisplayFormats()
    {
        foreach (var e in _allLog) e.ApplyDisplay(CurrentEnc, EscapeControls, CurrentRadix);
        RebuildVisible();
    }

    private void UpdateDetail(LogEntryViewModel? e)
    {
        if (e is null)
        {
            DetailHex = ""; DetailAscii = ""; DetailMeta = "未选中";
            return;
        }
        DetailHex = e.DisplayData;
        DetailAscii = e.DisplayText;
        DetailMeta = $"#{e.Sequence}  {e.Timestamp}  {e.Session}  {e.Direction}  {e.Ports}  {e.Length} B";
    }

    private void RefreshSessions()
    {
        foreach (var s in Sessions) s.Refresh();
    }

    private void RebuildTree()
    {
        var serialRoot = TreeRoots[0];
        serialRoot.Children.Clear();
        foreach (var s in Sessions)
        {
            serialRoot.Children.Add(new TreeNode(s.Name, s.Description, s.IsOpen ? "#6A9955" : "#858585")
            {
                Tag = s
            });
        }
        if (TreeRoots.Count > 1)
        {
            TreeRoots[1].Subtitle = Usb.IsCapturing ? "抓包中" : "USBPcap";
            TreeRoots[1].DotColor = Color.Parse(Usb.IsCapturing ? "#6A9955" : "#4EC9B0");
        }
    }

    private void UpdateStats()
    {
        int open = Sessions.Count(s => s.IsOpen);
        StatsText = $"会话 {open}/{Sessions.Count} | 事件 {_ring.Count}（{Fmt(_ring.ByteCount)}）| Modbus OK {_okCounter}/FAIL {_failCounter} | USB {Usb.Stats}";
        var sb = new StringBuilder();
        sb.AppendLine($"运行会话：{open}/{Sessions.Count}");
        sb.AppendLine($"环形缓冲：{_ring.Count} 事件 / {Fmt(_ring.ByteCount)}");
        sb.AppendLine($"Modbus：OK {_okCounter}  FAIL {_failCounter}");
        sb.AppendLine($"行视图：{LineEntries.Count}  列表：{LogEntries.Count}");
        sb.AppendLine($"自动保存：{(AutosaveEnabled ? "开" : "关")}");
        sb.AppendLine(Usb.Stats);
        foreach (var s in Sessions)
            sb.AppendLine($"· {s.Name}: {s.StatusLabel}  {s.BytesLabel}");
        StatsDetail = sb.ToString();
    }

    private static string Fmt(long n) => n < 1024 ? $"{n} B"
        : n < 1024 * 1024 ? $"{n / 1024.0:F1} KB" : $"{n / 1048576.0:F2} MB";

    private void AddSessionVm(MonitorSession s)
    {
        var vm = new SessionViewModel(s);
        vm.Refresh();
        Sessions.Add(vm);
        SessionFilters.Add(new FilterChoice(s.Id, $"{s.Name} ({string.Join("/", s.Ports)})"));
        if (SelectedSession is null) SelectedSession = vm;
    }

    private void AddDemoSessions()
    {
        var p = AvailablePorts;
        string P(int i, string fallback) => p.Count > i ? p[i] : fallback;
        try
        {
            AddSessionVm(_manager.Add(MonitorMode.Bridge, P(0, "COM10"), P(1, "COM11"),
                new SerialPortSettings { BaudRate = 9600 }, "PLC 桥接"));
            AddSessionVm(_manager.Add(MonitorMode.SinglePort, P(2, "COM3"), null,
                new SerialPortSettings { BaudRate = 115200 }, "传感器"));
        }
        catch { /* demo only */ }
    }

    /// <summary>Synthetic traffic for screenshots when no real COM/USB available.</summary>
    public void StartDemoTraffic()
    {
        if (_demoTimer is not null) return;
        StatusText = "演示流量已启动（--demo）";
        PortEvents.Insert(0, new PortEventRow(DateTimeOffset.Now, "系统", "演示模式：注入合成串口/Modbus 流量"));
        int tick = 0;
        _demoTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(280) };
        _demoTimer.Tick += (_, _) =>
        {
            tick++;
            if (Sessions.Count == 0) return;
            var plc = Sessions[0];
            var sensor = Sessions.Count > 1 ? Sessions[1] : Sessions[0];
            // Modbus RTU request/response
            byte[] req = [0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B];
            byte[] rsp = [0x01, 0x03, 0x04, 0x00, 0x64, 0x00, (byte)(0x20 + (tick % 40)), 0x00, 0x00];
            var crc = ModbusCrc16.Compute(rsp.AsSpan(0, 7));
            rsp[^2] = (byte)(crc & 0xFF); rsp[^1] = (byte)(crc >> 8);
            Ingest(Make(plc, SerialDirection.AToB, "COM10", "COM11", req));
            Ingest(Make(plc, SerialDirection.BToA, "COM11", "COM10", rsp));
            // ASCII Modbus
            if (tick % 5 == 0)
            {
                var ascii = Encoding.ASCII.GetBytes(":01030000000AC\r\n");
                Ingest(Make(plc, SerialDirection.AToB, "COM10", "COM11", ascii));
            }
            // Sensor line
            var line = Encoding.ASCII.GetBytes($"T={20 + tick % 10}.{tick % 10}C H={40 + tick % 15}%\r\n");
            Ingest(Make(sensor, SerialDirection.Receive, sensor.Session.PortA, "", line));
            if (tick == 1)
                PortEvents.Insert(0, new PortEventRow(DateTimeOffset.Now, plc.Name, "波特率 9600 8N1 · DTR=1 RTS=1（演示）"));
        };
        _demoTimer.Start();

        SerialEvent Make(SessionViewModel s, SerialDirection d, string a, string b, byte[] data) => new()
        {
            Sequence = Interlocked.Increment(ref _demoSeq),
            SessionId = s.Id,
            SessionName = s.Name,
            Timestamp = DateTimeOffset.Now,
            Direction = d,
            SourcePort = a,
            DestPort = b,
            Data = data
        };
    }

    public async ValueTask DisposeAsync()
    {
        if (_disposed) return;
        _disposed = true;
        _timer?.Stop();
        _demoTimer?.Stop();
        _autosave.Dispose();
        await Usb.DisposeAsync();
        await _manager.DisposeAsync();
    }
}
