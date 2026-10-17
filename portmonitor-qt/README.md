# portmonitor-qt — PortMonitor 端口监视（Qt6 骨架）

C++ / Qt6 Widgets 重写骨架。当前仅主窗口壳（左侧会话列表 + 右侧日志表），**尚未接入旁路监视 / Modbus / USB 逻辑**。

工作版 Avalonia 仍在仓库 [`../PortMonitor/`](../PortMonitor/)。大功能暂时冻结在 Avalonia，新开发优先往本目录迁移。

本目录与 Avalonia 的 `PortMonitor/` **并列**于 Tools 根下（不嵌套在 Avalonia 树内），便于独立演进。

## 依赖

- **CMake** ≥ 3.16
- **Qt 6**（Core / Gui / Widgets）
- **C++17** 编译器（MSVC 2019+、GCC 10+、Clang 12+）

## 构建（Linux）

```bash
sudo apt install qt6-base-dev cmake g++   # Debian/Ubuntu 示例
cd portmonitor-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/portmonitor-qt
```

## 构建（Windows + MSVC + Qt6）

1. 安装 [Qt 6](https://www.qt.io/download)（勾选 MSVC 套件）与 Visual Studio 2022。
2. 在「x64 Native Tools」或 Qt Creator 中构建：

```bat
cd portmonitor-qt
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\portmonitor-qt.exe
```

`CMAKE_PREFIX_PATH` 换成你本机 Qt 安装路径。

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

## 状态

| 项 | 状态 |
|----|------|
| 主窗口布局壳 | ✅ |
| 多会话 / 旁路监视 | ❌ 占位 |
| 与 Avalonia 功能对等 | ❌ 进行中 |
