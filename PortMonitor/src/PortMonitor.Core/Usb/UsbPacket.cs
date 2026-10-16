namespace PortMonitor.Core.Usb;

public enum UsbTransferType : byte { Isochronous = 0, Interrupt = 1, Control = 2, Bulk = 3, Unknown = 0xFF }

/// <summary>One USBPcap record (USBPCAP_BUFFER_PACKET_HEADER + payload).</summary>
public sealed class UsbPacket
{
    public long Index { get; init; }
    public DateTimeOffset Timestamp { get; init; }
    public ushort HeaderLength { get; init; }
    public ulong IrpId { get; init; }
    public uint Status { get; init; }
    public ushort Function { get; init; }
    public byte Info { get; init; }
    public ushort Bus { get; init; }
    public ushort Device { get; init; }
    public byte Endpoint { get; init; }
    public UsbTransferType Transfer { get; init; }
    public uint DataLength { get; init; }
    /// <summary>Control stage (0 setup, 1 data, 2 status, 3 complete) — control transfers only.</summary>
    public byte? Stage { get; init; }
    public byte[] Data { get; init; } = Array.Empty<byte>();

    /// <summary>info bit0: 1 = PDO→FDO (completion / device→host leg).</summary>
    public bool IsCompletion => (Info & 0x01) != 0;
    public bool IsInEndpoint => (Endpoint & 0x80) != 0;
    public int EndpointNumber => Endpoint & 0x0F;
    public string Address => $"{Bus}.{Device}.{EndpointNumber}";
    public string DirectionLabel => IsInEndpoint ? "IN" : "OUT";
    public string StageLabel => IsCompletion ? "完成" : "提交";
    public bool StatusOk => Status == 0;
    public string StatusLabel => Status == 0 ? "OK" : $"0x{Status:X8}";
    public string TransferLabel => Transfer switch
    {
        UsbTransferType.Isochronous => "ISO",
        UsbTransferType.Interrupt => "INT",
        UsbTransferType.Control => "CTRL",
        UsbTransferType.Bulk => "BULK",
        _ => "?"
    };
}
