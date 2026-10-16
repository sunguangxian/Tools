using System.IO.Ports;

namespace PortMonitor.Core.Bridge;

/// <summary>Lists available serial port names. Safe when no ports exist.</summary>
public static class SerialPortEnumerator
{
    public static IReadOnlyList<string> GetPortNames()
    {
        try
        {
            var names = SerialPort.GetPortNames();
            Array.Sort(names, StringComparer.OrdinalIgnoreCase);
            return names;
        }
        catch
        {
            return Array.Empty<string>();
        }
    }
}
