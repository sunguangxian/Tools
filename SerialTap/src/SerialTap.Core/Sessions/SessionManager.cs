using SerialTap.Core.Models;

namespace SerialTap.Core.Sessions;

/// <summary>
/// Holds N independent sessions. All sessions feed one shared event stream.
/// Guarantees a physical port is opened by at most one session at a time.
/// </summary>
public sealed class SessionManager : IAsyncDisposable
{
    private readonly object _gate = new();
    private readonly List<MonitorSession> _sessions = new();
    private int _nextId = 1;

    public event EventHandler<SerialEvent>? DataCaptured;
    public event EventHandler<SessionErrorEventArgs>? ErrorOccurred;
    public event EventHandler? SessionsChanged;

    public IReadOnlyList<MonitorSession> Sessions
    {
        get { lock (_gate) return _sessions.ToList(); }
    }

    public MonitorSession Add(MonitorMode mode, string portA, string? portB, SerialPortSettings settings, string? name = null)
    {
        if (string.IsNullOrWhiteSpace(portA))
            throw new ArgumentException("必须选择端口 A");
        if (mode == MonitorMode.Bridge)
        {
            if (string.IsNullOrWhiteSpace(portB))
                throw new ArgumentException("桥接模式必须选择端口 B");
            if (string.Equals(portA, portB, StringComparison.OrdinalIgnoreCase))
                throw new ArgumentException("端口 A 与端口 B 不能相同");
        }

        MonitorSession s;
        lock (_gate)
        {
            int id = _nextId++;
            s = new MonitorSession(id, name ?? $"S{id}", mode, portA, mode == MonitorMode.Bridge ? portB : null, settings);
            s.Bridge.DataCaptured += (_, ev) => DataCaptured?.Invoke(this, ev);
            s.Bridge.ErrorOccurred += (_, msg) =>
            {
                s.LastError = msg;
                ErrorOccurred?.Invoke(this, new SessionErrorEventArgs(s, msg));
            };
            _sessions.Add(s);
        }
        SessionsChanged?.Invoke(this, EventArgs.Empty);
        return s;
    }

    /// <summary>Returns the open session that already holds <paramref name="port"/>, if any.</summary>
    public MonitorSession? FindPortOwner(string port, int exceptSessionId = 0)
    {
        lock (_gate)
        {
            return _sessions.FirstOrDefault(s => s.Id != exceptSessionId && s.IsOpen &&
                s.Ports.Any(p => string.Equals(p, port, StringComparison.OrdinalIgnoreCase)));
        }
    }

    public async Task OpenAsync(int id)
    {
        var s = Get(id) ?? throw new InvalidOperationException($"会话 {id} 不存在");
        if (s.IsOpen) return;

        foreach (var port in s.Ports)
        {
            var owner = FindPortOwner(port, s.Id);
            if (owner is not null)
            {
                var msg = $"端口 {port} 已被会话「{owner.Name}」占用，请先关闭该会话";
                s.LastError = msg;
                throw new InvalidOperationException(msg);
            }
        }

        try
        {
            s.LastError = null;
            if (s.Mode == MonitorMode.Bridge)
                await s.Bridge.OpenBridgeAsync(s.PortA, s.PortB!, s.Settings);
            else
                await s.Bridge.OpenSingleAsync(s.PortA, s.Settings);
        }
        catch (Exception ex)
        {
            s.LastError = ex.Message;
            throw;
        }
        finally
        {
            SessionsChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public async Task CloseAsync(int id)
    {
        var s = Get(id);
        if (s is null) return;
        await s.Bridge.CloseAsync();
        SessionsChanged?.Invoke(this, EventArgs.Empty);
    }

    public async Task RemoveAsync(int id)
    {
        var s = Get(id);
        if (s is null) return;
        await s.Bridge.CloseAsync();
        lock (_gate) _sessions.Remove(s);
        SessionsChanged?.Invoke(this, EventArgs.Empty);
    }

    public async Task CloseAllAsync()
    {
        foreach (var s in Sessions)
            await s.Bridge.CloseAsync();
        SessionsChanged?.Invoke(this, EventArgs.Empty);
    }

    public MonitorSession? Get(int id)
    {
        lock (_gate) return _sessions.FirstOrDefault(s => s.Id == id);
    }

    public async ValueTask DisposeAsync()
    {
        foreach (var s in Sessions)
            await s.Bridge.DisposeAsync();
    }
}

public sealed class SessionErrorEventArgs(MonitorSession session, string message) : EventArgs
{
    public MonitorSession Session { get; } = session;
    public string Message { get; } = message;
}
