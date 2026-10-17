# St.Theme

早先 Avalonia 界面用的暗色色板（Visual Studio 风格 `ProfessionalDark`）。Qt 版 SSCOM / PortMonitor 不引用它。

## 引用

```xml
<ProjectReference Include="..\..\shared\St.Theme\St.Theme.csproj" />
```

在 `App.axaml` 中合并资源：

```xml
<ResourceInclude Source="avares://St.Theme/Themes/ProfessionalDark.axaml"/>
```

资源键前缀为 `St*`（如 `StAccent`、`StBgBrush`）。业务控件样式仍留在各 App 本地。

## 密度

各 App 的 `App.axaml` 须使用：

```xml
<FluentTheme DensityStyle="Compact" />
```

（Avalonia FluentTheme 属性名是 `DensityStyle`，不是 `DensityMode`。）控件高度/边距在各 App 本地样式中再收紧到约 26px 行高。

