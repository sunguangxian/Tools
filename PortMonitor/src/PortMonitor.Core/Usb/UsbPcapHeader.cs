using System.Buffers.Binary;

namespace PortMonitor.Core.Usb;

/// <summary>Parses the USBPcap per-packet header (LINKTYPE_USBPCAP = 249). All fields little-endian, packed.</summary>
public static class UsbPcapHeader
{
    public const int LinkType = 249;
    public const int BaseHeaderLength = 27;

    public static UsbPacket? Parse(ReadOnlySpan<byte> frame, DateTimeOffset ts, long index)
    {
        if (frame.Length < BaseHeaderLength) return null;
        ushort headerLen = BinaryPrimitives.ReadUInt16LittleEndian(frame);
        if (headerLen < BaseHeaderLength || headerLen > frame.Length) return null;

        var transfer = frame[22] <= 3 ? (UsbTransferType)frame[22] : UsbTransferType.Unknown;
        uint dataLen = BinaryPrimitives.ReadUInt32LittleEndian(frame[23..]);
        var payload = frame[headerLen..];
        if (payload.Length > dataLen) payload = payload[..(int)dataLen];

        return new UsbPacket
        {
            Index = index,
            Timestamp = ts,
            HeaderLength = headerLen,
            IrpId = BinaryPrimitives.ReadUInt64LittleEndian(frame[2..]),
            Status = BinaryPrimitives.ReadUInt32LittleEndian(frame[10..]),
            Function = BinaryPrimitives.ReadUInt16LittleEndian(frame[14..]),
            Info = frame[16],
            Bus = BinaryPrimitives.ReadUInt16LittleEndian(frame[17..]),
            Device = BinaryPrimitives.ReadUInt16LittleEndian(frame[19..]),
            Endpoint = frame[21],
            Transfer = transfer,
            DataLength = dataLen,
            Stage = transfer == UsbTransferType.Control && headerLen >= 28 ? frame[27] : null,
            Data = payload.ToArray()
        };
    }

    /// <summary>Builds a raw USBPcap frame (used by the sample generator / tests).</summary>
    public static byte[] Build(ulong irp, uint status, byte info, ushort bus, ushort device,
        byte endpoint, UsbTransferType transfer, ReadOnlySpan<byte> data, ushort function = 0x0009)
    {
        var buf = new byte[BaseHeaderLength + data.Length];
        var s = buf.AsSpan();
        BinaryPrimitives.WriteUInt16LittleEndian(s, BaseHeaderLength);
        BinaryPrimitives.WriteUInt64LittleEndian(s[2..], irp);
        BinaryPrimitives.WriteUInt32LittleEndian(s[10..], status);
        BinaryPrimitives.WriteUInt16LittleEndian(s[14..], function);
        s[16] = info;
        BinaryPrimitives.WriteUInt16LittleEndian(s[17..], bus);
        BinaryPrimitives.WriteUInt16LittleEndian(s[19..], device);
        s[21] = endpoint;
        s[22] = (byte)transfer;
        BinaryPrimitives.WriteUInt32LittleEndian(s[23..], (uint)data.Length);
        data.CopyTo(s[BaseHeaderLength..]);
        return buf;
    }
}
