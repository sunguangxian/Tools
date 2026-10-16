using SerialTap.Core.Bridge;
using SerialTap.Core.Models;

namespace SerialTap.Core.Sessions;

/// <summary>One monitoring session: a bridge (A↔B) or a single-port listener.</summary>
public sealed class MonitorSession
{
    internal MonitorSession(int id, string name, MonitorMode mode, string portA, string? portB, SerialPortSettings settings)
    {
        Id = id;
        Name = name;
        Mode = mode;
        PortA = portA;
        PortB = portB;
        Settings = settings;
        Bridge = new SerialBridge(id, name);
    }

    public int Id { get; }
    public string Name { get; }
    public MonitorMode Mode { get; }
    public string PortA { get; }
    public string? PortB { get; }
    public SerialPortSettings Settings { get; }
    internal SerialBridge Bridge { get; }

    public bool IsOpen => Bridge.IsOpen;
    public long BytesAToB => Bridge.BytesAToB;
    public long BytesBToA => Bridge.BytesBToA;
    public string? LastError { get; internal set; }

    public IEnumerable<string> Ports => Mode == MonitorMode.Bridge && PortB is not null
        ? new[] { PortA, PortB }
        : new[] { PortA };

    public string Description => Mode == MonitorMode.Bridge
        ? $"桥接 {PortA} ↔ {PortB} @ {Settings.BaudRate}"
        : $"单口 {PortA} @ {Settings.BaudRate}";
}
