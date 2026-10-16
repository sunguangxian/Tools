using PortMonitor.Core.Models;

namespace PortMonitor.Core.Protocol;

public sealed class LineOptions
{
    public TimeSpan Gap { get; set; } = TimeSpan.FromMilliseconds(50);
    public bool SplitOnDirection { get; set; } = true;
    public bool SplitOnNewline { get; set; } = true;
    public int MaxBytes { get; set; } = 256;
}

public sealed class MergedLine
{
    public long FirstSequence { get; init; }
    public long LastSequence { get; init; }
    public DateTimeOffset Start { get; init; }
    public DateTimeOffset End { get; init; }
    public int SessionId { get; init; }
    public string SessionName { get; init; } = "";
    public SerialDirection Direction { get; init; }
    public string SourcePort { get; init; } = "";
    public string DestPort { get; init; } = "";
    public byte[] Data { get; init; } = Array.Empty<byte>();
}

/// <summary>Merges serial chunks into line view rows (gap / direction / newline / max length).</summary>
public sealed class LineMerger
{
    private readonly Dictionary<int, Builder> _cur = new();
    private readonly object _gate = new();
    public LineOptions Options { get; } = new();

    public IReadOnlyList<MergedLine> Push(SerialEvent ev)
    {
        lock (_gate)
        {
            var closed = new List<MergedLine>();
            if (_cur.TryGetValue(ev.SessionId, out var b))
            {
                if ((Options.SplitOnDirection && b.Direction != ev.Direction) ||
                    ev.Timestamp - b.End > Options.Gap)
                {
                    closed.Add(b.Finish());
                    _cur.Remove(ev.SessionId);
                }
            }

            foreach (var byteVal in ev.Data)
            {
                if (!_cur.TryGetValue(ev.SessionId, out b))
                {
                    b = new Builder
                    {
                        FirstSequence = ev.Sequence,
                        Start = ev.Timestamp,
                        SessionId = ev.SessionId,
                        SessionName = ev.SessionName,
                        Direction = ev.Direction,
                        SourcePort = ev.SourcePort,
                        DestPort = ev.DestPort
                    };
                    _cur[ev.SessionId] = b;
                }
                b.Bytes.Add(byteVal);
                b.LastSequence = ev.Sequence;
                b.End = ev.Timestamp;
                if ((Options.SplitOnNewline && byteVal == (byte)'\n') ||
                    (Options.MaxBytes > 0 && b.Bytes.Count >= Options.MaxBytes))
                {
                    closed.Add(b.Finish());
                    _cur.Remove(ev.SessionId);
                }
            }
            return closed;
        }
    }

    public IReadOnlyList<MergedLine> FlushIdle(DateTimeOffset now)
    {
        lock (_gate)
        {
            var closed = new List<MergedLine>();
            foreach (var id in _cur.Keys.ToList())
            {
                var b = _cur[id];
                if (now - b.End > Options.Gap)
                {
                    closed.Add(b.Finish());
                    _cur.Remove(id);
                }
            }
            return closed;
        }
    }

    public IReadOnlyList<MergedLine> FlushAll()
    {
        lock (_gate)
        {
            var closed = _cur.Values.Select(b => b.Finish()).ToList();
            _cur.Clear();
            return closed;
        }
    }

    public void Reset()
    {
        lock (_gate) _cur.Clear();
    }

    private sealed class Builder
    {
        public long FirstSequence, LastSequence;
        public DateTimeOffset Start, End;
        public int SessionId;
        public string SessionName = "";
        public SerialDirection Direction;
        public string SourcePort = "", DestPort = "";
        public List<byte> Bytes { get; } = new();
        public MergedLine Finish() => new()
        {
            FirstSequence = FirstSequence,
            LastSequence = LastSequence,
            Start = Start,
            End = End,
            SessionId = SessionId,
            SessionName = SessionName,
            Direction = Direction,
            SourcePort = SourcePort,
            DestPort = DestPort,
            Data = Bytes.ToArray()
        };
    }
}
