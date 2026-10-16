using System.Text;

namespace SscomAvalonia.Services;

public static class HexUtils
{
    public static string ToHexDisplay(byte[] data)
    {
        if (data.Length == 0) return string.Empty;
        var sb = new StringBuilder(data.Length * 3);
        for (int i = 0; i < data.Length; i++)
        {
            if (i > 0) sb.Append(' ');
            sb.Append(data[i].ToString("X2"));
        }
        return sb.ToString();
    }

    public static byte[] FromHexText(string text, out bool ok)
    {
        var cleaned = new StringBuilder(text.Length);
        foreach (var c in text)
        {
            if (char.IsWhiteSpace(c)) continue;
            cleaned.Append(c);
        }
        if (cleaned.Length == 0)
        {
            ok = true;
            return Array.Empty<byte>();
        }
        if (cleaned.Length % 2 != 0)
        {
            ok = false;
            return Array.Empty<byte>();
        }
        for (int i = 0; i < cleaned.Length; i++)
        {
            var c = cleaned[i];
            if (!Uri.IsHexDigit(c))
            {
                ok = false;
                return Array.Empty<byte>();
            }
        }
        var outBytes = Convert.FromHexString(cleaned.ToString());
        ok = true;
        return outBytes;
    }

    public static string ToAsciiDisplay(byte[] data)
    {
        var sb = new StringBuilder(data.Length);
        foreach (var b in data)
        {
            if (b is (byte)'\r' or (byte)'\n' or (byte)'\t')
                sb.Append((char)b);
            else if (b >= 32 && b < 127)
                sb.Append((char)b);
            else
                sb.Append('.');
        }
        return sb.ToString();
    }
}
