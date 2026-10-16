using System.Collections.Specialized;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using PortMonitor.App.ViewModels;

namespace PortMonitor.App.Views;

public partial class MainWindow : Window
{
    private INotifyCollectionChanged? _logSub;
    private INotifyCollectionChanged? _modbusSub;
    private INotifyCollectionChanged? _lineSub;

    public MainWindow()
    {
        InitializeComponent();
        DataContextChanged += OnDataContextChanged;
        Opened += async (_, _) => await HandleStartupArgsAsync();
        Closed += async (_, _) =>
        {
            if (DataContext is MainViewModel vm)
                await vm.DisposeAsync();
        };
    }

    private void OnDataContextChanged(object? sender, EventArgs e)
    {
        void Unsub(ref INotifyCollectionChanged? sub, NotifyCollectionChangedEventHandler h)
        {
            if (sub is not null) sub.CollectionChanged -= h;
            sub = null;
        }
        Unsub(ref _logSub, OnLogChanged);
        Unsub(ref _modbusSub, OnModbusChanged);
        Unsub(ref _lineSub, OnLineChanged);

        if (DataContext is MainViewModel vm)
        {
            _logSub = vm.LogEntries; _logSub.CollectionChanged += OnLogChanged;
            _modbusSub = vm.ModbusFrames; _modbusSub.CollectionChanged += OnModbusChanged;
            _lineSub = vm.LineEntries; _lineSub.CollectionChanged += OnLineChanged;
        }
    }

    private void OnLogChanged(object? sender, NotifyCollectionChangedEventArgs e) =>
        AutoScroll(LogList, e);

    private void OnModbusChanged(object? sender, NotifyCollectionChangedEventArgs e) =>
        AutoScroll(ModbusList, e);

    private void OnLineChanged(object? sender, NotifyCollectionChangedEventArgs e) =>
        AutoScroll(LineList, e);

    private void AutoScroll(ListBox? list, NotifyCollectionChangedEventArgs e)
    {
        if (list is null || DataContext is not MainViewModel vm || !vm.AutoScroll) return;
        if (e.Action is not (NotifyCollectionChangedAction.Add or NotifyCollectionChangedAction.Reset))
            return;
        Dispatcher.UIThread.Post(() =>
        {
            if (list.ItemCount > 0)
                list.ScrollIntoView(list.ItemCount - 1);
        }, DispatcherPriority.Background);
    }

    private async Task HandleStartupArgsAsync()
    {
        var args = Environment.GetCommandLineArgs();
        if (DataContext is not MainViewModel vm) return;

        int i = Array.IndexOf(args, "--open-pcap");
        if (i >= 0 && i + 1 < args.Length)
        {
            MainTabs.SelectedIndex = 4; // USB / SCSI
            await vm.Usb.LoadFileAsync(args[i + 1]);
        }

        
        int t = Array.IndexOf(args, "--tab");
        if (t >= 0 && t + 1 < args.Length && int.TryParse(args[t + 1], out var tab) && tab >= 0 && tab < MainTabs.ItemCount)
            MainTabs.SelectedIndex = tab;

        if (args.Contains("--demo") || Environment.GetEnvironmentVariable("SERIALTAP_DEMO") == "1")
            vm.StartDemoTraffic();
    }

    private async void OnOpenPcapClick(object? sender, RoutedEventArgs e)
    {
        if (DataContext is not MainViewModel vm) return;
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "打开 USBPcap 抓包文件",
            AllowMultiple = false,
            FileTypeFilter =
            [
                new FilePickerFileType("USB 抓包 (*.pcap, *.pcapng)") { Patterns = ["*.pcap", "*.pcapng"] },
                new FilePickerFileType("所有文件") { Patterns = ["*"] }
            ]
        });
        var path = files.FirstOrDefault()?.TryGetLocalPath();
        if (path is not null)
        {
            MainTabs.SelectedIndex = 4;
            await vm.Usb.LoadFileAsync(path);
        }
    }
}
