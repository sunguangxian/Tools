# PythonTools - 嵌入式与数据处理工具集

本仓库包含一组专为嵌入式开发优化的 Python 脚本，旨在简化音频、字体、串口数据及发票校验等日常任务。

---

## 1. WAV <-> C 数组互转工具 (`wav_to_c_array.py`)
**专为嵌入式音频播放设计。**

- **WAV -> C数组**: 批量将目录下的 WAV 文件转换为包含完整 Header 的 C 数组。
- **C数组 -> WAV**: 100% 还原还原 C 文件中的数据到 WAV，确保字节级一致性。
- **GUI 界面**: 支持目录选择和实时转换日志。

---

## 2. LVGL 字体助手 (`lvgl_font_tool.py`)
**LVGL 界面开发的字库瘦身与管理利器。**

- **逆向解析**: 能够从现有的 LVGL `.c` 字体文件中“捞出”已有的字符和图标码点。
- **源码扫描**: 自动递归扫描项目目录，提取代码中出现的所有汉字。
- **增量更新**: 在提取结果基础上，支持手动编辑或输入 Unicode 码点追加图标（如 FontAwesome）。
- **官方转换**: 一键调用 `lv_font_conv` 生成标准 C 字库。
- **前提条件**: 需安装 Node.js 及其转换工具：`npm install -g lv_font_conv`。

---

## 3. 串口波形显示 (`serial_waveform/`)
**实时串口数据可视化工具。**

- **功能**: 接收串口数据并实时绘制波形，支持多通道显示。
- **运行**: `python serial_waveform/serial_waveform_gui.py`

---

## 4. 串口长度帧校验工具 (`serial_tools/serial_frame_check.py`)
**用于校验 DT/PCM/DATA 等“长度前缀 + 二进制 payload”串口协议。**

该工具不会按普通串口助手的“每次收到多少字节”来判断数据是否正常，而是把串口当成连续字节流处理，按协议重新组帧：

```text
+DT=<len>,<payload>\r\n
```

例如 `EXPECT_LEN = 320` 时，一帧大约为：

```text
len("+DT=320,") + 320 + len("\r\n") = 330 bytes
```

### 主要功能
- **长度组帧校验**: 自动处理半帧、粘包、前导垃圾字节，并按长度字段提取完整 payload。
- **接收线程**: 默认启用独立 RX 线程，避免打印日志、写文件或数据对比阻塞串口读取。
- **日志记录**: 输出系统时间、相对时间、帧间隔、CRC32、前几个字节、`dropped` 和 `bad` 统计。
- **CSV 输出**: 自动生成 CSV，方便用 Excel 或脚本分析帧间隔、错误数量和对比结果。
- **数据对比**: 支持 `none`、`first`、`file_loop`、`file_seq`、`seq` 五种对比模式。
- **协议可配置**: 默认检测 `+DT=`，也可以通过修改脚本顶部配置检测 `+PCM=`、`+DATA=` 等类似协议。

### 使用方法
先安装依赖：

```bash
py -m pip install pyserial
```

打开脚本，修改顶部配置区：

```python
PORT = "COM7"
BAUD = 230400
PREFIX = b"+DT="
EXPECT_LEN = 320
COMPARE_MODE = "none"
```

运行：

```bash
py serial_tools/serial_frame_check.py
```

### 判断标准
- 持续输出 `OK len=320`，且 `bad` / `dropped` 不增加，说明串口协议格式基本正常。串口助手看到“粘包/半包”不代表设备发送异常。
- `bad` 或 `dropped` 持续增加，说明字节流确实有格式错误、丢字节、波特率/线材/电平/流控等问题。
- 偶尔或持续 `LEN_BAD`，优先确认脚本里的 `EXPECT_LEN` 是否和设备端 `PCM_FRAME_SIZE` 一致。
- 如果启用 `seq` 或 `file_seq` 对比后出现跳变/错位，说明可能存在丢帧、重复帧、漏收或参考数据不一致。

---

## 5. 数据波形生成器 (`数据波形生成器.py`)
用于生成正弦波、方波等特定规律的数据，支持导出。

---

## 6. 发票检查工具 (`fapiao_check.py`)
快速校验发票信息。

---

## 环境要求
- **Python 3.x**
- **标准库**: `tkinter`, `re`, `os`, `subprocess` 等。
- **第三方库**:
  - 串口工具需安装: `pip install pyserial`
  - 若运行 `lvgl_font_tool.py` 且需要预览字体，可能需要 `pip install Pillow`。

## 运行方式
所有工具均支持直接运行：
```bash
python [文件名].py
```
