using System.Buffers.Binary;

namespace PortMonitor.Core.Usb;

/// <summary>
/// USB Mass Storage Bulk-Only Transport correlator.
/// CBW ('USBC', 31 B) on bulk OUT → optional data phase → CSW ('USBS', 13 B) on bulk IN; matched by tag.
/// BOT is strictly sequential per device, so one pending command per bus.device is tracked.
/// UAS (USB Attached SCSI) is not decoded.
/// </summary>
public sealed class MassStorageDecoder
{
    private const uint CbwSig = 0x43425355; // "USBC" little-endian
    private const uint CswSig = 0x53425355; // "USBS"
    private const int SampleCap = 512;

    private readonly Dictionary<(ushort, ushort), ScsiCommand> _pending = new();
    private long _index;

    /// <summary>Raised when a new CBW is seen (row can be shown as pending).</summary>
    public event Action<ScsiCommand>? CommandStarted;

    /// <summary>Feed one USB packet; returns a command when its CSW completes it.</summary>
    public ScsiCommand? Push(UsbPacket p)
    {
        if (p.Transfer != UsbTransferType.Bulk || p.Data.Length == 0) return null;

        // Bulk OUT payload is carried by the submission leg, bulk IN payload by the completion leg.
        bool carriesData = p.IsInEndpoint ? p.IsCompletion : !p.IsCompletion;
        if (!carriesData) return null;

        var key = (p.Bus, p.Device);
        var d = p.Data;

        if (!p.IsInEndpoint && d.Length == 31 && BinaryPrimitives.ReadUInt32LittleEndian(d) == CbwSig)
        {
            if (_pending.TryGetValue(key, out var stale) && stale.Status == CswStatus.Pending)
                stale.Warning = "未收到 CSW 即开始新命令";

            int cbLen = Math.Clamp(d[14] & 0x1F, 1, 16);
            var cdb = d.AsSpan(15, cbLen).ToArray();
            var cmd = new ScsiCommand
            {
                Index = ++_index,
                Timestamp = p.Timestamp,
                Bus = p.Bus,
                Device = p.Device,
                Tag = BinaryPrimitives.ReadUInt32LittleEndian(d.AsSpan(4)),
                ExpectedLength = BinaryPrimitives.ReadUInt32LittleEndian(d.AsSpan(8)),
                DataIn = (d[12] & 0x80) != 0,
                Lun = (byte)(d[13] & 0x0F),
                Cdb = cdb,
                Info = ScsiDecoder.DecodeCdb(cdb)
            };
            _pending[key] = cmd;
            CommandStarted?.Invoke(cmd);
            return null;
        }

        if (p.IsInEndpoint && d.Length == 13 && BinaryPrimitives.ReadUInt32LittleEndian(d) == CswSig
            && _pending.TryGetValue(key, out var cur))
        {
            uint tag = BinaryPrimitives.ReadUInt32LittleEndian(d.AsSpan(4));
            if (tag != cur.Tag) cur.Warning = $"CSW 标签不匹配 (0x{tag:X8})";
            cur.Residue = BinaryPrimitives.ReadUInt32LittleEndian(d.AsSpan(8));
            cur.Status = d[12] switch { 0 => CswStatus.Passed, 1 => CswStatus.Failed, _ => CswStatus.PhaseError };
            cur.CompletedAt = p.Timestamp;
            cur.Response = ScsiDecoder.DecodeResponse(cur.Info.Opcode, cur.Cdb, cur.DataSample) ?? "";
            _pending.Remove(key);
            return cur;
        }

        // Data phase
        if (_pending.TryGetValue(key, out var c) && c.Status == CswStatus.Pending && p.IsInEndpoint == c.DataIn)
        {
            if (c.DataSample.Length < SampleCap)
            {
                int take = Math.Min(SampleCap - c.DataSample.Length, d.Length);
                c.DataSample = [.. c.DataSample, .. d.AsSpan(0, take)];
            }
            c.DataBytes += d.Length;
        }
        return null;
    }

    public void Reset()
    {
        _pending.Clear();
        _index = 0;
    }
}
