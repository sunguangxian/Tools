using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using SscomAvalonia.ViewModels;
using SscomAvalonia.Views;

namespace SscomAvalonia;

public partial class App : Application
{
    public static string[] StartupArgs { get; set; } = Array.Empty<string>();

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
            var openExt = false;
            var args = StartupArgs;
            for (var i = 0; i < args.Length; i++)
            {
                if (args[i] == "--screenshot" && i + 1 < args.Length)
                    shot = args[++i];
                else if (args[i] == "--open-ext")
                    openExt = true;
            }

            if (shot != null)
                win.ArmScreenshot(shot, openExt);

            desktop.MainWindow = win;
        }

        base.OnFrameworkInitializationCompleted();
    }
}
