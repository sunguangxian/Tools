# sscom-avalonia 功能验证报告

- **工程**：`/workspace/sscom-avalonia/`
- **规格**：`/workspace/sscom-qt/FEATURES.md`、`/workspace/sscom-qt/docs/SSCOM_LAYOUT_SPEC.md`
- **日期**：2026-10-17（Asia/Shanghai）
- **构建**：`dotnet build -c Release` — **成功**（0 Error）
- **截图**：`verify-main.png` / `verify-ext.png`（均为 **794×600**，扩展后外框不变）
- **环回**：`socat` PTY `/tmp/sscom-ptyA` ↔ `/tmp/sscom-ptyB` + `tools/VerifyHarness`

## 状态说明

| 状态 | 含义 |
|------|------|
| **Pass** | 代码已实现，且本机可验证路径正常 |
| **Partial** | 有 UI/绑定，逻辑不完整或与 Qt 同为桩 |
| **Fail** | 明显缺陷（已尽量在本轮修复） |
| **Blocked** | 无真机串口；用 PTY/失败路径代替 |

---

## 功能对照表

| Feature | Status | Notes |
|---------|--------|-------|
| 端口列表刷新 | **Pass** | `RefreshPortsCommand` → `SerialService.AvailablePorts()`；空列表时占位 `/dev/ttyS0|USB0|ACM0`；菜单与 ↻ 按钮均绑定 |
| 打开/关闭串口 | **Pass** | `ToggleConnectCommand`；无设备时 `Open` 返回失败并弹「打开失败」；PTY 打开成功；关闭停自动发送并更新 LED/文案 |
| 打开失败错误处理 | **Pass** | 无设备：`Access to the port '/dev/ttyS0' is denied.`（Harness）；空端口提示「请选择端口号」 |
| 波特率 / 数据位 / 停止位 / 校验 / 流控 | **Pass** | Combo 绑定齐全；「更多串口设置」切换 `MoreSerialVisible`；`Open(...)` 映射到 `SerialPort` 参数 |
| RTS / DTR | **Pass** | 勾选变更时若已连接则 `SetRts`/`SetDtr`；PTY 上 ioctl 可能失败，已 **try/catch** 上报错误而不崩溃 |
| RX ASCII / HEX 显示 | **Pass** | `HexDisplay` → `HexUtils.ToAsciiDisplay` / `ToHexDisplay`；Harness 单测通过 |
| 时间戳分包显示 | **Partial** | 勾选后按每次 `DataReceived` 块加 `[HH:mm:ss.fff]`；**超时 ms（PacketTimeoutMs）仅持久化，未用于分包计时**（与 sscom-qt 相同） |
| 清除接收区 | **Pass** | `ClearReceiveCommand` 清空缓冲/计数并恢复欢迎文案 |
| 保存接收数据（保存数据） | **Pass** | `SaveData` 弹保存对话框，写 `FileStream`；取消则取消勾选 |
| 接收数据到文件 | **Pass**（本轮修复） | 原先仅有勾选无写入；已实现独立 `_recvFile` 流，行为与「保存数据」对称 |
| TX ASCII / HEX | **Pass** | `ComposeSendPayload` + `HexUtils.FromHexText`；非法 HEX 弹「HEX错误」；未连接弹「请先打开串口」 |
| 加回车换行 CRLF | **Pass** | `AppendNewline` 追加 `\r\n` |
| 定时自动发送 | **Pass** | `AutoSend` + `DispatcherTimer`；未连接强制关并提示；间隔 `AutoSendIntervalMs`（≥10ms） |
| 校验 XOR / Sum8 | **Pass**（本轮增强） | `MaybeAppendChecksum`；**「第 n 字节至末尾」CsFrom 已参与计算**（Qt 侧未用 CsFrom） |
| 发送文件 | **Pass** | `SendFileCommand`：有路径则读字节发送，否则文件框；未连接提示 |
| 打开文件到发送区 | **Pass** | `OpenFileToSendCommand` → `ReadAllText` 填入 `SendText` |
| 停止 | **Pass** | `_stopFlag` + 停定时器 + `AutoSend=false` |
| 扩展 / 隐藏多字符串 | **Pass** | 接收区三列 Grid + `ApplyExtensionLayout`（47%/53%）；按钮文案「扩展 >>」↔「隐藏」；截图外框同为 794×600 |
| 多字符串预设发送 | **Pass** | 32 行 `PresetItemViewModel`，HEX + 发送；未连接提示；持久化 presets |
| 置顶（最前） | **Pass** | `StayOnTop` → `Window.Topmost`（`RequestStayOnTopChanged`） |
| 参数记忆 | **Pass** | `~/.config/sscom-avalonia/settings.json`（`ApplicationData`）；退出若勾选「保存参数」则 `PersistSettings`；Harness 读写往返通过 |
| English 发送按钮 | **Pass** | `EnglishLabels` 切换「发送/Send」「打开串口/Open」等 |
| ProfessionalDark 主题 | **Pass** | `App.axaml`：`FluentTheme Compact` + `RequestedThemeVariant=Dark` + Merge `Themes/ProfessionalDark.axaml`；启动截图无崩溃，深色背景约 `#1E1E1E` 系 |
| 启动无崩溃 | **Pass** | `--screenshot` 两次 exit 0，生成 PNG |

---

## 运行时验证摘要

| 项 | 结果 |
|----|------|
| Release 构建 | OK |
| HexUtils | OK |
| SettingsStore 路径 / 往返 | OK → `/home/box/.config/sscom-avalonia/settings.json` |
| SerialService Open 失败路径 | OK |
| SerialService PTY Open / Write / DataReceived / Close | OK |
| RTS/DTR on PTY | OK（异常已吞并上报，不崩） |
| 截图 main / ext 尺寸 | 794×600 / 794×600 |

Harness：`dotnet run --project /workspace/sscom-avalonia/tools/VerifyHarness -c Release` → `SUMMARY|fails=0`

---

## 本轮修复（&lt;30min 级）

1. **接收数据到文件**：补全 `OnRecvToFileChanged` + `_recvFile` 写入（原先 UI 桩）。
2. **校验起始字节 CsFrom**：`MaybeAppendChecksum` 按 1-based 起点计算 XOR/Sum8。
3. **RTS/DTR**：`SerialService.SetRts/SetDtr` 捕获异常，避免 PTY/部分设备 ioctl 崩溃。
4. **无端口占位名**：改为 `/dev/ttyS0` 等，避免相对名歧义。
5. **工程**：`SscomAvalonia.csproj` 排除 `tools/**`，避免 VerifyHarness 被主工程误编。

---

## 已知差距 / 说明

| 项 | 说明 |
|----|------|
| PacketTimeoutMs | UI + 持久化有，**未实现超时分包合并**（sscom-qt 同样未用） |
| SaveParamsCommand | ViewModel 有「立即保存」命令，工具栏「保存参数」是 **退出时记忆勾选**，未绑立即保存按钮 |
| LedColor | 属性存在；界面用 `IsConnected` 双 Ellipse 显色 |
| 真机串口 | 本环境无物理口；打开失败与 PTY 环回已覆盖主路径 |
| 发送文件大文件 | 一次 `ReadAllBytes` 全量写入，无分块进度（与轻量调试工具定位相符） |

---

## 截图

- 折叠扩展：`/workspace/sscom-avalonia/verify-main.png`
- 展开多字符串：`/workspace/sscom-avalonia/verify-ext.png`

---

## 结论

对照 FEATURES 串口功能：**核心收发、参数、扩展布局、主题、参数记忆均为 Pass**；时间戳「超时分包」与 Qt 一样为 Partial。本轮已修接收到文件、CsFrom 校验、RTS/DTR 健壮性。整体可交付使用；无真机时打开串口会走失败提示，属预期。
