using System.Globalization;
using System.Net;
using System.Text;
using PortMonitor.Core.Models;

namespace PortMonitor.Core.Export;

public static class HtmlExporter
{
    public static void Write(string path, IEnumerable<SerialEvent> events)
    {
        using var sw = new StreamWriter(path, false, new UTF8Encoding(true));
        sw.WriteLine("<!DOCTYPE html><html><head><meta charset=\"utf-8\"/><title>PortMonitor</title>");
        sw.WriteLine("<style>body{font:12px Consolas,monospace;background:#1e1e1e;color:#d4d4d4}");
        sw.WriteLine("table{border-collapse:collapse;width:100%}th,td{border:1px solid #333;padding:2px 6px;text-align:left}");
        sw.WriteLine("th{background:#252526;color:#9cdcfe}.dir{color:#dcdcaa}.hex{color:#ce9178}.ok{color:#b5cea8}</style></head><body>");
        sw.WriteLine("<h3>PortMonitor export</h3><table><tr><th>#</th><th>Time</th><th>Session</th><th>Dir</th><th>Ports</th><th>Len</th><th>Hex</th><th>ASCII</th></tr>");
        foreach (var e in events)
        {
            var ports = string.IsNullOrEmpty(e.DestPort) ? e.SourcePort : $"{e.SourcePort} → {e.DestPort}";
            sw.Write("<tr><td>");
            sw.Write(e.Sequence);
            sw.Write("</td><td>");
            sw.Write(WebUtility.HtmlEncode(e.Timestamp.ToString("HH:mm:ss.fff", CultureInfo.InvariantCulture)));
            sw.Write("</td><td>");
            sw.Write(WebUtility.HtmlEncode(e.SessionName));
            sw.Write("</td><td class=\"dir\">");
            sw.Write(WebUtility.HtmlEncode(e.DirectionLabel));
            sw.Write("</td><td>");
            sw.Write(WebUtility.HtmlEncode(ports));
            sw.Write("</td><td>");
            sw.Write(e.Length);
            sw.Write("</td><td class=\"hex\">");
            sw.Write(WebUtility.HtmlEncode(e.HexDump));
            sw.Write("</td><td class=\"ok\">");
            sw.Write(WebUtility.HtmlEncode(e.AsciiPreview));
            sw.WriteLine("</td></tr>");
        }
        sw.WriteLine("</table></body></html>");
    }
}
