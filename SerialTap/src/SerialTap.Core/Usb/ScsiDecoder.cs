using System.Buffers.Binary;
using System.Text;

namespace SerialTap.Core.Usb;

public readonly record struct CdbInfo(byte Opcode, string Name, ulong? Lba, uint? Blocks, string Detail);

/// <summary>SCSI CDB / response decoding for USB Mass Storage (BOT).</summary>
public static class ScsiDecoder
{
    public static CdbInfo DecodeCdb(ReadOnlySpan<byte> cdb)
    {
        if (cdb.IsEmpty) return new CdbInfo(0, "(空 CDB)", null, null, "");
        byte op = cdb[0];
        static uint BE16(ReadOnlySpan<byte> s, int o) => s.Length >= o + 2 ? BinaryPrimitives.ReadUInt16BigEndian(s[o..]) : 0u;
        static uint BE32(ReadOnlySpan<byte> s, int o) => s.Length >= o + 4 ? BinaryPrimitives.ReadUInt32BigEndian(s[o..]) : 0u;
        static ulong BE64(ReadOnlySpan<byte> s, int o) => s.Length >= o + 8 ? BinaryPrimitives.ReadUInt64BigEndian(s[o..]) : 0ul;

        switch (op)
        {
            case 0x00: return new(op, "TEST UNIT READY", null, null, "");
            case 0x03: return new(op, "REQUEST SENSE", null, null, $"分配长度={(cdb.Length > 4 ? cdb[4] : 0)}");
            case 0x12:
            {
                bool evpd = cdb.Length > 1 && (cdb[1] & 1) != 0;
                string d = evpd ? $"VPD 页=0x{(cdb.Length > 2 ? cdb[2] : 0):X2}" : "标准";
                return new(op, "INQUIRY", null, null, $"{d} 分配长度={BE16(cdb, 3)}");
            }
            case 0x1A: return new(op, "MODE SENSE(6)", null, null, $"页=0x{(cdb.Length > 2 ? cdb[2] & 0x3F : 0):X2} 分配长度={(cdb.Length > 4 ? cdb[4] : 0)}");
            case 0x5A: return new(op, "MODE SENSE(10)", null, null, $"页=0x{(cdb.Length > 2 ? cdb[2] & 0x3F : 0):X2} 分配长度={BE16(cdb, 7)}");
            case 0x1B:
            {
                byte b4 = cdb.Length > 4 ? cdb[4] : (byte)0;
                bool start = (b4 & 1) != 0, loej = (b4 & 2) != 0;
                string act = loej ? (start ? "装载介质" : "弹出介质") : (start ? "启动" : "停止");
                return new(op, "START STOP UNIT", null, null, act);
            }
            case 0x1E:
                return new(op, "PREVENT ALLOW MEDIUM REMOVAL", null, null,
                    (cdb.Length > 4 && (cdb[4] & 3) != 0) ? "禁止移除" : "允许移除");
            case 0x23: return new(op, "READ FORMAT CAPACITIES", null, null, $"分配长度={BE16(cdb, 7)}");
            case 0x25: return new(op, "READ CAPACITY(10)", null, null, "");
            case 0x9E when cdb.Length > 1 && (cdb[1] & 0x1F) == 0x10:
                return new(op, "READ CAPACITY(16)", null, null, $"分配长度={BE32(cdb, 10)}");
            case 0x28: return new(op, "READ(10)", BE32(cdb, 2), BE16(cdb, 7), "");
            case 0x2A: return new(op, "WRITE(10)", BE32(cdb, 2), BE16(cdb, 7), "");
            case 0xA8: return new(op, "READ(12)", BE32(cdb, 2), BE32(cdb, 6), "");
            case 0xAA: return new(op, "WRITE(12)", BE32(cdb, 2), BE32(cdb, 6), "");
            case 0x88: return new(op, "READ(16)", BE64(cdb, 2), BE32(cdb, 10), "");
            case 0x8A: return new(op, "WRITE(16)", BE64(cdb, 2), BE32(cdb, 10), "");
            case 0x35: return new(op, "SYNCHRONIZE CACHE(10)", BE32(cdb, 2), BE16(cdb, 7), "");
            case 0x91: return new(op, "SYNCHRONIZE CACHE(16)", BE64(cdb, 2), BE32(cdb, 10), "");
            default: return new(op, $"OP 0x{op:X2}", null, null, $"CDB={Hex(cdb)}");
        }
    }

    /// <summary>Summarise a data-in payload for known opcodes; returns null if nothing to say.</summary>
    public static string? DecodeResponse(byte opcode, ReadOnlySpan<byte> cdb, ReadOnlySpan<byte> data)
    {
        if (data.IsEmpty) return null;
        switch (opcode)
        {
            case 0x12 when data.Length >= 36 && !(cdb.Length > 1 && (cdb[1] & 1) != 0):
            {
                int type = data[0] & 0x1F;
                bool rmb = (data[1] & 0x80) != 0;
                return $"厂商=\"{Ascii(data.Slice(8, 8))}\" 产品=\"{Ascii(data.Slice(16, 16))}\" 版本=\"{Ascii(data.Slice(32, 4))}\" 类型={DeviceType(type)}{(rmb ? " 可移动" : "")}";
            }
            case 0x25 when data.Length >= 8:
            {
                ulong last = BinaryPrimitives.ReadUInt32BigEndian(data);
                uint bs = BinaryPrimitives.ReadUInt32BigEndian(data[4..]);
                return Capacity(last, bs);
            }
            case 0x9E when data.Length >= 12:
            {
                ulong last = BinaryPrimitives.ReadUInt64BigEndian(data);
                uint bs = BinaryPrimitives.ReadUInt32BigEndian(data[8..]);
                return Capacity(last, bs);
            }
            case 0x03:
                return DecodeSense(data);
            case 0x23 when data.Length >= 12:
            {
                uint blocks = BinaryPrimitives.ReadUInt32BigEndian(data[4..]);
                int kind = data[8] & 3;
                uint bl = (uint)(data[9] << 16 | data[10] << 8 | data[11]);
                string k = kind switch { 1 => "未格式化", 2 => "已格式化", 3 => "无介质", _ => "保留" };
                return $"块数={blocks} 块大小={bl} ({FormatSize((ulong)blocks * bl)}) {k}";
            }
            case 0x1A when data.Length >= 4:
                return (data[2] & 0x80) != 0 ? "写保护(WP)" : "可写";
            case 0x5A when data.Length >= 8:
                return (data[3] & 0x80) != 0 ? "写保护(WP)" : "可写";
            default:
                return null;
        }
    }

    public static string? DecodeSense(ReadOnlySpan<byte> d)
    {
        if (d.Length < 4) return null;
        int code = d[0] & 0x7F;
        int key, asc, ascq;
        if (code is 0x70 or 0x71)
        {
            if (d.Length < 14) return null;
            key = d[2] & 0x0F; asc = d[12]; ascq = d[13];
        }
        else if (code is 0x72 or 0x73)
        {
            key = d[1] & 0x0F; asc = d[2]; ascq = d[3];
        }
        else return null;
        return $"Sense: {SenseKeyText(key)} ASC/ASCQ={asc:X2}/{ascq:X2} {AscText(asc, ascq)}".TrimEnd();
    }

    public static string SenseKeyText(int key) => key switch
    {
        0x0 => "NO SENSE", 0x1 => "RECOVERED ERROR", 0x2 => "NOT READY", 0x3 => "MEDIUM ERROR",
        0x4 => "HARDWARE ERROR", 0x5 => "ILLEGAL REQUEST", 0x6 => "UNIT ATTENTION", 0x7 => "DATA PROTECT",
        0x8 => "BLANK CHECK", 0xB => "ABORTED COMMAND", 0xE => "MISCOMPARE", _ => $"KEY 0x{key:X}"
    };

    public static string AscText(int asc, int ascq) => (asc, ascq) switch
    {
        (0x04, 0x01) => "(正在就绪)",
        (0x04, _) => "(逻辑单元未就绪)",
        (0x11, 0x00) => "(不可恢复读错误)",
        (0x1A, 0x00) => "(参数列表长度错误)",
        (0x20, 0x00) => "(无效操作码)",
        (0x21, 0x00) => "(LBA 越界)",
        (0x24, 0x00) => "(CDB 字段无效)",
        (0x25, 0x00) => "(LUN 不支持)",
        (0x27, 0x00) => "(写保护)",
        (0x28, 0x00) => "(介质可能已更换)",
        (0x29, _) => "(上电/复位)",
        (0x3A, _) => "(无介质)",
        _ => ""
    };

    public static string FormatSize(ulong bytes)
    {
        string[] u = ["B", "KB", "MB", "GB", "TB", "PB"];
        double v = bytes; int i = 0;
        while (v >= 1024 && i < u.Length - 1) { v /= 1024; i++; }
        return $"{v:0.##} {u[i]}";
    }

    private static string Capacity(ulong lastLba, uint blockSize) =>
        $"最后 LBA={lastLba} 块大小={blockSize} 容量={FormatSize((lastLba + 1) * blockSize)}";

    private static string DeviceType(int t) => t switch
    {
        0x00 => "直接访问(磁盘)", 0x05 => "CD/DVD", 0x07 => "光存储", 0x0E => "简化直接访问", _ => $"0x{t:X2}"
    };

    private static string Ascii(ReadOnlySpan<byte> s)
    {
        var sb = new StringBuilder(s.Length);
        foreach (var b in s) sb.Append(b is >= 0x20 and < 0x7F ? (char)b : ' ');
        return sb.ToString().Trim();
    }

    private static string Hex(ReadOnlySpan<byte> s) => Convert.ToHexString(s);
}
