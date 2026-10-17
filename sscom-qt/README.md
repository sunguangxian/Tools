# sscom-qt — SSCOM 串口调试助手（Qt6 骨架）

C++ / Qt6 Widgets 重写骨架。当前仅主窗口壳（接收区 / 发送区 / 串口参数），**尚未接入真实串口逻辑**。

工作版 Avalonia 仍在仓库 [`../sscom/`](../sscom/)。大功能暂时冻结在 Avalonia，新开发优先往本目录迁移。

## 依赖

- **CMake** ≥ 3.16
- **Qt 6**（Core / Gui / Widgets）
- **C++17** 编译器（MSVC 2019+、GCC 10+、Clang 12+）

可选后续：`Qt6::SerialPort`（骨架未链接）。

## 构建（Linux）

```bash
sudo apt install qt6-base-dev cmake g++   # Debian/Ubuntu 示例
cd sscom-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/sscom-qt
```

## 构建（Windows + MSVC + Qt6）

1. 安装 [Qt 6](https://www.qt.io/download)（勾选 MSVC 套件）与 Visual Studio 2022（含「使用 C++ 的桌面开发」）。
2. 打开「x64 Native Tools Command Prompt for VS」，或在 Qt Creator 中打开本目录的 `CMakeLists.txt`。

```bat
cd sscom-qt
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\sscom-qt.exe
```

`CMAKE_PREFIX_PATH` 换成你本机 Qt 安装路径。也可用 Qt Creator：打开 `CMakeLists.txt` → 配置 Kit → 构建运行。

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

## 状态

| 项 | 状态 |
|----|------|
| 主窗口布局壳 | ✅ |
| 串口收发 | ❌ 占位 |
| 与 Avalonia 功能对等 | ❌ 进行中 |
