# PythonTools — 嵌入式与数据处理工具集

一组面向嵌入式开发与日常数据处理的 Python / 脚本工具：音频、LVGL 字体、串口、发票查重、CLI 启动等。

仓库：[sunguangxian/Tools](https://github.com/sunguangxian/Tools)

---

## 目录

- [快速开始](#快速开始)
- [工具一览](#工具一览)
- [1. WAV ↔ C 数组](#1-wav--c-数组互转-wav_to_c_arraypy)
- [2. LVGL 字体助手](#2-lvgl-字体助手-lvgl_font_toolpy)
- [3. 串口波形显示](#3-串口波形显示-serial_waveform)
- [4. 串口长度帧校验](#4-串口长度帧校验-serial_toolsserial_frame_checkpy)
- [5. 数据波形绘图](#5-数据波形绘图-数据波形生成器py)
- [6. 发票查重](#6-发票查重-fapiao_checkpy)
- [7. CLI 启动器](#7-cli-启动器-start_grokbat)
- [8. 猫猫云 Clash 解密](#8-猫猫云-clash-配置解密-maomaoyun_clashpy)
- [9. USB 串口挂到 WSL](#9-usb-串口挂到-wsl-usb_serial-wslps1)
- [环境与依赖](#环境与依赖)
- [用 run.bat 启动](#用-runbat-启动)
- [协作说明](#协作说明)

---

## 快速开始

```bash
# 克隆
git clone https://github.com/sunguangxian/Tools.git
cd Tools

# 安装依赖（推荐）
py -m pip install -r requirements.txt

# 任选一种方式运行
py wav_to_c_array.py
# 或双击 run.bat，从菜单选择
```

Windows 建议用 `py` 启动器；也可用 `python`。

---

## 工具一览

| # | 工具 | 路径 | 类型 |
|---|------|------|------|
| 1 | WAV ↔ C 数组 | `wav_to_c_array.py` | GUI |
| 2 | LVGL 字体助手 | `lvgl_font_tool.py` | GUI（需 Node/`lv_font_conv`） |
| 3 | 串口波形显示 | `serial_waveform/serial_waveform_gui.py` | GUI（PyQt5） |
| 4 | 串口长度帧校验 | `serial_tools/serial_frame_check.py` | CLI |
| 5 | 数据波形绘图 | `数据波形生成器.py` | GUI |
| 6 | 发票查重 | `fapiao_check.py` | GUI |
| 7 | Grok/Codex/Gemini 启动器 | `start_grok.bat` | Windows |
| 8 | 猫猫云 Clash 解密 | `maomaoyun_clash.py` | CLI |
| 9 | USB→WSL 串口 | `usb_serial-wsl.ps1` | PowerShell（需 usbipd） |

---

## 1. WAV ↔ C 数组互转 (`wav_to_c_array.py`)

**专为嵌入式音频播放设计。**

- **WAV → C 数组**：批量将目录下 WAV 转为带完整 Header 的 C 数组。
- **C 数组 → WAV**：从 C 文件还原 WAV，字节级一致。
- **GUI**：目录/文件选择与实时日志。

```bash
py wav_to_c_array.py
```

---

## 2. LVGL 字体助手 (`lvgl_font_tool.py`)

LVGL 字库瘦身与管理。

- 从现有 LVGL `.c` 字库逆向提取字符/图标码点。
- 递归扫描工程，提取源码中的汉字等非 ASCII 字符。
- 可手动编辑字符集，或追加 Unicode 图标码点（如 FontAwesome）。
- 一键调用 `lv_font_conv` 生成标准 C 字库。

**前提**：安装 [Node.js](https://nodejs.org/)，并安装转换工具：

```bash
npm install -g lv_font_conv
```

（工具内部也可通过 `npx lv_font_conv` 尝试运行。）

```bash
py lvgl_font_tool.py
```

---

## 3. 串口波形显示 (`serial_waveform/`)

实时串口数据可视化（多通道），基于 PyQt5 + pyqtgraph。

```bash
py serial_waveform/serial_waveform_gui.py
```

依赖见 `requirements.txt` 中的 `pyserial`、`numpy`、`pyqtgraph`、`PyQt5`。

---

## 4. 串口长度帧校验 (`serial_tools/serial_frame_check.py`)

校验 DT/PCM/DATA 等「长度前缀 + 二进制 payload」串口协议。

不按「每次 read 多少字节」判断，而是把串口当连续字节流，按协议重新组帧：

```text
+DT=<len>,<payload>\r\n
```

例如 `EXPECT_LEN = 320` 时，一帧大约：

```text
len("+DT=320,") + 320 + len("\r\n") = 330 bytes
```

### 主要功能

- 长度组帧：处理半帧、粘包、前导垃圾字节。
- 独立 RX 线程（默认），避免日志/写文件阻塞读串口。
- 日志：系统时间、相对时间、帧间隔、CRC32、`dropped`/`bad`；若 payload 含设备 tick，另输出 `dev_dt`。
- 可选 CSV 输出（配置区 `LOG_TO_FILE`）。
- 对比模式：`none` / `first` / `file_loop` / `file_seq` / `seq`。
- 协议可配：默认 `+DT=`，可改 `PREFIX` / `EXPECT_LEN` 等。

### 使用方法

```bash
py -m pip install pyserial   # 或 pip install -r requirements.txt
py serial_tools/serial_frame_check.py
```

启动后会**列出可用串口与常用波特率供选择**。其余参数在脚本顶部「配置区」修改，例如：

```python
PREFIX = b"+DT="
EXPECT_LEN = 320
COMPARE_MODE = "none"
# seq 示例：
# COMPARE_MODE = "seq"
# SEQ_OFFSET = 0
# SEQ_SIZE = 4
# SEQ_ENDIAN = "little"
# FIXED_TAIL_SKIP = 4
# DEV_TICK_OFFSET = 4
# DEV_TICK_SIZE = 4
# DEV_TICK_ENDIAN = "little"
# DEV_TICK_UNIT = "ms"
# FIXED_TAIL_SKIP = 8
```

### 判断标准（摘要）

- 持续 `OK len=320` 且 `bad`/`dropped` 不涨 → 协议格式基本正常（助手看到「粘包/半包」不代表设备异常）。
- `bad`/`dropped` 持续增加 → 字节流格式错误、丢字节、波特率/线材等问题。
- `LEN_BAD` → 优先核对 `EXPECT_LEN` 是否与设备 `PCM_FRAME_SIZE` 一致。
- `seq` / `file_seq` 跳变或 `DIFF` → 可能丢帧、重复、漏收或参考数据不一致。
- `dev_dt` 正常而 `dt` 偏大 → 设备定时正常、PC 收包偏慢；`dev_interval=WARN` 优先查设备侧。

---

## 5. 数据波形绘图 (`数据波形生成器.py`)

在文本框中粘贴一串整数（空格 / 逗号 / 换行分隔），用 matplotlib 绘制折线图，便于快速查看采样序列。

> 说明：当前版本是「已有数据 → 绘图」，不会自动生成正弦/方波，也不导出文件。

```bash
py 数据波形生成器.py
```

---

## 6. 发票查重 (`fapiao_check.py`)

扫描目录下 PDF 发票，解析代码/号码/日期/金额/税号等，按「发票代码 + 发票号码」查重，并导出 `发票查重结果.xlsx`。

```bash
py fapiao_check.py
```

额外依赖：`pandas`、`pdfplumber`、`openpyxl`（已写入 `requirements.txt`）。

---

## 7. CLI 启动器 (`start_grok.bat`)

在最近使用的目录里启动 Grok、Codex 或 Gemini。可选手工具、目录、是否在 WSL 中启动、是否走本机 VPN 代理。

- **运行**：双击 `start_grok.bat`，或在 `run.bat` 菜单中选择对应项。
- **配置**：同目录 `cli_launcher.json`（`vpnPort`、`useVpn`、`wsl`、`tool`、`recentDirs`）。`tool` 为 `grok` / `codex` / `gemini`。最近目录最多 20 个。若仍有旧名 `grok_launcher.json`，首次启动会改名为 `cli_launcher.json`。该文件仅本机保存，**不进版本库**。
- **代理**：仅勾选「使用本机 VPN」时设置 `HTTP_PROXY` / `HTTPS_PROXY`；默认端口 `10090`。
- **Windows**：Grok / Gemini 在所选目录开 `cmd`；Codex 由 npm 装为 `codex.ps1`，故用 PowerShell 启动。
- **WSL**：勾选后将 Windows 路径转为 WSL 路径再执行；优先 `$HOME/.grok/bin`、`$HOME/.local/bin`。

---

## 8. 猫猫云 Clash 配置解密 (`maomaoyun_clash.py`)

将猫猫云客户端加密的 `config.yaml` 解成明文 Clash/mihomo 配置。

```bash
py -m pip install cryptography
py maomaoyun_clash.py
# 或指定路径
py maomaoyun_clash.py <加密文件> <明文输出>
```

默认读脚本同目录 `config.yaml`，写出 `config.decrypted.yaml`。

**注意**：明文含节点密码。`config.yaml` 与 `config.decrypted.yaml` 已在 `.gitignore`，勿外传或提交。

---

## 9. USB 串口挂到 WSL (`usb_serial-wsl.ps1`)

在 Windows 上用 [usbipd-win](https://github.com/dorssel/usbipd-win) 将 USB 设备 attach/detach 到 WSL，并列出 `/dev/ttyACM*`、`/dev/ttyUSB*`。

```powershell
# 建议以管理员或已授权 usbipd 的环境运行
powershell -ExecutionPolicy Bypass -File .\usb_serial-wsl.ps1
```

菜单：`[0]` Attach · `[1]` Detach · `[2]` Status · `[q]` 退出。

---

## 环境与依赖

- **Python 3.x**（带 tkinter；多数 GUI 工具需要）
- **第三方**（见 `requirements.txt`）：

| 包 | 用途 |
|----|------|
| `pyserial` | 串口帧校验 / 串口波形 |
| `numpy` / `matplotlib` | 数值与绘图 |
| `pyqtgraph` / `PyQt5` | 串口波形 GUI |
| `cryptography` | 猫猫云解密 |
| `pandas` / `pdfplumber` / `openpyxl` | 发票查重与 Excel 导出 |

```bash
py -m pip install -r requirements.txt
```

LVGL 字体助手另需 Node.js + `lv_font_conv`。  
USB→WSL 另需本机安装 `usbipd`。

---

## 用 run.bat 启动

在仓库根目录双击 `run.bat`（或命令行运行），按编号选择工具。

> 维护约定：新增可通过菜单启动的工具时，须同步改 `run.bat`、`README.md`、`requirements.txt`（详见 `AGENTS.md`）。

建议菜单编号示例（实现时与 `run.bat` 保持一致）：

1. WAV ↔ C 数组  
2. LVGL 字体助手  
3. 串口波形显示  
4. 串口长度帧校验  
5. 数据波形绘图  
6. 发票查重  
7. CLI 启动器（Grok / Codex / Gemini）  
8. 猫猫云 Clash 解密  
9. USB 串口 → WSL  
0 / q. 退出  

也可直接：

```bash
py [脚本路径]
```

---

## 协作说明

面向 Codex / Cursor 等助手的仓库约定见根目录 [`AGENTS.md`](AGENTS.md)（功能变更须联动 README、`run.bat`、`requirements.txt`；提交信息中文 + Conventional Commits）。
