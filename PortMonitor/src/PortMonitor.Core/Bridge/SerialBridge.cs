using System.IO.Ports;
using PortMonitor.Core.Models;

namespace PortMonitor.Core.Bridge;

/// <summary>
/// User-space serial bridge / single-port listener.
/// Bridge mode opens two ports and copies bytes both ways while logging.
/// Does NOT attach to an already-open COM port (that needs a kernel filter).
/// </summary>
public sealed class SerialBridge : ISerialBridge
{
    public SerialBridge(int sessionId = 0, string sessionName = "")
    {
        SessionId = sessionId;
        SessionName = sessionName;
    }

    public int SessionId { get; }
    public string SessionName { get; set; }

    private readonly object _gate = new();
    private SerialPort? _portA;
    private SerialPort? _portB;
    private CancellationTokenSource? _cts;
    private Task? _pumpA;
    private Task? _pumpB;
    private static long _seq; // global across sessions
    private long _bytesAToB;
    private long _bytesBToA;
    private bool _isOpen;

    public bool IsOpen
    {
        get { lock (_gate) return _isOpen; }
    }

    public long BytesAToB => Interlocked.Read(ref _bytesAToB);
    public long BytesBToA => Interlocked.Read(ref _bytesBToA);

    public event EventHandler<SerialEvent>? DataCaptured;
    public event EventHandler<string>? ErrorOccurred;

    public Task OpenBridgeAsync(string portA, string portB, SerialPortSettings settings, CancellationToken ct = default)
    {
        if (string.IsNullOrWhiteSpace(portA)) throw new ArgumentException("Port A is required.", nameof(portA));
        if (string.IsNullOrWhiteSpace(portB)) throw new ArgumentException("Port B is required.", nameof(portB));
        if (string.Equals(portA, portB, StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("Port A and Port B must be different.");

        lock (_gate)
        {
            if (_isOpen) throw new InvalidOperationException("Already open. Close first.");
            _portA = CreatePort(portA, settings);
            _portB = CreatePort(portB, settings);
            try
            {
                _portA.Open();
                _portB.Open();
            }
            catch
            {
                SafeClose(_portA);
                SafeClose(_portB);
                _portA = null;
                _portB = null;
                throw;
            }

            _cts = new CancellationTokenSource();
            var token = _cts.Token;
            _bytesAToB = 0;
            _bytesBToA = 0;
            _isOpen = true;
            _pumpA = Task.Run(() => PumpAsync(_portA, _portB, SerialDirection.AToB, token), token);
            _pumpB = Task.Run(() => PumpAsync(_portB, _portA, SerialDirection.BToA, token), token);
        }

        return Task.CompletedTask;
    }

    public Task OpenSingleAsync(string port, SerialPortSettings settings, CancellationToken ct = default)
    {
        if (string.IsNullOrWhiteSpace(port)) throw new ArgumentException("Port is required.", nameof(port));

        lock (_gate)
        {
            if (_isOpen) throw new InvalidOperationException("Already open. Close first.");
            _portA = CreatePort(port, settings);
            try
            {
                _portA.Open();
            }
            catch
            {
                SafeClose(_portA);
                _portA = null;
                throw;
            }

            _cts = new CancellationTokenSource();
            var token = _cts.Token;
            _bytesAToB = 0;
            _bytesBToA = 0;
            _isOpen = true;
            // Listen only — no second port to forward to.
            _pumpA = Task.Run(() => ListenAsync(_portA, token), token);
        }

        return Task.CompletedTask;
    }

    public async Task CloseAsync()
    {
        CancellationTokenSource? cts;
        Task? t1;
        Task? t2;
        SerialPort? a;
        SerialPort? b;

        lock (_gate)
        {
            if (!_isOpen) return;
            _isOpen = false;
            cts = _cts;
            t1 = _pumpA;
            t2 = _pumpB;
            a = _portA;
            b = _portB;
            _cts = null;
            _pumpA = null;
            _pumpB = null;
            _portA = null;
            _portB = null;
        }

        try { cts?.Cancel(); } catch { /* ignore */ }

        try
        {
            if (t1 is not null) await t1.ConfigureAwait(false);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { RaiseError(ex.Message); }

        try
        {
            if (t2 is not null) await t2.ConfigureAwait(false);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { RaiseError(ex.Message); }

        SafeClose(a);
        SafeClose(b);
        cts?.Dispose();
    }

    public async ValueTask DisposeAsync()
    {
        await CloseAsync().ConfigureAwait(false);
    }

    private async Task PumpAsync(SerialPort from, SerialPort to, SerialDirection direction, CancellationToken ct)
    {
        var buffer = new byte[4096];
        try
        {
            while (!ct.IsCancellationRequested)
            {
                int n;
                try
                {
                    n = await from.BaseStream.ReadAsync(buffer.AsMemory(0, buffer.Length), ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException) { break; }
                catch (ObjectDisposedException) { break; }
                catch (IOException ex)
                {
                    RaiseError($"Read failed ({from.PortName}): {ex.Message}");
                    break;
                }

                if (n <= 0) continue;

                var chunk = new byte[n];
                Buffer.BlockCopy(buffer, 0, chunk, 0, n);

                try
                {
                    await to.BaseStream.WriteAsync(chunk.AsMemory(0, n), ct).ConfigureAwait(false);
                    await to.BaseStream.FlushAsync(ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException) { break; }
                catch (ObjectDisposedException) { break; }
                catch (IOException ex)
                {
                    RaiseError($"Write failed ({to.PortName}): {ex.Message}");
                    break;
                }

                if (direction == SerialDirection.AToB)
                    Interlocked.Add(ref _bytesAToB, n);
                else
                    Interlocked.Add(ref _bytesBToA, n);

                Emit(direction, from.PortName, to.PortName, chunk);
            }
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            RaiseError(ex.Message);
        }
    }

    private async Task ListenAsync(SerialPort port, CancellationToken ct)
    {
        var buffer = new byte[4096];
        try
        {
            while (!ct.IsCancellationRequested)
            {
                int n;
                try
                {
                    n = await port.BaseStream.ReadAsync(buffer.AsMemory(0, buffer.Length), ct).ConfigureAwait(false);
                }
                catch (OperationCanceledException) { break; }
                catch (ObjectDisposedException) { break; }
                catch (IOException ex)
                {
                    RaiseError($"Read failed ({port.PortName}): {ex.Message}");
                    break;
                }

                if (n <= 0) continue;

                var chunk = new byte[n];
                Buffer.BlockCopy(buffer, 0, chunk, 0, n);
                Interlocked.Add(ref _bytesAToB, n);
                Emit(SerialDirection.Receive, port.PortName, string.Empty, chunk);
            }
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            RaiseError(ex.Message);
        }
    }

    private void Emit(SerialDirection direction, string source, string dest, byte[] data)
    {
        var ev = new SerialEvent
        {
            Sequence = Interlocked.Increment(ref _seq),
            SessionId = SessionId,
            SessionName = SessionName,
            Timestamp = DateTimeOffset.Now,
            Direction = direction,
            SourcePort = source,
            DestPort = dest,
            Data = data
        };
        try
        {
            DataCaptured?.Invoke(this, ev);
        }
        catch
        {
            // UI handlers must not break the pump.
        }
    }

    private void RaiseError(string message)
    {
        try { ErrorOccurred?.Invoke(this, message); } catch { /* ignore */ }
    }

    private static SerialPort CreatePort(string name, SerialPortSettings s)
    {
        return new SerialPort(name)
        {
            BaudRate = s.BaudRate,
            DataBits = s.DataBits,
            Parity = MapParity(s.Parity),
            StopBits = MapStopBits(s.StopBits),
            Handshake = Handshake.None,
            ReadTimeout = SerialPort.InfiniteTimeout,
            WriteTimeout = 5000,
            ReadBufferSize = 65536,
            WriteBufferSize = 65536
        };
    }

    private static Parity MapParity(ParityKind p) => p switch
    {
        ParityKind.Odd => Parity.Odd,
        ParityKind.Even => Parity.Even,
        ParityKind.Mark => Parity.Mark,
        ParityKind.Space => Parity.Space,
        _ => Parity.None
    };

    private static StopBits MapStopBits(StopBitsKind s) => s switch
    {
        StopBitsKind.Two => StopBits.Two,
        StopBitsKind.OnePointFive => StopBits.OnePointFive,
        _ => StopBits.One
    };

    private static void SafeClose(SerialPort? port)
    {
        if (port is null) return;
        try
        {
            if (port.IsOpen) port.Close();
        }
        catch { /* ignore */ }
        try { port.Dispose(); } catch { /* ignore */ }
    }
}
