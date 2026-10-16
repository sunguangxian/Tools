using System.Globalization;
using System.Text;

namespace PortMonitor.Core.Usb;

public static class ScsiCsvExporter
{
    public static void Write(string path, IEnumerable<ScsiCommand> commands)
    {
        using var sw = new StreamWriter(path, false, new UTF8Encoding(true));
        sw.WriteLine("Index,Timestamp,Device,Tag,LUN,Opcode,Command,LBA,Blocks,ExpectedBytes,DataBytes,Status,Residue,DurationMs,Summary,CDB");
        foreach (var c in commands)
        {
            string[] f =
            [
                c.Index.ToString(CultureInfo.InvariantCulture),
                c.Timestamp.ToLocalTime().ToString("yyyy-MM-dd HH:mm:ss.ffffff", CultureInfo.InvariantCulture),
                c.DeviceLabel,
                $"0x{c.Tag:X8}",
                c.Lun.ToString(CultureInfo.InvariantCulture),
                $"0x{c.Info.Opcode:X2}",
                c.OpcodeName,
                c.Info.Lba?.ToString(CultureInfo.InvariantCulture) ?? "",
                c.Info.Blocks?.ToString(CultureInfo.InvariantCulture) ?? "",
                c.ExpectedLength.ToString(CultureInfo.InvariantCulture),
                c.DataBytes.ToString(CultureInfo.InvariantCulture),
                c.StatusLabel,
                c.Residue.ToString(CultureInfo.InvariantCulture),
                c.DurationMs?.ToString("0.###", CultureInfo.InvariantCulture) ?? "",
                c.Summary,
                Convert.ToHexString(c.Cdb)
            ];
            sw.WriteLine(string.Join(',', f.Select(Escape)));
        }
    }

    private static string Escape(string v) =>
        v.IndexOfAny(['"', ',', '\r', '\n']) < 0 ? v : "\"" + v.Replace("\"", "\"\"") + "\"";
}
