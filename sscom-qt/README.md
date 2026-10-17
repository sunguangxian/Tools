# sscom-qt — SSCOM 串口调试助手（Qt6）

C++ / Qt6 重写，界面和串口行为参考 SSCOM V5.13.1。**只做串口**，不含 TCP/IP、ISP 烧写和 LUA。

仓库里不再保留 Avalonia 版 `sscom/`。

## 功能

- 打开/关闭串口：端口列表、可编辑波特率、数据位 / 校验 / 停止位 / 流控、RTS / DTR
- 接收区：本地编码（中文 Windows 为 GBK）或 HEX、按空闲超时分包并加时间戳、显示缓冲上限、字体与背景色
- 保存接收窗口文本；勾选「接收数据到文件」后，把原始字节写到「文档/sscom-qt/ReceivedTofile-….DAT」
- 发送区：ASCII 或 HEX、加回车换行、定时发送、校验（无 / Modbus CRC16 / ADD / XOR，可指定起始字节和末尾跳过字节）
- 打开文件到发送区；发送文件时按 256 字节分块，延时可选连续 / 1 / 10 / 50 / 100 ms
- 「停止」会停下定时发送、文件发送和多字符串循环
- 扩展区 99 条多字符串：序号、按钮名（右键重命名）、HEX、内容、延时。序号大于 0 的项按序号循环发送
- 终端：回车发送一行、按键立即发送、本地回显、回车后补 0x0A
- 启动时打开串口、窗口最前、中英文字、参数自动记在本机（退出或点「保存参数」）
- 「串口设置 → 导入 sscom51.ini」可读原版配置里的多字符串和串口选项（不导入 TCP 地址）
- 小工具：HEX 转换计算器（长度、整数、ADD、XOR、Modbus CRC16）

## 依赖

- **CMake** ≥ 3.16
- **Qt 6**（Core / Gui / Widgets / **SerialPort**）
- **C++17**（MSVC 2019+、GCC 10+、Clang 12+）

Debian/Ubuntu：

```bash
sudo apt install qt6-base-dev qt6-serialport-dev cmake g++
```

## 构建

Linux：

```bash
cd sscom-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/sscom-qt
./build/sscom-qt --self-test
```

Windows（x64 Native Tools 或已配置好的 Qt 套件）：

```bat
cd sscom-qt
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\sscom-qt.exe
build\sscom-qt.exe --self-test
```

`CMAKE_PREFIX_PATH` 换成本机 Qt 安装路径。

## 目录

```
sscom-qt/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp
    ├── codec.h          HEX / Modbus CRC16 / ADD / XOR
    ├── MainWindow.h
    └── MainWindow.cpp
```
