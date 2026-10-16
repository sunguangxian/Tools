using System.Collections;

namespace SerialTap.Core.Models;

/// <summary>
/// Bounded ring of <see cref="SerialEvent"/>. Evicts oldest when over capacity
/// (by event count or approximate byte budget).
/// </summary>
public sealed class RingBuffer : IReadOnlyList<SerialEvent>
{
    private readonly LinkedList<SerialEvent> _items = new();
    private readonly object _gate = new();
    private readonly int _maxEvents;
    private readonly long _maxBytes;
    private long _byteCount;

    public RingBuffer(int maxEvents = 50_000, long maxBytes = 50L * 1024 * 1024)
    {
        if (maxEvents < 1) throw new ArgumentOutOfRangeException(nameof(maxEvents));
        if (maxBytes < 1024) throw new ArgumentOutOfRangeException(nameof(maxBytes));
        _maxEvents = maxEvents;
        _maxBytes = maxBytes;
    }

    public int Count
    {
        get { lock (_gate) return _items.Count; }
    }

    public long ByteCount
    {
        get { lock (_gate) return _byteCount; }
    }

    public SerialEvent this[int index]
    {
        get
        {
            lock (_gate)
            {
                if (index < 0 || index >= _items.Count)
                    throw new ArgumentOutOfRangeException(nameof(index));
                var n = _items.First!;
                for (int i = 0; i < index; i++)
                    n = n.Next!;
                return n.Value;
            }
        }
    }

    /// <summary>Adds an event; returns how many were evicted.</summary>
    public int Add(SerialEvent ev)
    {
        lock (_gate)
        {
            _items.AddLast(ev);
            _byteCount += ev.Length + 64; // payload + rough overhead
            int evicted = 0;
            while (_items.Count > _maxEvents || _byteCount > _maxBytes)
            {
                var old = _items.First!.Value;
                _items.RemoveFirst();
                _byteCount -= old.Length + 64;
                if (_byteCount < 0) _byteCount = 0;
                evicted++;
            }
            return evicted;
        }
    }

    public void Clear()
    {
        lock (_gate)
        {
            _items.Clear();
            _byteCount = 0;
        }
    }

    public List<SerialEvent> Snapshot()
    {
        lock (_gate)
            return _items.ToList();
    }

    public IEnumerator<SerialEvent> GetEnumerator() => Snapshot().GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}
