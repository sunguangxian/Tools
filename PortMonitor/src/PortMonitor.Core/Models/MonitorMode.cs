namespace PortMonitor.Core.Models;

public enum MonitorMode
{
    /// <summary>
    /// Bidirectional copy between two COM ports (e.g. com0com pair).
    /// Supported path for observing traffic without a kernel filter.
    /// </summary>
    Bridge,

    /// <summary>
    /// Open a single port for raw listen. Cannot monitor a port already
    /// exclusively opened by another process — that needs a kernel filter (out of scope).
    /// </summary>
    SinglePort
}
