using System.IO.Ports;

namespace SscomAvalonia.Services;

public sealed class SerialService : IDisposable
{
    private SerialPort? _port;

    public event Action<byte[]>? DataReceived;
    public event Action<string>? ErrorOccurred;

    public bool IsOpen => _port?.IsOpen == true;

    public static string[] AvailablePorts()
    {
        try
        {
            var ports = SerialPort.GetPortNames();
            Array.Sort(ports, StringComparer.Ordinal);
            return ports;
        }
        catch
        {
            return Array.Empty<string>();
        }
    }

    public bool Open(string portName, int baud, int dataBits, int stopBits,
        int parity, int flow, bool dtr, bool rts, out string error)
    {
        error = "";
        try
        {
            Close();
            var sp = new SerialPort(portName)
            {
                BaudRate = baud,
                DataBits = dataBits,
                StopBits = stopBits switch
                {
                    15 => StopBits.OnePointFive,
                    2 => StopBits.Two,
                    _ => StopBits.One
                },
                Parity = parity switch
                {
                    1 => Parity.Odd,
                    2 => Parity.Even,
                    3 => Parity.Mark,
                    4 => Parity.Space,
                    _ => Parity.None
                },
                Handshake = flow switch
                {
                    1 => Handshake.RequestToSend,
                    2 => Handshake.XOnXOff,
                    _ => Handshake.None
                },
                ReadTimeout = 500,
                WriteTimeout = 500,
                DtrEnable = dtr,
                RtsEnable = rts
            };
            sp.DataReceived += OnDataReceived;
            sp.ErrorReceived += OnErrorReceived;
            sp.Open();
            _port = sp;
            return true;
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }

    public void Close()
    {
        if (_port == null) return;
        try
        {
            _port.DataReceived -= OnDataReceived;
            _port.ErrorReceived -= OnErrorReceived;
            if (_port.IsOpen) _port.Close();
            _port.Dispose();
        }
        catch { /* ignore */ }
        _port = null;
    }

    public int Write(byte[] data)
    {
        if (_port?.IsOpen != true) return -1;
        try
        {
            _port.Write(data, 0, data.Length);
            return data.Length;
        }
        catch (Exception ex)
        {
            ErrorOccurred?.Invoke(ex.Message);
            return -1;
        }
    }

    public void SetDtr(bool on)
    {
        try
        {
            if (_port?.IsOpen == true) _port.DtrEnable = on;
        }
        catch (Exception ex)
        {
            ErrorOccurred?.Invoke($"DTR: {ex.Message}");
        }
    }

    public void SetRts(bool on)
    {
        try
        {
            if (_port?.IsOpen == true) _port.RtsEnable = on;
        }
        catch (Exception ex)
        {
            ErrorOccurred?.Invoke($"RTS: {ex.Message}");
        }
    }

    private void OnDataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            if (_port == null || !_port.IsOpen) return;
            var n = _port.BytesToRead;
            if (n <= 0) return;
            var buf = new byte[n];
            var read = _port.Read(buf, 0, n);
            if (read > 0)
            {
                if (read != buf.Length) Array.Resize(ref buf, read);
                DataReceived?.Invoke(buf);
            }
        }
        catch (Exception ex)
        {
            ErrorOccurred?.Invoke(ex.Message);
        }
    }

    private void OnErrorReceived(object sender, SerialErrorReceivedEventArgs e)
    {
        ErrorOccurred?.Invoke(e.EventType.ToString());
    }

    public void Dispose() => Close();
}
