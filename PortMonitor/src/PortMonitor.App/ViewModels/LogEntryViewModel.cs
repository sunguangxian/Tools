using PortMonitor.Core.Models;
using PortMonitor.Core.Protocol;

namespace PortMonitor.App.ViewModels;

/// <summary>UI-facing row for the live log list.</summary>
public sealed class LogEntryViewModel
{
    public LogEntryViewModel(SerialEvent ev, TextEncodingKind enc = TextEncodingKind.Ascii, bool escape = false, string radix = "HEX")
    {
        Sequence = ev.Sequence;
        SessionId = ev.SessionId;
        Session = ev.SessionName;
        DirectionKind = ev.Direction;
        Timestamp = ev.Timestamp.ToString("HH:mm:ss.fff");
        Direction = ev.DirectionLabel;
        Ports = string.IsNullOrEmpty(ev.DestPort)
            ? ev.SourcePort
            : $"{ev.SourcePort} → {ev.DestPort}";
        Length = ev.Length;
        Hex = ev.HexDump;
        Ascii = ev.AsciiPreview;
        Event = ev;
        ApplyDisplay(enc, escape, radix);
    }

    public void ApplyDisplay(TextEncodingKind enc, bool escape, string radix)
    {
        DisplayData = FormatRadix(Event.Data, radix);
        DisplayText = TextDecoder.Decode(enc, Event.Data, escape, keepNewlines: false);
    }

    public long Sequence { get; }
    public int SessionId { get; }
    public string Session { get; }
    public SerialDirection DirectionKind { get; }
    public string Timestamp { get; }
    public string Direction { get; }
    public string Ports { get; }
    public int Length { get; }
    public string Hex { get; }
    public string Ascii { get; }
    public string DisplayData { get; private set; } = "";
    public string DisplayText { get; private set; } = "";
    public SerialEvent Event { get; }

    private static string FormatRadix(byte[] data, string radix)
    {
        if (data.Length == 0) return "";
        return radix.ToUpperInvariant() switch
        {
            "DEC" => string.Join(' ', data.Select(b => b.ToString())),
            "OCT" => string.Join(' ', data.Select(b => Convert.ToString(b, 8).PadLeft(3, '0'))),
            "BIN" => string.Join(' ', data.Select(b => Convert.ToString(b, 2).PadLeft(8, '0'))),
            _ => SerialEvent.ToHex(data)
        };
    }
}
