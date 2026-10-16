namespace SerialTap.Core.Protocol;

/// <summary>
/// Modbus RTU CRC-16 (poly 0xA001, init 0xFFFF).
/// Extension point for a future Modbus frame view.
/// </summary>
public static class ModbusCrc16
{
    public static ushort Compute(ReadOnlySpan<byte> data)
    {
        ushort crc = 0xFFFF;
        for (int i = 0; i < data.Length; i++)
        {
            crc ^= data[i];
            for (int b = 0; b < 8; b++)
            {
                bool lsb = (crc & 1) != 0;
                crc >>= 1;
                if (lsb) crc ^= 0xA001;
            }
        }
        return crc;
    }

    public static bool ValidateFrame(ReadOnlySpan<byte> frameWithCrc)
    {
        if (frameWithCrc.Length < 3) return false;
        var payload = frameWithCrc[..^2];
        ushort expected = Compute(payload);
        ushort actual = (ushort)(frameWithCrc[^2] | (frameWithCrc[^1] << 8));
        return expected == actual;
    }
}
