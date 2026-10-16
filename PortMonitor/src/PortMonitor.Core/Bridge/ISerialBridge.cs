using PortMonitor.Core.Models;

namespace PortMonitor.Core.Bridge;

public interface ISerialBridge : IAsyncDisposable
{
    bool IsOpen { get; }
    long BytesAToB { get; }
    long BytesBToA { get; }
    event EventHandler<SerialEvent>? DataCaptured;
    event EventHandler<string>? ErrorOccurred;

    Task OpenBridgeAsync(string portA, string portB, SerialPortSettings settings, CancellationToken ct = default);
    Task OpenSingleAsync(string port, SerialPortSettings settings, CancellationToken ct = default);
    Task CloseAsync();
}
