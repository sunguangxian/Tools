namespace SerialTap.Core.Models;

/// <summary>Serial line settings shared by both ends of a bridge.</summary>
public sealed class SerialPortSettings
{
    public int BaudRate { get; set; } = 115200;
    public int DataBits { get; set; } = 8;
    public ParityKind Parity { get; set; } = ParityKind.None;
    public StopBitsKind StopBits { get; set; } = StopBitsKind.One;
}

public enum ParityKind
{
    None,
    Odd,
    Even,
    Mark,
    Space
}

public enum StopBitsKind
{
    One,
    OnePointFive,
    Two
}
