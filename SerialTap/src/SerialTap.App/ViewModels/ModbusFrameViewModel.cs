using Avalonia.Media;
using Avalonia.Media.Immutable;
using SerialTap.Core.Models;

namespace SerialTap.App.ViewModels;

public sealed class ModbusFrameViewModel
{
    // Immutable (thread-safe) brushes: rows are created on the serial read thread.
    private static readonly IBrush OkBrush = new ImmutableSolidColorBrush(Color.Parse("#6A9955"));
    private static readonly IBrush BadBrush = new ImmutableSolidColorBrush(Color.Parse("#F44747"));

    public ModbusFrameViewModel(ModbusFrame f)
    {
        Sequence = f.Sequence;
        SessionId = f.SessionId;
        Session = f.SessionName;
        DirectionKind = f.Direction;
        Timestamp = f.Timestamp.ToString("HH:mm:ss.fff");
        Direction = f.DirectionLabel;
        Ports = string.IsNullOrEmpty(f.DestPort) ? f.SourcePort : $"{f.SourcePort} → {f.DestPort}";
        Slave = f.SlaveAddress.ToString();
        Function = f.FunctionLabel;
        Crc = f.CrcLabel;
        CrcOk = f.CrcOk;
        Summary = f.Summary;
        Hex = f.HexDump;
        Length = f.Raw.Length;
        Protocol = f.IsAscii ? "ASCII" : "RTU";
        CrcBrush = f.CrcOk ? OkBrush : BadBrush;
    }

    public long Sequence { get; }
    public int SessionId { get; }
    public string Session { get; }
    public SerialTap.Core.Models.SerialDirection DirectionKind { get; }
    public string Timestamp { get; }
    public string Direction { get; }
    public string Ports { get; }
    public string Slave { get; }
    public string Function { get; }
    public string Crc { get; }
    public bool CrcOk { get; }
    public string Summary { get; }
    public string Hex { get; }
    public int Length { get; }
    public string Protocol { get; }
    public IBrush CrcBrush { get; }
}
