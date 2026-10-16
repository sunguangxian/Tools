using System.Collections.ObjectModel;
using Avalonia.Media;
using Avalonia.Media.Immutable;
using CommunityToolkit.Mvvm.ComponentModel;
using SerialTap.Core.Models;
using SerialTap.Core.Protocol;

namespace SerialTap.App.ViewModels;

public sealed class TreeNode : ObservableObject
{
    public TreeNode(string title, string subtitle = "", string dot = "#858585")
    {
        Title = title; Subtitle = subtitle; DotColor = Color.Parse(dot);
    }
    public string Title { get; set; }
    public string Subtitle { get; set; }
    public Color DotColor { get; set; }
    public object? Tag { get; set; }
    public ObservableCollection<TreeNode> Children { get; } = new();
}

public sealed class LineRowViewModel
{
    public LineRowViewModel(MergedLine line, TextEncodingKind enc, bool escape)
    {
        Sequence = line.FirstSequence;
        SessionId = line.SessionId;
        Session = line.SessionName;
        DirectionKind = line.Direction;
        Timestamp = line.Start.ToString("HH:mm:ss.fff");
        Direction = line.Direction switch
        {
            SerialDirection.AToB => "A→B",
            SerialDirection.BToA => "B→A",
            SerialDirection.Receive => "RX",
            SerialDirection.Transmit => "TX",
            _ => "?"
        };
        Length = line.Data.Length;
        Hex = SerialEvent.ToHex(line.Data);
        Text = TextDecoder.Decode(enc, line.Data, escape, keepNewlines: false);
        Data = line.Data;
    }
    public long Sequence { get; }
    public int SessionId { get; }
    public string Session { get; }
    public SerialDirection DirectionKind { get; }
    public string Timestamp { get; }
    public string Direction { get; }
    public int Length { get; }
    public string Hex { get; }
    public string Text { get; }
    public byte[] Data { get; }
}

public sealed class PortEventRow(DateTimeOffset t, string session, string text)
{
    public string Display { get; } = $"{t:HH:mm:ss.fff}  [{session}]  {text}";
}

public sealed class UsbTreeNode
{
    public string Title { get; init; } = "";
    public ObservableCollection<UsbTreeNode> Children { get; } = new();
}

public sealed record RadixChoice(string Id, string Display)
{
    public override string ToString() => Display;
}

public sealed record EncodingChoice(TextEncodingKind Kind, string Display)
{
    public override string ToString() => Display;
}

public sealed record ModbusModeChoice(string Id, string Display)
{
    public override string ToString() => Display;
}
