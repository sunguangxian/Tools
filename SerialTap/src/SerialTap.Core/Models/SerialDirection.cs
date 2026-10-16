namespace SerialTap.Core.Models;

/// <summary>Traffic direction relative to the bridge or single-port sniffer.</summary>
public enum SerialDirection
{
    /// <summary>Port A → Port B (device toward host app).</summary>
    AToB,

    /// <summary>Port B → Port A (host app toward device).</summary>
    BToA,

    /// <summary>Single-port sniffer: bytes received on the monitored port.</summary>
    Receive,

    /// <summary>Single-port sniffer: bytes written (if write path is used).</summary>
    Transmit
}
