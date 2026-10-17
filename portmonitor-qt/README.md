# portmonitor-qt — PortMonitor 端口监视（Qt6）

C++ / Qt6 Widgets 重写。已接入 `QSerialPort`：多会话添加/移除、各会话独立打开串口、RX 汇总到日志表。

工作版 Avalonia 仍在仓库 [`../PortMonitor/`](../PortMonitor/)。大功能暂时冻结在 Avalonia，新开发优先往本目录迁移。

本目录与 Avalonia 的 `PortMonitor/` **并列**于 Tools 根下（不嵌套在 Avalonia 树内），便于独立演进。

## 依赖

- **CMake** ≥ 3.16
- **Qt 6**（Core / Gui / Widgets / **SerialPort**）
- **C++17** 编译器

Debian/Ubuntu：

```bash
sudo apt install qt6-base-dev qt6-serialport-dev cmake g++
```

## 构建（Linux）

```bash
cd portmonitor-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/portmonitor-qt
```

## 构建（Windows + MSVC + Qt6）

```bat
cd portmonitor-qt
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\portmonitor-qt.exe
```

## 功能（v0.1）

| 项 | 状态 |
|----|------|
| 端口枚举 / 刷新 | ✅ |
| 添加会话并打开串口 | ✅（默认 **115200 8N1**，UI 暂无波特率选择） |
| 移除会话并关闭串口 | ✅ |
| 多会话 RX → 日志表（时间/方向/会话/数据） | ✅ |
| 按会话过滤日志 / TX / 旁路 | ❌ 后续 |
| 与 Avalonia 功能对等 | ❌ 进行中 |

## 目录

```
portmonitor-qt/
├── CMakeLists.txt
├── README.md
├── .gitignore
└── src/
    ├── main.cpp
    ├── MainWindow.h
    └── MainWindow.cpp
```
