using System.Globalization;
using System.Text;
using SerialTap.Core.Models;

namespace SerialTap.Core.Export;

public static class TxtExporter
{
    public static void Write(string path, IEnumerable<SerialEvent> events)
    {
        using var sw = new StreamWriter(path, false, new UTF8Encoding(true));
        sw.WriteLine("# SerialTap export");
        sw.WriteLine("# Sequence\tSession\tTimestamp\tDirection\tPorts\tLength\tHex\tAscii");
        foreach (var e in events)
        {
            var ports = string.IsNullOrEmpty(e.DestPort) ? e.SourcePort : $"{e.SourcePort}->{e.DestPort}";
            sw.WriteLine(string.Join('\t',
                e.Sequence.ToString(CultureInfo.InvariantCulture),
                e.SessionName,
                e.Timestamp.ToString("yyyy-MM-dd HH:mm:ss.fff", CultureInfo.InvariantCulture),
                e.DirectionLabel,
                ports,
                e.Length.ToString(CultureInfo.InvariantCulture),
                e.HexDump,
                e.AsciiPreview));
        }
    }
}
