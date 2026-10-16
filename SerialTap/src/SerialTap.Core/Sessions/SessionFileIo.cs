using System.Buffers.Binary;
using System.Text;
using SerialTap.Core.Models;

namespace SerialTap.Core.Sessions;

public sealed class SessionFileInfo
{
    public int Id { get; set; }
    public int Mode { get; set; }
    public int Baud { get; set; } = 9600;
    public int DataBits { get; set; } = 8;
    public int Parity { get; set; }
    public int Stop { get; set; }
    public string Name { get; set; } = "";
    public string PortA { get; set; } = "";
    public string PortB { get; set; } = "";
}

public sealed class SessionFileRecord
{
    public byte Kind { get; set; } // 0 data, 1 event
    public long TimestampTicks { get; set; }
    public int SessionId { get; set; }
    public int Direction { get; set; }
    public byte[] Data { get; set; } = Array.Empty<byte>();
}

public sealed class SessionFile
{
    public List<SessionFileInfo> Sessions { get; } = new();
    public List<SessionFileRecord> Records { get; } = new();
}

/// <summary>Binary .stqs session file (compatible layout with Native/Qt P1).</summary>
public static class SessionFileIo
{
    private static readonly byte[] Magic = "STQS"u8.ToArray();

    public static void Save(string path, SessionFile file)
    {
        using var ms = new MemoryStream();
        ms.Write(Magic);
        WriteU32(ms, 1);
        WriteU32(ms, (uint)file.Sessions.Count);
        foreach (var s in file.Sessions)
        {
            WriteI32(ms, s.Id); WriteI32(ms, s.Mode); WriteI32(ms, s.Baud);
            WriteI32(ms, s.DataBits); WriteI32(ms, s.Parity); WriteI32(ms, s.Stop);
            WriteStr(ms, s.Name); WriteStr(ms, s.PortA); WriteStr(ms, s.PortB);
        }
        WriteU64(ms, (ulong)file.Records.Count);
        foreach (var r in file.Records)
        {
            ms.WriteByte(r.Kind);
            WriteU64(ms, (ulong)r.TimestampTicks);
            WriteI32(ms, r.SessionId);
            WriteI32(ms, r.Direction);
            WriteU32(ms, (uint)r.Data.Length);
            ms.Write(r.Data);
        }
        File.WriteAllBytes(path, ms.ToArray());
    }

    public static SessionFile Load(string path)
    {
        var d = File.ReadAllBytes(path);
        int pos = 0;
        if (d.Length < 12 || !d.AsSpan(0, 4).SequenceEqual(Magic))
            throw new InvalidDataException("不是 SerialTap 会话文件");
        pos = 4;
        uint ver = ReadU32(d, ref pos);
        if (ver != 1) throw new InvalidDataException($"不支持的版本 {ver}");
        uint ns = ReadU32(d, ref pos);
        var f = new SessionFile();
        for (uint i = 0; i < ns; i++)
        {
            var s = new SessionFileInfo
            {
                Id = ReadI32(d, ref pos), Mode = ReadI32(d, ref pos), Baud = ReadI32(d, ref pos),
                DataBits = ReadI32(d, ref pos), Parity = ReadI32(d, ref pos), Stop = ReadI32(d, ref pos),
                Name = ReadStr(d, ref pos), PortA = ReadStr(d, ref pos), PortB = ReadStr(d, ref pos)
            };
            f.Sessions.Add(s);
        }
        ulong nr = ReadU64(d, ref pos);
        for (ulong i = 0; i < nr; i++)
        {
            byte kind = d[pos++];
            var r = new SessionFileRecord
            {
                Kind = kind,
                TimestampTicks = (long)ReadU64(d, ref pos),
                SessionId = ReadI32(d, ref pos),
                Direction = ReadI32(d, ref pos)
            };
            uint n = ReadU32(d, ref pos);
            r.Data = d.AsSpan(pos, (int)n).ToArray();
            pos += (int)n;
            f.Records.Add(r);
        }
        return f;
    }

    private static void WriteU32(Stream s, uint v) { Span<byte> b = stackalloc byte[4]; BinaryPrimitives.WriteUInt32LittleEndian(b, v); s.Write(b); }
    private static void WriteI32(Stream s, int v) => WriteU32(s, (uint)v);
    private static void WriteU64(Stream s, ulong v) { Span<byte> b = stackalloc byte[8]; BinaryPrimitives.WriteUInt64LittleEndian(b, v); s.Write(b); }
    private static void WriteStr(Stream s, string str)
    {
        var bytes = Encoding.UTF8.GetBytes(str ?? "");
        WriteU32(s, (uint)bytes.Length);
        s.Write(bytes);
    }
    private static uint ReadU32(byte[] d, ref int pos) { var v = BinaryPrimitives.ReadUInt32LittleEndian(d.AsSpan(pos)); pos += 4; return v; }
    private static int ReadI32(byte[] d, ref int pos) => (int)ReadU32(d, ref pos);
    private static ulong ReadU64(byte[] d, ref int pos) { var v = BinaryPrimitives.ReadUInt64LittleEndian(d.AsSpan(pos)); pos += 8; return v; }
    private static string ReadStr(byte[] d, ref int pos)
    {
        uint n = ReadU32(d, ref pos);
        var s = Encoding.UTF8.GetString(d, pos, (int)n);
        pos += (int)n;
        return s;
    }
}
