namespace PortMonitor.Core.Protocol;

/// <summary>
/// Modbus RTU frame field parser (CRC + function-code summary).
/// Framing (silence / length peel) lives in <see cref="ModbusRtuStreamParser"/>.
/// </summary>
public static class ModbusRtuParser
{
    public readonly record struct ParseResult(
        bool CrcOk,
        byte SlaveAddress,
        byte FunctionCode,
        bool IsException,
        string Summary);

    public static ParseResult TryParse(ReadOnlySpan<byte> frame)
    {
        if (frame.Length < 4)
            return new ParseResult(false, 0, 0, false, "太短，无法构成 RTU 帧");

        byte addr = frame[0];
        byte fcRaw = frame[1];
        bool isEx = (fcRaw & 0x80) != 0;
        byte fc = (byte)(fcRaw & 0x7F);
        bool crcOk = ModbusCrc16.ValidateFrame(frame);

        string summary = BuildSummary(frame, addr, fc, isEx, crcOk);
        return new ParseResult(crcOk, addr, fcRaw, isEx, summary);
    }

    private static string BuildSummary(ReadOnlySpan<byte> frame, byte addr, byte fc, bool isEx, bool crcOk)
    {
        if (!crcOk)
            return $"CRC 失败 | 从站={addr} FC=0x{frame[1]:X2} 长度={frame.Length}";

        if (isEx && frame.Length >= 3)
            return $"异常响应 | 从站={addr} FC=0x{fc:X2} 异常码={frame[2]}";

        // PDU without CRC
        var pdu = frame.Length >= 2 ? frame[2..^2] : ReadOnlySpan<byte>.Empty;

        return fc switch
        {
            0x01 or 0x02 when pdu.Length == 4 =>
                $"读线圈/离散 | 起始=0x{(pdu[0] << 8 | pdu[1]):X4} 数量={pdu[2] << 8 | pdu[3]}",
            0x01 or 0x02 when pdu.Length >= 1 =>
                $"读线圈/离散响应 | 字节数={pdu[0]} 数据={PortMonitor.Core.Models.SerialEvent.ToHex(pdu[1..])}",
            0x03 or 0x04 when pdu.Length == 4 =>
                $"读保持/输入寄存器 | 起始=0x{(pdu[0] << 8 | pdu[1]):X4} 数量={pdu[2] << 8 | pdu[3]}",
            0x03 or 0x04 when pdu.Length >= 1 =>
                $"读寄存器响应 | 字节数={pdu[0]} 数据={FormatRegs(pdu[1..])}",
            0x05 when pdu.Length >= 4 =>
                $"写单线圈 | 地址=0x{(pdu[0] << 8 | pdu[1]):X4} 值=0x{(pdu[2] << 8 | pdu[3]):X4}",
            0x06 when pdu.Length >= 4 =>
                $"写单寄存器 | 地址=0x{(pdu[0] << 8 | pdu[1]):X4} 值=0x{(pdu[2] << 8 | pdu[3]):X4}",
            0x0F when pdu.Length >= 5 =>
                $"写多线圈 | 起始=0x{(pdu[0] << 8 | pdu[1]):X4} 数量={pdu[2] << 8 | pdu[3]} 字节数={pdu[4]}",
            0x10 when pdu.Length >= 5 =>
                $"写多寄存器 | 起始=0x{(pdu[0] << 8 | pdu[1]):X4} 数量={pdu[2] << 8 | pdu[3]} 字节数={pdu[4]}",
            _ => $"从站={addr} FC=0x{fc:X2} PDU={PortMonitor.Core.Models.SerialEvent.ToHex(pdu)} 长度={frame.Length}"
        };
    }

    private static string FormatRegs(ReadOnlySpan<byte> data)
    {
        if (data.IsEmpty) return "";
        var parts = new List<string>(data.Length / 2 + 1);
        for (int i = 0; i + 1 < data.Length; i += 2)
            parts.Add($"0x{(data[i] << 8 | data[i + 1]):X4}");
        if (data.Length % 2 != 0)
            parts.Add($"{data[^1]:X2}?");
        return string.Join(' ', parts);
    }
}
