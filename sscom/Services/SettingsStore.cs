using System.Text.Json;
using SscomAvalonia.Models;

namespace SscomAvalonia.Services;

public static class SettingsStore
{
    public const int PresetCount = 32;

    private static string SettingsPath =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "sscom-avalonia",
            "settings.json");

    public static AppSettings Load()
    {
        try
        {
            var path = SettingsPath;
            if (!File.Exists(path))
                return Normalize(new AppSettings());
            var json = File.ReadAllText(path);
            var s = JsonSerializer.Deserialize<AppSettings>(json) ?? new AppSettings();
            return Normalize(s);
        }
        catch
        {
            return Normalize(new AppSettings());
        }
    }

    public static void Save(AppSettings s)
    {
        var path = SettingsPath;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var json = JsonSerializer.Serialize(Normalize(s), new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(path, json);
    }

    private static AppSettings Normalize(AppSettings s)
    {
        while (s.Presets.Count < PresetCount) s.Presets.Add("");
        while (s.PresetHexFlags.Count < PresetCount) s.PresetHexFlags.Add(false);
        if (s.Presets.Count > PresetCount) s.Presets = s.Presets.Take(PresetCount).ToList();
        if (s.PresetHexFlags.Count > PresetCount) s.PresetHexFlags = s.PresetHexFlags.Take(PresetCount).ToList();
        return s;
    }
}
