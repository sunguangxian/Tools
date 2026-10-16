using PortMonitor.Core.Models;

namespace PortMonitor.Core.Protocol;

/// <summary>
/// Per-direction Modbus RTU framer.
/// Accumulates bytes; peels frames with valid CRC at function-code–plausible lengths;
/// on silence gap (≈3.5 character times), flushes remainder (CRC scan, then bad frame).
/// </summary>
public sealed class ModbusRtuStreamParser
{
    private readonly Dictionary<string, Channel> _channels = new(StringComparer.Ordinal);
    private readonly object _gate = new();
    private readonly byte[] _scratch = new byte[256];
    private long _seq;
    private TimeSpan _silenceGap;

    public ModbusRtuStreamParser(int baudRate = 115200) => SetBaudRate(baudRate);

    public TimeSpan SilenceGap => _silenceGap;

    public void SetBaudRate(int baudRate)
    {
        double charMs = baudRate <= 0 ? 1.0 : (11.0 * 1000.0) / baudRate;
        double gapMs = Math.Clamp(charMs * 3.5, 3.0, 50.0);
        _silenceGap = TimeSpan.FromMilliseconds(gapMs);
    }

    public IReadOnlyList<ModbusFrame> Push(SerialEvent ev)
    {
        lock (_gate)
        {
            var key = $"{ev.SessionId}|{ev.Direction}|{ev.SourcePort}|{ev.DestPort}";
            if (!_channels.TryGetValue(key, out var ch))
            {
                ch = new Channel();
                _channels[key] = ch;
            }

            var result = new List<ModbusFrame>();

            if (ch.Buffer.Count > 0 && ev.Timestamp - ch.LastByteTime > _silenceGap)
                FlushIncomplete(ch, result);

            ch.LastDirection = ev.Direction;
            ch.SessionId = ev.SessionId;
            ch.SessionName = ev.SessionName;
            ch.LastSource = ev.SourcePort;
            ch.LastDest = ev.DestPort;
            ch.Buffer.AddRange(ev.Data);
            ch.LastByteTime = ev.Timestamp;
            if (ch.FrameStart == default)
                ch.FrameStart = ev.Timestamp;

            Peel(ch, result, scanAllLengths: false);
            return result;
        }
    }

    public IReadOnlyList<ModbusFrame> PollSilence(DateTimeOffset now)
    {
        lock (_gate)
        {
            var result = new List<ModbusFrame>();
            foreach (var ch in _channels.Values)
            {
                if (ch.Buffer.Count == 0) continue;
                if (now - ch.LastByteTime <= _silenceGap) continue;
                FlushIncomplete(ch, result);
            }
            return result;
        }
    }

    public void Reset()
    {
        lock (_gate) _channels.Clear();
    }

    private void Peel(Channel ch, List<ModbusFrame> output, bool scanAllLengths)
    {
        const int maxFrame = 256;
        while (ch.Buffer.Count >= 4)
        {
            int limit = Math.Min(ch.Buffer.Count, maxFrame);
            int foundLen = -1;

            foreach (var candidate in CandidateLengths(ch.Buffer, limit, scanAllLengths))
            {
                CopyToScratch(ch.Buffer, candidate);
                if (ModbusCrc16.ValidateFrame(_scratch.AsSpan(0, candidate)))
                {
                    foundLen = candidate;
                    break;
                }
            }

            if (foundLen < 0)
                break;

            Emit(ch, foundLen, output);
        }
    }

    private void FlushIncomplete(Channel ch, List<ModbusFrame> output)
    {
        if (ch.Buffer.Count == 0) return;
        Peel(ch, output, scanAllLengths: true);
        if (ch.Buffer.Count == 0) return;
        Emit(ch, ch.Buffer.Count, output);
    }

    private void Emit(Channel ch, int len, List<ModbusFrame> output)
    {
        var raw = new byte[len];
        ch.Buffer.CopyTo(0, raw, 0, len);
        ch.Buffer.RemoveRange(0, len);

        var parsed = ModbusRtuParser.TryParse(raw);
        output.Add(new ModbusFrame
        {
            Sequence = Interlocked.Increment(ref _seq),
            SessionId = ch.SessionId,
            SessionName = ch.SessionName,
            Timestamp = ch.FrameStart == default ? DateTimeOffset.Now : ch.FrameStart,
            Direction = ch.LastDirection,
            SourcePort = ch.LastSource,
            DestPort = ch.LastDest,
            Raw = raw,
            CrcOk = parsed.CrcOk,
            SlaveAddress = parsed.SlaveAddress,
            FunctionCode = parsed.FunctionCode,
            IsException = parsed.IsException,
            Summary = parsed.Summary
        });

        ch.FrameStart = ch.Buffer.Count > 0 ? ch.LastByteTime : default;
    }

    private void CopyToScratch(List<byte> buf, int len)
    {
        for (int i = 0; i < len; i++)
            _scratch[i] = buf[i];
    }

    /// <summary>
    /// Yield plausible frame lengths. Requests (esp. FC01–04 = 8 bytes) before
    /// response sizes, so start-address high byte is not mistaken for byte count.
    /// </summary>
    private static IEnumerable<int> CandidateLengths(List<byte> buf, int limit, bool scanAll)
    {
        var preferred = new List<int>();
        void Pref(int n)
        {
            if (n >= 4 && n <= limit && !preferred.Contains(n))
                preferred.Add(n);
        }

        if (buf.Count >= 2)
        {
            byte fcRaw = buf[1];
            if ((fcRaw & 0x80) != 0)
            {
                Pref(5);
            }
            else
            {
                byte fc = (byte)(fcRaw & 0x7F);
                switch (fc)
                {
                    case 0x01:
                    case 0x02:
                        Pref(8);
                        if (buf.Count >= 3)
                        {
                            int bc = buf[2];
                            if (bc is > 0 and <= 250)
                                Pref(5 + bc);
                        }
                        break;
                    case 0x03:
                    case 0x04:
                        Pref(8);
                        if (buf.Count >= 3)
                        {
                            int bc = buf[2];
                            // Response byte count must be even and non-zero
                            if (bc is > 0 and <= 250 && (bc % 2) == 0)
                                Pref(5 + bc);
                        }
                        break;
                    case 0x05:
                    case 0x06:
                        Pref(8);
                        break;
                    case 0x0F:
                    case 0x10:
                        Pref(8);
                        if (buf.Count >= 7)
                        {
                            int bc = buf[6];
                            if (bc is >= 0 and <= 246)
                                Pref(9 + bc);
                        }
                        break;
                }
            }
        }

        foreach (var n in preferred)
            yield return n;

        if (!scanAll)
            yield break;

        // Longest-first reduces short false-positive CRC hits on silence flush
        for (int n = limit; n >= 4; n--)
        {
            if (!preferred.Contains(n))
                yield return n;
        }
    }

    private sealed class Channel
    {
        public List<byte> Buffer { get; } = new(256);
        public DateTimeOffset LastByteTime { get; set; }
        public DateTimeOffset FrameStart { get; set; }
        public SerialDirection LastDirection { get; set; }
        public string LastSource { get; set; } = "";
        public string LastDest { get; set; } = "";
        public int SessionId { get; set; }
        public string SessionName { get; set; } = "";
    }
}
