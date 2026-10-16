# sscom-avalonia

开源串口调试助手（Avalonia UI），布局参考 SSCOM V5.13，**仅串口**。

## 主题

与 **端口监视（PortMonitor）** 一致：`FluentTheme DensityStyle="Compact"` + `RequestedThemeVariant="Dark"`，色板来自仓库共享库 [`shared/St.Theme`](../shared/St.Theme/)（`avares://St.Theme/Themes/ProfessionalDark.axaml`；`StBg` #1E1E1E、`StAccent` #007ACC、`StPanel` #2D2D30 等）。

## 功能

- 打开/关闭串口，端口刷新，波特率 / 数据位 / 停止位 / 校验 / 流控，RTS/DTR
- 接收区、HEX 显示、时间戳、保存接收数据
- 发送区、HEX 发送、定时发送、加回车换行、校验（XOR / Sum8）
- 发送文件 / 打开文件到发送区 / 停止
- **扩展**：接收区内左右分栏多字符串（窗口尺寸不变，按钮为「隐藏」）
- 参数记忆（`~/.config/sscom-avalonia/settings.json`）、置顶、English 发送按钮文案

## 运行

```bash
export PATH="$HOME/.dotnet:$PATH"
cd sscom   # 本仓库子目录
dotnet run -c Release
```

依赖：.NET 10（或改 `TargetFramework` 为可用 SDK）、Linux 下 `System.IO.Ports` 与串口设备权限。

## 截图

布局为 Grid（接收区 * / 工具栏 Auto / 下栏固定高度），扩展在接收区内左右分栏，窗口 794×600 不变。

- `screenshot-main.png` — 默认（扩展关闭）
- `screenshot-ext.png` — 扩展打开（多字符串）

## 结构

```
../shared/St.Theme/           共用 ProfessionalDark 色板
Views/MainWindow.axaml(.cs)   UI 与文件对话框
ViewModels/MainViewModel.cs   主逻辑
Services/SerialService.cs     System.IO.Ports 封装
Services/HexUtils.cs          HEX/ASCII
Services/SettingsStore.cs     JSON 持久化
Models/AppSettings.cs
```
