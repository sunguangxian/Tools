using System.Text;

namespace SerialTap.Core.Protocol;

public enum TextEncodingKind { Ascii, Utf8, Gbk, Utf16Le, Utf16Be, Latin1 }

public static class TextDecoder
{
    private static readonly string[] Ctl =
    [
        "NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL","BS","HT","LF","VT","FF","CR","SO","SI",
        "DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB","CAN","EM","SUB","ESC","FS","GS","RS","US"
    ];

    public static string Name(TextEncodingKind e) => e switch
    {
        TextEncodingKind.Ascii => "ASCII",
        TextEncodingKind.Utf8 => "UTF-8",
        TextEncodingKind.Gbk => "GBK",
        TextEncodingKind.Utf16Le => "UTF-16LE",
        TextEncodingKind.Utf16Be => "UTF-16BE",
        TextEncodingKind.Latin1 => "Latin-1",
        _ => "?"
    };

    public static string Decode(TextEncodingKind kind, ReadOnlySpan<byte> data, bool escapeControls = false, bool keepNewlines = false)
    {
        return kind switch
        {
            TextEncodingKind.Ascii => DecodeAscii(data, escapeControls, keepNewlines),
            TextEncodingKind.Latin1 => DecodeLatin1(data, escapeControls, keepNewlines),
            TextEncodingKind.Utf8 => DecodeUtf8(data, escapeControls, keepNewlines),
            TextEncodingKind.Utf16Le => DecodeUtf16(data, littleEndian: true, escapeControls, keepNewlines),
            TextEncodingKind.Utf16Be => DecodeUtf16(data, littleEndian: false, escapeControls, keepNewlines),
            TextEncodingKind.Gbk => DecodeGbk(data, escapeControls, keepNewlines),
            _ => DecodeAscii(data, escapeControls, keepNewlines)
        };
    }

    private static string DecodeAscii(ReadOnlySpan<byte> data, bool esc, bool nl)
    {
        var sb = new StringBuilder(data.Length);
        foreach (var b in data)
        {
            if (b < 0x80) PutCp(sb, b, esc, nl);
            else if (esc) sb.Append($"\\x{b:X2}");
            else sb.Append('.');
        }
        return sb.ToString();
    }

    private static string DecodeLatin1(ReadOnlySpan<byte> data, bool esc, bool nl)
    {
        var sb = new StringBuilder(data.Length);
        foreach (var b in data) PutCp(sb, b, esc, nl);
        return sb.ToString();
    }

    private static string DecodeUtf8(ReadOnlySpan<byte> data, bool esc, bool nl)
    {
        var sb = new StringBuilder(data.Length);
        int i = 0;
        while (i < data.Length)
        {
            byte c = data[i];
            int len; uint cp;
            if (c < 0x80) { cp = c; len = 1; }
            else if ((c & 0xE0) == 0xC0) { cp = (uint)(c & 0x1F); len = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = (uint)(c & 0x0F); len = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = (uint)(c & 0x07); len = 4; }
            else { sb.Append('\uFFFD'); i++; continue; }
            if (i + len > data.Length) { sb.Append('\uFFFD'); break; }
            bool ok = true;
            for (int k = 1; k < len; k++)
            {
                if ((data[i + k] & 0xC0) != 0x80) { ok = false; break; }
                cp = (cp << 6) | (uint)(data[i + k] & 0x3F);
            }
            if (!ok) { sb.Append('\uFFFD'); i++; continue; }
            PutCp(sb, cp, esc, nl);
            i += len;
        }
        return sb.ToString();
    }

    private static string DecodeUtf16(ReadOnlySpan<byte> data, bool littleEndian, bool esc, bool nl)
    {
        var sb = new StringBuilder(data.Length / 2);
        for (int i = 0; i + 1 < data.Length; i += 2)
        {
            uint u = littleEndian ? (uint)(data[i] | (data[i + 1] << 8)) : (uint)((data[i] << 8) | data[i + 1]);
            if (u is >= 0xD800 and < 0xDC00 && i + 3 < data.Length)
            {
                uint v = littleEndian ? (uint)(data[i + 2] | (data[i + 3] << 8)) : (uint)((data[i + 2] << 8) | data[i + 3]);
                if (v is >= 0xDC00 and < 0xE000)
                {
                    PutCp(sb, 0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00), esc, nl);
                    i += 2;
                    continue;
                }
            }
            PutCp(sb, u is >= 0xD800 and < 0xE000 ? 0xFFFD : u, esc, nl);
        }
        if ((data.Length & 1) != 0) sb.Append('.');
        return sb.ToString();
    }

    private static string DecodeGbk(ReadOnlySpan<byte> data, bool esc, bool nl)
    {
        Encoding? gbk = null;
        try { gbk = Encoding.GetEncoding(936); } catch { /* optional code page */ }
        var sb = new StringBuilder(data.Length);
        int i = 0;
        while (i < data.Length)
        {
            if (data[i] < 0x80) { PutCp(sb, data[i], esc, nl); i++; continue; }
            if (i + 1 >= data.Length || data[i] is 0x80 or 0xFF) { sb.Append('\uFFFD'); i++; continue; }
            if (gbk is null) { sb.Append('\uFFFD'); i += 2; continue; }
            try
            {
                sb.Append(gbk.GetString(data.Slice(i, 2)));
                i += 2;
            }
            catch
            {
                sb.Append('\uFFFD');
                i++;
            }
        }
        return sb.ToString();
    }

    private static void PutCp(StringBuilder sb, uint c, bool esc, bool nl)
    {
        if (c < 0x20 || c == 0x7F)
        {
            if (nl && c is '\n' or '\r' or '\t') { sb.Append((char)c); return; }
            if (esc)
            {
                sb.Append('\\');
                sb.Append(c == 0x7F ? "DEL" : Ctl[c]);
            }
            else sb.Append('.');
            return;
        }
        if (c <= 0xFFFF) sb.Append((char)c);
        else
        {
            c -= 0x10000;
            sb.Append((char)(0xD800 + (c >> 10)));
            sb.Append((char)(0xDC00 + (c & 0x3FF)));
        }
    }
}
