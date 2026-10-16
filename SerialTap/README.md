# SerialTap

干净、可扩展的 **Avalonia + C#** 串口桥接监视工具。
界面为 **专业暗色（Visual Studio / Fluent Dark 风格）**，布局参考 CommMonitor（工具栏 + 左侧会话树 + 筛选 + 详情/发送/统计）。  
**不是** CEIWEI CommMonitor 的克隆，也不提供内核级「旁路已打开 COM」能力。  
另含基于 USBPcap 的 USB 大容量存储（U 盘）SCSI 命令监视。

> **主题**：暗色色板已抽至仓库共享库 [`../shared/St.Theme`](../shared/St.Theme/)（`avares://St.Theme/Themes/ProfessionalDark.axaml`），与 `sscom/` 共用。

## 它能做什么

| 模式 | 说明 |
|------|------|
| **桥接（推荐）** | 同时打开端口 A、端口 B，双向转发字节，并为每一段数据记日志（时间戳、方向、Hex、ASCII）。适合配合 [com0com](https://sourceforge.net/projects/com0com/) 等虚拟串口对：真实设备 ↔ SerialTap ↔ 业务程序。 |
| **单口监听** | 独占打开一个口并读数据。若该口已被其他进程占用，打开会失败——这是 Windows 用户态串口的正常限制。 |

## 它不能做什么

- 不占用 / 不挂接 **已被其他程序打开** 的 COM（那需要内核过滤驱动，超出本项目范围）
- 不复制任何商业串口监控软件的界面、驱动或授权逻辑
- Linux 上 `System.IO.Ports` 可用程度因发行版而异；**主目标平台是 Windows**

## 环境要求

- [.NET 9 SDK](https://dotnet.microsoft.com/download)（本仓库按 `net9.0` 构建）
- Windows 10/11 或 Windows Server（推荐）
- 可选：com0com / 硬件 USB 转串口，用于桥接演示

## 构建与运行

```bash
cd SerialTap
dotnet build SerialTap.sln
dotnet run --project src/SerialTap.App
```

发布单文件（Windows x64 示例）：

```bash
dotnet publish src/SerialTap.App -c Release -r win-x64 --self-contained true \
  -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true \
  -o publish/win-x64-single
```

在本仓库的 Linux 构建机上，`dotnet build SerialTap.sln` 应能成功；没有真实 COM 口时界面仍可打开，刷新端口列表可能为空。

## 桥接模式怎么用（Windows + com0com）

1. 安装 com0com，创建一对虚拟口，例如 `COM10` ↔ `COM11`。
2. 业务程序打开 `COM11`。
3. 真实设备（或另一端测试工具）接到 `COM10` 一侧（或再桥一层）。
4. 在 SerialTap 中选 **桥接**，端口 A = `COM10`，端口 B = `COM11`（或你实际用的一对），设好波特率后点 **打开**。
5. 日志区会看到 `A→B` / `B→A` 的 Hex 与 ASCII；可 **暂停**、**清空**、**导出 CSV**。

更简单的自测：用两对虚拟口或两台终端分别连 A/B，手工发字节观察日志。


## 多串口 / 多会话

SerialTap 可同时运行多个**会话**，每个会话独立设置模式与串口参数：

- **左侧会话面板**：填写名称（可选）、模式（桥接 A↔B / 单口监听）、端口（可下拉或手输，如 `COM5`）、波特率、数据位/校验/停止位，点 **＋ 添加会话**。
- 每个会话卡片显示：运行状态、字节数（A→B / B→A 或 RX）、最近错误；可单独 **打开 / 关闭 / 移除**，也可 **全部打开 / 全部关闭**。
- **同一物理端口不能被两个会话同时打开**：会提示“端口 COMx 已被会话「…」占用”。
- 所有会话汇入同一个事件流：**原始 Hex** 与 **Modbus RTU** 页签都有「会话」列；顶部可按**会话**和**方向**筛选。
- Modbus 解析按「会话 + 方向」分别缓冲，不同串口的字节不会拼成一帧。
- **暂停 / 清空 / 导出 CSV** 是全局操作；CSV 新增 `Session` 列。内存仍有全局上限（约 5 万事件 / 50MB）。

首次启动会预置两个**未打开**的示例会话（“PLC 桥接”“传感器”），可直接移除。



## P1 能力（相对早期 MVP）

- 行视图 / 终端视图；列表 HEX/DEC/OCT/BIN；编码 ASCII/UTF-8/GBK/UTF-16
- Modbus RTU + ASCII（LRC）；会话 `.stqs` 保存/加载；CSV/TXT/HTML 导出；自动保存切分
- 端口事件面板；发送页（HEX/文本 + CRC16）
- USB 设备树（从 USBPcap 流学习）+ SCSI / URB
- **注入监控**：Avalonia 版为界面占位；完整 MinHook 注入见同目录 `SerialTapHook.dll`（Native 实现）

演示：`SerialTap --demo` 或 `--demo --tab 3`（0 列表 / 1 行 / 2 终端 / 3 Modbus / 4 USB）

## Modbus RTU 视图

主窗口有两个页签：

1. **原始 Hex** — 每个串口读写块的时间戳 / 方向 / Hex / ASCII（原有功能）
2. **Modbus RTU** — 按方向分缓冲，用「功能码长度启发 + CRC-16」剥帧，静默间隙（约 3.5 字符时间，3–50 ms）结束时冲刷残留；列表显示时间、方向、从站、功能码、CRC OK/FAIL、摘要与 Hex

工具栏 **显示 CRC 错误帧** 勾选后也会列出校验失败或不完整帧（默认勾选）。解析逻辑在 `SerialTap.Core`：`ModbusRtuStreamParser`、`ModbusRtuParser`、`ModbusCrc16`。

## USB / SCSI 监视（U 盘 / 移动硬盘）

「USB / SCSI」页签用于观察 USB 大容量存储设备（Bulk-Only Transport，BOT）上的 SCSI 命令。
**SerialTap 自身不包含任何内核驱动**，实时抓包依赖用户自行安装的、已签名的 [USBPcap](https://desowin.org/usbpcap/)。

### 实时抓包（Windows）

1. 安装 USBPcap（Wireshark 安装包里也可勾选），装完**重启**。
2. **右键 SerialTap.exe → 以管理员身份运行**（USBPcap 需要管理员权限，否则会提示）。
3. 「USBPcapCMD」默认 `C:\Program Files\USBPcap\USBPcapCMD.exe`，装在别处可修改。
4. 「接口」选择 `\\.\USBPcap1`…（点 ⟳ 会通过 `USBPcapCMD --extcap-interfaces` 刷新；每个接口对应一个 USB 根集线器，不确定就逐个试）。
5. 「设备地址」可留空（抓该根集线器下全部设备，`-A`），或填 USB 设备地址只抓一个（`--devices N`）。
6. 点 **▶ 开始抓包**，然后插拔 / 读写 U 盘。

内部实现：启动 `USBPcapCMD.exe -d \\.\USBPcapN -o - [-A | --devices N]`，从标准输出读取 pcap 流（LINKTYPE_USBPCAP = 249）实时解析。

### 离线分析

点 **打开 pcap 文件…**，可加载 USBPcap / Wireshark 保存的 `.pcap` 或 `.pcapng`（Linux / macOS 上也能用）。
仓库自带示例：`samples/usb-msc-sample.pcap`（INQUIRY、READ CAPACITY、TEST UNIT READY 失败 + REQUEST SENSE、READ(10)、WRITE(10)、SYNCHRONIZE CACHE）。
命令行：`SerialTap.exe --open-pcap 路径` 启动即打开该文件并切到 USB / SCSI 页签。

### 显示内容

- **SCSI 命令**：时间、设备（总线.地址）、Tag、LUN、命令名、LBA、长度（块数或字节）、CSW 状态（Passed / Failed / Phase Error）、Data Residue、CBW→CSW 耗时（ms）、摘要。
  已解码：TEST UNIT READY、REQUEST SENSE（Sense Key / ASC / ASCQ + 中文说明）、INQUIRY（厂商 / 产品 / 版本）、READ CAPACITY(10/16)（最后 LBA、块大小、容量）、READ/WRITE(10/12/16)、MODE SENSE(6/10)（写保护位）、SYNCHRONIZE CACHE(10/16)、START STOP UNIT、PREVENT ALLOW MEDIUM REMOVAL、READ FORMAT CAPACITIES；其它操作码显示为十六进制 CDB。
- **USB 传输**：每个 USBPcap 记录（总线.设备.端点、IN/OUT、提交/完成、类型、长度、USBD 状态、前 64 字节）。
- **导出 SCSI CSV**：保存到 `文档/SerialTap/scsi-*.csv`。

### 限制

- 实时抓包只支持 **Windows + USBPcap**；本项目不写、不带内核驱动。
- 只解码 **BOT（Bulk-Only）** 协议。USB 3.x 硬盘盒若走 **UAS（USB Attached SCSI）**，只能在「USB 传输」里看到原始数据，不会解成 SCSI 命令（可在设备管理器把驱动换成 BOT 再观察）。
- 每条命令只保留前 512 字节数据用于解析响应，不保存完整扇区内容。
- 自测：`dotnet run --project tests/SerialTap.UsbSelfTest -- samples/usb-msc-sample.pcap`（会重新生成示例文件并校验解码结果）。

## 单文件 Windows 可执行程序

```bash
dotnet publish src/SerialTap.App -c Release -r win-x64 --self-contained true \
  -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true \
  -o publish/win-x64-single
```

产物：`publish/win-x64-single/SerialTap.exe`（可拷到 Windows 双击运行，无需预装 .NET）。

## CSV 导出

默认写到文档目录下的 `SerialTap/serialtap-yyyyMMdd-HHmmss.csv`，列包括：Sequence、Timestamp、Direction、SourcePort、DestPort、Length、Hex、Ascii。

## 项目结构

```
SerialTap/
├── SerialTap.sln
├── README.md
└── src/
    ├── SerialTap.Core/          # 与 UI 无关的核心库
    │   ├── Models/              # SerialEvent、RingBuffer、设置枚举
    │   ├── Bridge/              # ISerialBridge、SerialBridge、端口枚举
    │   ├── Sessions/            # SessionManager、MonitorSession（多会话）
    │   ├── Usb/                 # pcap/pcapng 读取、USBPcap 头、BOT + SCSI 解码、USBPcapCMD 抓包
    │   ├── Protocol/            # ModbusCrc16、ModbusRtuParser、ModbusRtuStreamParser
    │   └── Export/              # CsvExporter
    └── SerialTap.App/           # Avalonia MVVM 桌面端
tests/SerialTap.UsbSelfTest/     # USB/SCSI 解码自测（生成并校验示例 pcap）
samples/usb-msc-sample.pcap      # 示例抓包
        ├── ViewModels/
        └── Views/
```

### 后续扩展建议

- Modbus ASCII / 更多功能码语义
- HTML / 分段大日志导出
- 过滤（按方向、端口、字节模式）
- （若真有需求）独立内核过滤驱动项目——勿与本用户态 MVP 混在一处

## 许可证说明

本仓库为自有实现，供进一步开发。请勿将商业软件的二进制、资源或专有协议实现拷入本工程。
