namespace SerialTap.Core.Usb;

public enum CswStatus { Pending = -1, Passed = 0, Failed = 1, PhaseError = 2 }

/// <summary>One BOT command: CBW → (data) → CSW.</summary>
public sealed class ScsiCommand
{
    public long Index { get; init; }
    public DateTimeOffset Timestamp { get; init; }
    public ushort Bus { get; init; }
    public ushort Device { get; init; }
    public uint Tag { get; init; }
    public byte Lun { get; init; }
    public bool DataIn { get; init; }
    public uint ExpectedLength { get; init; }
    public byte[] Cdb { get; init; } = Array.Empty<byte>();
    public CdbInfo Info { get; init; }

    public int DataBytes { get; internal set; }
    public byte[] DataSample { get; internal set; } = Array.Empty<byte>();
    public CswStatus Status { get; internal set; } = CswStatus.Pending;
    public uint Residue { get; internal set; }
    public DateTimeOffset? CompletedAt { get; internal set; }
    public string Response { get; internal set; } = "";
    /// <summary>Set when the CSW tag didn't match (or a new CBW arrived before CSW).</summary>
    public string Warning { get; internal set; } = "";

    public string DeviceLabel => $"{Bus}.{Device}";
    public string OpcodeName => Info.Name;
    public double? DurationMs => CompletedAt is { } c ? (c - Timestamp).TotalMilliseconds : null;
    public string StatusLabel => Status switch
    {
        CswStatus.Passed => "Passed",
        CswStatus.Failed => "Failed",
        CswStatus.PhaseError => "Phase Error",
        _ => "未完成"
    };

    public string Summary
    {
        get
        {
            var parts = new List<string>();
            if (!string.IsNullOrEmpty(Info.Detail)) parts.Add(Info.Detail);
            if (!string.IsNullOrEmpty(Response)) parts.Add(Response);
            if (!string.IsNullOrEmpty(Warning)) parts.Add("⚠ " + Warning);
            parts.Add($"{(DataIn ? "IN" : "OUT")} 期望 {ExpectedLength} B / 实际 {DataBytes} B");
            return string.Join(" | ", parts);
        }
    }
}
