using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using PortMonitor.App.ViewModels;
using PortMonitor.App.Views;

namespace PortMonitor.App;

public partial class App : Application
{
    public static string[] StartupArgs { get; set; } = System.Array.Empty<string>();

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var win = new MainWindow
            {
                DataContext = new MainViewModel(),
            };

            string? shot = null;
            var args = StartupArgs.Length > 0 ? StartupArgs : System.Environment.GetCommandLineArgs();
            for (var i = 0; i < args.Length; i++)
            {
                if (args[i] == "--screenshot" && i + 1 < args.Length)
                    shot = args[++i];
            }
            if (shot != null)
                win.ArmScreenshot(shot);

            desktop.MainWindow = win;
        }

        base.OnFrameworkInitializationCompleted();
    }
}
