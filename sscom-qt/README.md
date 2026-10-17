# sscom-qt — SSCOM 串口调试助手（Qt6）

C++ / Qt6 Widgets 重写。已接入 `QSerialPort`：枚举端口、打开/关闭、RX/TX（文本与 HEX）。

工作版 Avalonia 仍在仓库 [`../sscom/`](../sscom/)。大功能暂时冻结在 Avalonia，新开发优先往本目录迁移。

## 依赖

- **CMake** ≥ 3.16
- **Qt 6**（Core / Gui / Widgets / **SerialPort**）
- **C++17** 编译器（MSVC 2019+、GCC 10+、Clang 12+）

Debian/Ubuntu：

```bash
sudo apt install qt6-base-dev qt6-serialport-dev cmake g++
```

## 构建（Linux）

```bash
cd sscom-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/sscom-qt
```

## 构建（Windows + MSVC + Qt6）

1. 安装 [Qt 6](https://www.qt.io/download)（勾选 MSVC 套件与 SerialPort）与 Visual Studio 2022。
2. 打开「x64 Native Tools Command Prompt for VS」，或在 Qt Creator 中打开本目录的 `CMakeLists.txt`。

```bat
cd sscom-qt
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\sscom-qt.exe
```

`CMAKE_PREFIX_PATH` 换成你本机 Qt 安装路径。

## 功能（v0.1）

| 项 | 状态 |
|----|------|
| 端口枚举 / 刷新 | ✅ |
| 打开/关闭（波特率/数据位/校验/停止位） | ✅ |
| RX 显示（文本 / HEX / 时间戳） | ✅ |
| TX（文本 UTF-8 / HEX） | ✅ |
| 清空接收 | ✅ |
| 与 Avalonia 功能对等 | ❌ 进行中 |

## 目录

```
sscom-qt/
├── CMakeLists.txt
├── README.md
├── .gitignore
└── src/
    ├── main.cpp
    ├── MainWindow.h
    └── MainWindow.cpp
```
