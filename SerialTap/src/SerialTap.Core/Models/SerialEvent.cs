namespace SerialTap.Core.Models;

/// <summary>One captured chunk of serial traffic.</summary>
public sealed class SerialEvent
{
    public long Sequence { get; init; }
    public int SessionId { get; init; }
    public string SessionName { get; init; } = string.Empty;
    public DateTimeOffset Timestamp { get; init; }
    public SerialDirection Direction { get; init; }
    public string SourcePort { get; init; } = string.Empty;
    public string DestPort { get; init; } = string.Empty;
    public byte[] Data { get; init; } = Array.Empty<byte>();

    public int Length => Data.Length;

    public string DirectionLabel => Direction switch
    {
        SerialDirection.AToB => "A→B",
        SerialDirection.BToA => "B→A",
        SerialDirection.Receive => "RX",
        SerialDirection.Transmit => "TX",
        _ => "?"
    };

    public string HexDump => ToHex(Data);
    public string AsciiPreview => ToAscii(Data);

    public static string ToHex(ReadOnlySpan<byte> data)
    {
        if (data.IsEmpty) return string.Empty;
        var chars = new char[data.Length * 3 - 1];
        for (int i = 0; i < data.Length; i++)
        {
            var b = data[i];
            chars[i * 3] = HexNibble(b >> 4);
            chars[i * 3 + 1] = HexNibble(b & 0xF);
            if (i < data.Length - 1)
                chars[i * 3 + 2] = ' ';
        }
        return new string(chars);
    }

    public static string ToAscii(ReadOnlySpan<byte> data)
    {
        if (data.IsEmpty) return string.Empty;
        var chars = new char[data.Length];
        for (int i = 0; i < data.Length; i++)
        {
            var b = data[i];
            chars[i] = b is >= 0x20 and <= 0x7E ? (char)b : '.';
        }
        return new string(chars);
    }

    private static char HexNibble(int v) => (char)(v < 10 ? '0' + v : 'A' + (v - 10));
}
