using System.Globalization;
using System.Text;
using PortMonitor.Core.Models;

namespace PortMonitor.Core.Export;

public static class CsvExporter
{
    public static void Write(string path, IEnumerable<SerialEvent> events)
    {
        using var sw = new StreamWriter(path, false, new UTF8Encoding(encoderShouldEmitUTF8Identifier: true));
        sw.WriteLine("Sequence,Session,Timestamp,Direction,SourcePort,DestPort,Length,Hex,Ascii");
        foreach (var e in events)
        {
            sw.Write(e.Sequence.ToString(CultureInfo.InvariantCulture));
            sw.Write(',');
            sw.Write(Escape(e.SessionName));
            sw.Write(',');
            sw.Write(Escape(e.Timestamp.ToString("yyyy-MM-dd HH:mm:ss.fff", CultureInfo.InvariantCulture)));
            sw.Write(',');
            sw.Write(Escape(e.DirectionLabel));
            sw.Write(',');
            sw.Write(Escape(e.SourcePort));
            sw.Write(',');
            sw.Write(Escape(e.DestPort));
            sw.Write(',');
            sw.Write(e.Length.ToString(CultureInfo.InvariantCulture));
            sw.Write(',');
            sw.Write(Escape(e.HexDump));
            sw.Write(',');
            sw.Write(Escape(e.AsciiPreview));
            sw.WriteLine();
        }
    }

    private static string Escape(string value)
    {
        if (value.IndexOfAny(['"', ',', '\r', '\n']) < 0)
            return value;
        return "\"" + value.Replace("\"", "\"\"") + "\"";
    }
}
