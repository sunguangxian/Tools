using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using SscomAvalonia.ViewModels;

namespace SscomAvalonia.Views;

public partial class MainWindow : Window
{
    private MainViewModel? Vm => DataContext as MainViewModel;
    private string? _screenshotPath;
    private bool _screenshotOpenExt;

    public MainWindow()
    {
        InitializeComponent();
        Opened += OnOpened;
        Closing += (_, _) => Vm?.OnClosing();
    }

    /// <summary>Enable one-shot layout capture after the window is shown.</summary>
    public void ArmScreenshot(string path, bool openExt)
    {
        _screenshotPath = path;
        _screenshotOpenExt = openExt;
    }

    private void OnOpened(object? sender, EventArgs e)
    {
        if (Vm == null) return;
        Vm.RequestStayOnTopChanged += () => Topmost = Vm.StayOnTop;
        Topmost = Vm.StayOnTop;
        Vm.RequestMessage += ShowMessage;
        Vm.RequestOpenFile += OpenFileAsync;
        Vm.RequestSaveFile += SaveFileAsync;

        Vm.PropertyChanged += (_, args) =>
        {
            if (args.PropertyName == nameof(MainViewModel.ExtensionVisible))
                ApplyExtensionLayout();
        };
        ApplyExtensionLayout();

        if (_screenshotPath != null)
        {
            // Force layout for capture; ignore saved ExtensionVisible.
            Vm.ExtensionVisible = _screenshotOpenExt;
            Dispatcher.UIThread.Post(async () =>
            {
                await Task.Delay(350);
                ApplyExtensionLayout();
                await Task.Delay(150);
                CaptureTo(_screenshotPath!);
                if (Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desk)
                    desk.Shutdown();
            }, DispatcherPriority.Background);
        }
    }

    private void CaptureTo(string path)
    {
        var size = new PixelSize(Math.Max(1, (int)Bounds.Width), Math.Max(1, (int)Bounds.Height));
        var dpi = 96.0;
        using var bmp = new RenderTargetBitmap(size, new Vector(dpi, dpi));
        bmp.Render(this);
        bmp.Save(path); // layout capture
    }

    private void ApplyExtensionLayout()
    {
        if (Vm == null) return;
        var grid = this.FindControl<Grid>("RecvGrid");
        if (grid == null || grid.ColumnDefinitions.Count < 3) return;
        if (Vm.ExtensionVisible)
        {
            grid.ColumnDefinitions[0].Width = new GridLength(0.47, GridUnitType.Star);
            grid.ColumnDefinitions[1].Width = new GridLength(4);
            grid.ColumnDefinitions[2].Width = new GridLength(0.53, GridUnitType.Star);
        }
        else
        {
            grid.ColumnDefinitions[0].Width = new GridLength(1, GridUnitType.Star);
            grid.ColumnDefinitions[1].Width = new GridLength(0);
            grid.ColumnDefinitions[2].Width = new GridLength(0);
        }
    }

    private async void ShowMessage(string title, string message)
    {
        var ok = new Button
        {
            Content = "确定",
            HorizontalAlignment = HorizontalAlignment.Right,
            MinWidth = 72
        };
        var dlg = new Window
        {
            Title = title,
            Width = 380,
            Height = 170,
            CanResize = false,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Content = new StackPanel
            {
                Margin = new Thickness(16),
                Spacing = 16,
                Children =
                {
                    new TextBlock
                    {
                        Text = message,
                        TextWrapping = TextWrapping.Wrap
                    },
                    ok
                }
            }
        };
        ok.Click += (_, _) => dlg.Close();
        await dlg.ShowDialog(this);
    }

    private async Task<string?> OpenFileAsync(string? title)
    {
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = title ?? "打开文件",
            AllowMultiple = false
        });
        return files.Count > 0 ? files[0].TryGetLocalPath() : null;
    }

    private async Task<string?> SaveFileAsync(string? title)
    {
        var file = await StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = title ?? "保存文件",
            SuggestedFileName = "recv.bin"
        });
        return file?.TryGetLocalPath();
    }

    private void OnExitClick(object? sender, RoutedEventArgs e) => Close();

    private void OnAboutClick(object? sender, RoutedEventArgs e)
    {
        ShowMessage("关于",
            "sscom-avalonia\nAvalonia ProfessionalDark（与端口监视/PortMonitor 同款）\n开源串口调试助手（布局参考 SSCOM V5.13）");
    }
}
