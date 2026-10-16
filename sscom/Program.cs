using Avalonia;
using System;

namespace SscomAvalonia;

sealed class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        App.StartupArgs = args;
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
#if DEBUG
            .WithDeveloperTools()
#endif
            .WithInterFont()
            .LogToTrace();
}
