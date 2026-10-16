namespace SscomAvalonia.Models;

public sealed class AppSettings
{
    public string PortName { get; set; } = "";
    public int BaudRate { get; set; } = 115200;
    public int DataBits { get; set; } = 8;
    public int StopBits { get; set; } = 1;
    public int Parity { get; set; } = 0;
    public int FlowControl { get; set; } = 0;
    public bool Dtr { get; set; }
    public bool Rts { get; set; }

    public bool HexDisplay { get; set; }
    public bool HexSend { get; set; }
    public bool ShowTimestamp { get; set; }
    public bool AppendNewline { get; set; }
    public bool AutoSend { get; set; }
    public int AutoSendIntervalMs { get; set; } = 1000;
    public int PacketTimeoutMs { get; set; } = 20;
    public bool SaveData { get; set; }
    public bool RecvToFile { get; set; }
    public int ChecksumMode { get; set; }
    public bool StayOnTop { get; set; }
    public bool SaveParamsOnExit { get; set; } = true;
    public bool ExtensionVisible { get; set; }
    public string? SendDraft { get; set; }

    public List<string> Presets { get; set; } = new();
    public List<bool> PresetHexFlags { get; set; } = new();
}
