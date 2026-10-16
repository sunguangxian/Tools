using SerialTap.Core.Models;

namespace SerialTap.Core.Export;

public enum AutosaveFormat { Csv, Txt, Html }

/// <summary>Rotating autosave: split by size (MB) or time (minutes).</summary>
public sealed class AutosaveWriter : IDisposable
{
    private readonly object _gate = new();
    private string _dir = "";
    private AutosaveFormat _format = AutosaveFormat.Csv;
    private long _maxBytes;
    private TimeSpan _maxAge;
    private StreamWriter? _writer;
    private string _currentPath = "";
    private long _written;
    private DateTimeOffset _openedAt;
    private bool _enabled;

    public bool Enabled
    {
        get { lock (_gate) return _enabled; }
        set { lock (_gate) { if (!value) CloseWriter(); _enabled = value; } }
    }

    public void Configure(string directory, AutosaveFormat format, double maxMb, double maxMinutes)
    {
        lock (_gate)
        {
            CloseWriter();
            _dir = directory;
            _format = format;
            _maxBytes = maxMb > 0 ? (long)(maxMb * 1024 * 1024) : 0;
            _maxAge = maxMinutes > 0 ? TimeSpan.FromMinutes(maxMinutes) : TimeSpan.Zero;
        }
    }

    public void Write(SerialEvent ev)
    {
        lock (_gate)
        {
            if (!_enabled || string.IsNullOrWhiteSpace(_dir)) return;
            EnsureWriter(ev.Timestamp);
            if (_writer is null) return;
            switch (_format)
            {
                case AutosaveFormat.Txt:
                    _writer.WriteLine($"{ev.Sequence}\t{ev.SessionName}\t{ev.Timestamp:O}\t{ev.DirectionLabel}\t{ev.HexDump}");
                    break;
                case AutosaveFormat.Html:
                    // HTML autosave writes a simple line comment; full HTML is for batch export.
                    _writer.WriteLine($"<!-- {ev.Sequence} {ev.DirectionLabel} {ev.HexDump} -->");
                    break;
                default:
                    _writer.WriteLine($"{ev.Sequence},{Escape(ev.SessionName)},{ev.Timestamp:O},{ev.DirectionLabel},{ev.Length},{Escape(ev.HexDump)},{Escape(ev.AsciiPreview)}");
                    break;
            }
            _written += 64 + ev.Length * 3;
            if ((_maxBytes > 0 && _written >= _maxBytes) ||
                (_maxAge > TimeSpan.Zero && ev.Timestamp - _openedAt >= _maxAge))
                Rotate(ev.Timestamp);
        }
    }

    public void Dispose() { lock (_gate) CloseWriter(); }

    private void EnsureWriter(DateTimeOffset now)
    {
        if (_writer is not null) return;
        Directory.CreateDirectory(_dir);
        var ext = _format switch { AutosaveFormat.Txt => "txt", AutosaveFormat.Html => "html", _ => "csv" };
        _currentPath = Path.Combine(_dir, $"serialtap-auto-{now:yyyyMMdd-HHmmss}.{ext}");
        _writer = new StreamWriter(_currentPath, false, new System.Text.UTF8Encoding(true)) { AutoFlush = true };
        if (_format == AutosaveFormat.Csv)
            _writer.WriteLine("Sequence,Session,Timestamp,Direction,Length,Hex,Ascii");
        else if (_format == AutosaveFormat.Html)
            _writer.WriteLine("<!DOCTYPE html><html><body><pre>");
        _written = 0;
        _openedAt = now;
    }

    private void Rotate(DateTimeOffset now)
    {
        CloseWriter();
        EnsureWriter(now);
    }

    private void CloseWriter()
    {
        if (_writer is null) return;
        if (_format == AutosaveFormat.Html)
            _writer.WriteLine("</pre></body></html>");
        _writer.Dispose();
        _writer = null;
    }

    private static string Escape(string v) =>
        v.IndexOfAny(['"', ',', '\r', '\n']) < 0 ? v : "\"" + v.Replace("\"", "\"\"") + "\"";
}
