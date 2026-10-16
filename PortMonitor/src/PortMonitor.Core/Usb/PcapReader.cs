using System.Buffers.Binary;

namespace PortMonitor.Core.Usb;

public readonly record struct PcapRecord(DateTimeOffset Timestamp, int LinkType, byte[] Data);

/// <summary>
/// Streaming reader for classic pcap (µs / ns, either byte order) and little/big-endian pcapng
/// (SHB / IDB / EPB / SPB). Works on files and on a live stdout pipe.
/// </summary>
public static class PcapReader
{
    public static IEnumerable<PcapRecord> Read(Stream stream, CancellationToken ct = default)
    {
        var magic = new byte[4];
        if (!ReadExact(stream, magic)) yield break;
        uint m = BinaryPrimitives.ReadUInt32LittleEndian(magic);

        if (m == 0x0A0D0D0A)
        {
            foreach (var r in ReadPcapNg(stream, magic, ct)) yield return r;
            yield break;
        }

        bool le, nano;
        switch (m)
        {
            case 0xA1B2C3D4: le = true; nano = false; break;
            case 0xA1B23C4D: le = true; nano = true; break;
            case 0xD4C3B2A1: le = false; nano = false; break;
            case 0x4D3CB2A1: le = false; nano = true; break;
            default: throw new InvalidDataException($"不是 pcap/pcapng 文件（magic=0x{m:X8}）");
        }

        var gh = new byte[20];
        if (!ReadExact(stream, gh)) yield break;
        int linkType = (int)(U32(gh.AsSpan(16), le) & 0x0FFFFFFF);

        var rh = new byte[16];
        while (!ct.IsCancellationRequested && ReadExact(stream, rh))
        {
            uint sec = U32(rh, le), frac = U32(rh.AsSpan(4), le), incl = U32(rh.AsSpan(8), le);
            if (incl > 64 * 1024 * 1024) throw new InvalidDataException("记录长度异常，文件可能损坏");
            var data = new byte[incl];
            if (!ReadExact(stream, data)) yield break;
            long ticks = nano ? frac / 100 : frac * 10L;
            yield return new PcapRecord(DateTimeOffset.FromUnixTimeSeconds(sec).AddTicks(ticks), linkType, data);
        }
    }

    private static IEnumerable<PcapRecord> ReadPcapNg(Stream stream, byte[] firstType, CancellationToken ct)
    {
        bool le = true;
        var linkTypes = new List<int>();
        var tsDiv = new List<double>(); // timestamp units per second, per interface
        bool first = true;
        var hdr = new byte[8];

        while (!ct.IsCancellationRequested)
        {
            if (first)
            {
                first = false;
                firstType.CopyTo(hdr, 0);
                if (!ReadExact(stream, hdr.AsSpan(4, 4))) yield break;
            }
            else if (!ReadExact(stream, hdr)) yield break;

            uint type;
            uint total;
            byte[] body;

            if (BinaryPrimitives.ReadUInt32LittleEndian(hdr) == 0x0A0D0D0A)
            {
                // Section Header Block: byte-order magic decides endianness for the section.
                var bom = new byte[4];
                if (!ReadExact(stream, bom)) yield break;
                le = BinaryPrimitives.ReadUInt32LittleEndian(bom) == 0x1A2B3C4D;
                total = U32(hdr.AsSpan(4), le);
                if (total < 16 || total > 64 * 1024 * 1024) throw new InvalidDataException("pcapng 段头长度异常");
                if (!ReadExact(stream, new byte[total - 12])) yield break;
                linkTypes.Clear(); tsDiv.Clear();
                continue;
            }

            type = U32(hdr, le);
            total = U32(hdr.AsSpan(4), le);
            if (total < 12 || total > 64 * 1024 * 1024) throw new InvalidDataException("pcapng 块长度异常");
            body = new byte[total - 8];
            if (!ReadExact(stream, body)) yield break;
            var b = body.AsSpan(0, body.Length - 4); // strip trailing length

            switch (type)
            {
                case 1: // IDB
                {
                    linkTypes.Add(U16(b, le));
                    double div = 1_000_000;
                    int o = 8;
                    while (o + 4 <= b.Length)
                    {
                        int code = U16(b[o..], le), len = U16(b[(o + 2)..], le);
                        if (code == 0) break;
                        if (code == 9 && len >= 1 && o + 4 < b.Length)
                        {
                            byte r = b[o + 4];
                            div = (r & 0x80) != 0 ? Math.Pow(2, r & 0x7F) : Math.Pow(10, r);
                        }
                        o += 4 + ((len + 3) & ~3);
                    }
                    tsDiv.Add(div);
                    break;
                }
                case 6: // EPB
                {
                    int iface = (int)U32(b, le);
                    ulong ts = ((ulong)U32(b[4..], le) << 32) | U32(b[8..], le);
                    int cap = (int)U32(b[12..], le);
                    if (20 + cap > b.Length) break;
                    double div = iface < tsDiv.Count ? tsDiv[iface] : 1_000_000;
                    int lt = iface < linkTypes.Count ? linkTypes[iface] : 0;
                    yield return new PcapRecord(ToTime(ts, div), lt, b.Slice(20, cap).ToArray());
                    break;
                }
                case 3: // SPB
                {
                    int orig = (int)U32(b, le);
                    int cap = Math.Min(orig, b.Length - 4);
                    yield return new PcapRecord(DateTimeOffset.UnixEpoch, linkTypes.Count > 0 ? linkTypes[0] : 0,
                        b.Slice(4, cap).ToArray());
                    break;
                }
            }
        }
    }

    private static DateTimeOffset ToTime(ulong ts, double unitsPerSecond)
    {
        double seconds = ts / unitsPerSecond;
        long whole = (long)Math.Floor(seconds);
        return DateTimeOffset.FromUnixTimeSeconds(whole).AddTicks((long)((seconds - whole) * TimeSpan.TicksPerSecond));
    }

    private static uint U32(ReadOnlySpan<byte> s, bool le) =>
        le ? BinaryPrimitives.ReadUInt32LittleEndian(s) : BinaryPrimitives.ReadUInt32BigEndian(s);

    private static ushort U16(ReadOnlySpan<byte> s, bool le) =>
        le ? BinaryPrimitives.ReadUInt16LittleEndian(s) : BinaryPrimitives.ReadUInt16BigEndian(s);

    private static bool ReadExact(Stream s, Span<byte> buf)
    {
        int read = 0;
        while (read < buf.Length)
        {
            int n = s.Read(buf[read..]);
            if (n <= 0)
            {
                if (read == 0) return false;
                throw new EndOfStreamException("pcap 数据被截断");
            }
            read += n;
        }
        return true;
    }

    /// <summary>Writes a classic little-endian µs pcap file (sample generation / export).</summary>
    public static void WriteClassic(Stream output, int linkType, IEnumerable<(DateTimeOffset ts, byte[] data)> records)
    {
        var gh = new byte[24];
        BinaryPrimitives.WriteUInt32LittleEndian(gh, 0xA1B2C3D4);
        BinaryPrimitives.WriteUInt16LittleEndian(gh.AsSpan(4), 2);
        BinaryPrimitives.WriteUInt16LittleEndian(gh.AsSpan(6), 4);
        BinaryPrimitives.WriteUInt32LittleEndian(gh.AsSpan(16), 65535);
        BinaryPrimitives.WriteUInt32LittleEndian(gh.AsSpan(20), (uint)linkType);
        output.Write(gh);
        var rh = new byte[16];
        foreach (var (ts, data) in records)
        {
            long us = (ts - DateTimeOffset.UnixEpoch).Ticks / 10;
            BinaryPrimitives.WriteUInt32LittleEndian(rh, (uint)(us / 1_000_000));
            BinaryPrimitives.WriteUInt32LittleEndian(rh.AsSpan(4), (uint)(us % 1_000_000));
            BinaryPrimitives.WriteUInt32LittleEndian(rh.AsSpan(8), (uint)data.Length);
            BinaryPrimitives.WriteUInt32LittleEndian(rh.AsSpan(12), (uint)data.Length);
            output.Write(rh);
            output.Write(data);
        }
    }
}
