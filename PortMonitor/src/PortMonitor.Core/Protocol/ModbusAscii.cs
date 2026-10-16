using System.Text;
using PortMonitor.Core.Models;

namespace PortMonitor.Core.Protocol;

/// <summary>Modbus ASCII framing (: … CR LF) with LRC check.</summary>
public static class ModbusLrc
{
    public static byte Compute(ReadOnlySpan<byte> data)
    {
        byte s = 0;
        foreach (var b in data) s += b;
        return (byte)(0 - s);
    }
}

public sealed class ModbusAsciiStreamParser
{
    private readonly Dictionary<string, Channel> _channels = new(StringComparer.Ordinal);
    private readonly object _gate = new();
    private long _seq;

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
            foreach (var b in ev.Data)
            {
                char c = (char)b;
                if (c == ':')
                {
                    ch.Buf.Clear();
                    ch.Buf.Append(':');
                    ch.InFrame = true;
                    ch.Start = ev.Timestamp;
                    ch.SessionId = ev.SessionId;
                    ch.SessionName = ev.SessionName;
                    ch.Direction = ev.Direction;
                    ch.Source = ev.SourcePort;
                    ch.Dest = ev.DestPort;
                    continue;
                }
                if (!ch.InFrame) continue;
                ch.Buf.Append(c);
                if (c == '\n' && ch.Buf.Length >= 2 && ch.Buf[^2] == '\r')
                {
                    if (TryDecode(ch.Buf.ToString(), out var frame))
                    {
                        result.Add(new ModbusFrame
                        {
                            Sequence = ++_seq,
                            SessionId = ch.SessionId,
                            SessionName = ch.SessionName,
                            Timestamp = ch.Start,
                            Direction = ch.Direction,
                            SourcePort = ch.Source,
                            DestPort = ch.Dest,
                            Raw = frame.Raw,
                            CrcOk = frame.LrcOk,
                            SlaveAddress = frame.Slave,
                            FunctionCode = frame.Fc,
                            IsException = frame.IsException,
                            Summary = frame.Summary,
                            IsAscii = true
                        });
                    }
                    ch.Buf.Clear();
                    ch.InFrame = false;
                }
                else if (ch.Buf.Length > 600)
                {
                    ch.Buf.Clear();
                    ch.InFrame = false;
                }
            }
            return result;
        }
    }

    public void Reset()
    {
        lock (_gate) _channels.Clear();
    }

    private static bool TryDecode(string text, out (byte[] Raw, bool LrcOk, byte Slave, byte Fc, bool IsException, string Summary) frame)
    {
        frame = default;
        var t = text.TrimEnd('\r', '\n');
        if (t.Length == 0 || t[0] != ':') return false;
        var body = t[1..];
        if (body.Length % 2 != 0)
        {
            frame = (Encoding.ASCII.GetBytes(t), false, 0, 0, false, $"ASCII 帧字符数为奇数 | 长度={t.Length}");
            return true;
        }
        var bytes = new List<byte>(body.Length / 2);
        for (int i = 0; i < body.Length; i += 2)
        {
            if (!IsHex(body[i]) || !IsHex(body[i + 1]))
            {
                frame = (Encoding.ASCII.GetBytes(t), false, 0, 0, false, "ASCII 帧含非十六进制字符");
                return true;
            }
            bytes.Add((byte)((Hex(body[i]) << 4) | Hex(body[i + 1])));
        }
        if (bytes.Count < 3)
        {
            frame = (bytes.ToArray(), false, 0, 0, false, "ASCII 帧太短");
            return true;
        }
        var raw = bytes.ToArray();
        bool lrcOk = ModbusLrc.Compute(raw.AsSpan(0, raw.Length - 1)) == raw[^1];
        // Reuse RTU field decoder on ADU without LRC + synthetic CRC.
        var adu = raw.AsSpan(0, raw.Length - 1).ToArray();
        var crc = ModbusCrc16.Compute(adu);
        var withCrc = new byte[adu.Length + 2];
        Buffer.BlockCopy(adu, 0, withCrc, 0, adu.Length);
        withCrc[^2] = (byte)(crc & 0xFF);
        withCrc[^1] = (byte)(crc >> 8);
        var parsed = ModbusRtuParser.TryParse(withCrc);
        string summary = lrcOk
            ? "[ASCII] " + parsed.Summary
            : $"LRC 失败（应为 {ModbusLrc.Compute(raw.AsSpan(0, raw.Length - 1)):X2}）| {parsed.Summary}";
        frame = (raw, lrcOk, raw[0], raw[1], (raw[1] & 0x80) != 0, summary);
        return true;
    }

    private static bool IsHex(char c) => c is (>= '0' and <= '9') or (>= 'A' and <= 'F') or (>= 'a' and <= 'f');
    private static int Hex(char c) => c switch
    {
        >= '0' and <= '9' => c - '0',
        >= 'A' and <= 'F' => c - 'A' + 10,
        _ => c - 'a' + 10
    };

    private sealed class Channel
    {
        public StringBuilder Buf { get; } = new();
        public bool InFrame;
        public DateTimeOffset Start;
        public int SessionId;
        public string SessionName = "";
        public SerialDirection Direction;
        public string Source = "";
        public string Dest = "";
    }
}
