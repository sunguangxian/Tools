# PortMonitor — 串口 / USB / SCSI 监视

Qt6 桌面程序。界面按串口监视工具的常见布局：深色工具栏、左侧设备树、数据表 / 行视图 / 终端 / 转储 / SCSI、底部 HEX 与 ASCII、状态栏计数。

只做三件事：

- **串口**：勾选 COM，独占打开后记录读、写（默认 115200 8N1，可改波特率、数据位、校验、停止位）。可向已打开的口发送 HEX 或 UTF-8 文本。
- **USB**：通过本机已安装的 [USBPcap](https://desowin.org/usbpcap/) 抓 URB（需要管理员权限）。
- **USB SCSI**：把大容量存储的 Bulk-Only（BOT）传输解成 SCSI 命令（INQUIRY、READ/WRITE、REQUEST SENSE 等）。

不需要注册，也不需要登录。本程序不附带、不安装内核驱动。

## 做不到的事

- 不能旁路监听**已经被其它程序打开**的 COM。那需要内核过滤驱动；用户态 `QSerialPort` 打开失败时会提示。
- 不解码 UAS（USB Attached SCSI）。这类盘只出现在数据表的原始 URB 里。
- 实时 USB 抓包只支持 Windows + USBPcap。其它系统可以用「打开 pcap」看 `.pcap` / `.pcapng`。

## 依赖

- CMake ≥ 3.16
- Qt 6（Core / Gui / Widgets / SerialPort）
- C++17（Windows 上用 MSVC）
- 实时 USB： [USBPcap](https://desowin.org/usbpcap/)（装完需重启），并且**以管理员身份运行**

## 构建（Windows + MSVC + Qt6）

```bat
cd portmonitor-qt
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=D:\Qt\6.12.0\msvc2022_64
cmake --build build --config Release
```

产物：`build\Release\portmonitor-qt.exe`。拿到别的机器运行前，用 Qt 的 `windeployqt` 带上 DLL：

```bat
D:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe --release build\Release\portmonitor-qt.exe
```

## 用法

1. 左侧勾选串口和/或 `\\.\USBPcapN`。
2. 点 **开始监视**。读为蓝色，写 / OUT 为琥珀色，HEX 为绿色。
3. **暂停** 时新数据不记入列表。**清空** 只清列表，不关闭端口。
4. 点一行，在底部和「转储」里看 HEX / ASCII。
5. U 盘读写出现在 **SCSI** 页：命令、LBA、块数、CSW 状态、耗时。
6. **打开 pcap** 可离线分析。示例：`samples/usb-msc-sample.pcap`。
7. 命令行：`portmonitor-qt.exe --open-pcap 路径`。

导出：文件菜单里的数据 CSV、SCSI CSV。

## 自测

```bat
cmake --build build --config Release --target portmonitor-scsi-test
build\Release\portmonitor-scsi-test.exe samples\usb-msc-sample.pcap
```

覆盖 SCSI CDB、BOT 关联、classic pcap 与 pcapng 往返，以及示例抓包（INQUIRY、READ CAPACITY、TEST UNIT READY 失败、REQUEST SENSE、READ(10)、WRITE(10)、SYNCHRONIZE CACHE）。

## 目录

```
portmonitor-qt/
├── CMakeLists.txt
├── samples/usb-msc-sample.pcap
├── tests/scsi_selftest.cpp
└── src/
    ├── main.cpp / MainWindow.*     # 界面
    ├── CaptureModel.*              # 数据表与筛选
    ├── PcapParser.*                # pcap / pcapng
    └── UsbDecode.*                 # USBPcap 头、BOT、SCSI
```
