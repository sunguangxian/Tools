using PortMonitor.Core.Protocol;

namespace PortMonitor.Core.Models;

/// <summary>One Modbus RTU frame extracted from a serial byte stream.</summary>
public sealed class ModbusFrame
{
    public long Sequence { get; init; }
    public int SessionId { get; init; }
    public string SessionName { get; init; } = string.Empty;
    public DateTimeOffset Timestamp { get; init; }
    public SerialDirection Direction { get; init; }
    public string SourcePort { get; init; } = string.Empty;
    public string DestPort { get; init; } = string.Empty;
    public byte[] Raw { get; init; } = Array.Empty<byte>();
    public bool CrcOk { get; init; }
    public byte SlaveAddress { get; init; }
    public byte FunctionCode { get; init; }
    public bool IsException { get; init; }
    public string Summary { get; init; } = string.Empty;
    public bool IsAscii { get; init; }
    public string HexDump => SerialEvent.ToHex(Raw);

    public string DirectionLabel => Direction switch
    {
        SerialDirection.AToB => "A→B",
        SerialDirection.BToA => "B→A",
        SerialDirection.Receive => "RX",
        SerialDirection.Transmit => "TX",
        _ => "?"
    };

    public string CrcLabel => CrcOk ? "OK" : "FAIL";
    public string ProtocolLabel => IsAscii ? "ASCII" : "RTU";
    public string FunctionLabel => IsException
        ? $"0x{FunctionCode:X2} (异常)"
        : $"0x{FunctionCode:X2}";
}
