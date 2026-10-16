using CommunityToolkit.Mvvm.ComponentModel;
using SerialTap.Core.Models;
using SerialTap.Core.Sessions;

namespace SerialTap.App.ViewModels;

/// <summary>Row in the session side panel.</summary>
public partial class SessionViewModel : ViewModelBase
{
    public SessionViewModel(MonitorSession session) => Session = session;

    public MonitorSession Session { get; }
    public int Id => Session.Id;
    public string Name => Session.Name;
    public string Description => Session.Description;
    public bool IsBridge => Session.Mode == MonitorMode.Bridge;

    [ObservableProperty] private bool _isOpen;
    [ObservableProperty] private string _statusLabel = "已关闭";
    [ObservableProperty] private string _bytesLabel = "";
    [ObservableProperty] private string? _error;

    public void Refresh()
    {
        IsOpen = Session.IsOpen;
        StatusLabel = IsOpen ? "● 运行中" : "○ 已关闭";
        BytesLabel = IsBridge
            ? $"A→B {Fmt(Session.BytesAToB)}  B→A {Fmt(Session.BytesBToA)}"
            : $"RX {Fmt(Session.BytesAToB)}";
        Error = Session.LastError;
    }

    private static string Fmt(long n) => n < 1024 ? $"{n} B"
        : n < 1024 * 1024 ? $"{n / 1024.0:F1} KB" : $"{n / 1048576.0:F2} MB";
}

/// <summary>Item for filter dropdowns. Id 0 = all.</summary>
public sealed record FilterChoice(int Id, string Display)
{
    public override string ToString() => Display;
}

public sealed record DirectionChoice(SerialDirection? Direction, string Display)
{
    public override string ToString() => Display;
}
