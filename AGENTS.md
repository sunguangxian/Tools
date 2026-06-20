# Tools 仓库 AI 协作说明

本文件供 OpenAI Codex、Cursor 等编码助手阅读。修改本仓库代码时请遵守以下约定。

## 项目概览

本仓库是一组 Python 工具集，面向嵌入式开发与数据处理（音频、字体、串口、发票等）。主要入口脚本位于仓库根目录或子目录，人类用户通常通过 `run.bat` 菜单或 `python xxx.py` 直接运行。

## 功能变更联动（必须）

**新增或修改任何工具功能时，必须同步更新以下文件，缺一不可：**

1. **`README.md`** — 补充或修正工具说明、依赖、用法、配置项、示例命令。
2. **`run.bat`** — 若工具应通过菜单启动，添加或更新对应菜单项与 `python` 启动命令；若菜单结构变化，同步调整编号与提示文字。
3. **`requirements.txt`** — 新增、升级或移除第三方依赖时同步更新包名与版本约束；注明用途注释（与现有格式一致）。

仅改内部实现、不影响对外行为或使用方式时，可不更新 `run.bat`；但仍需确认 `README.md` 与 `requirements.txt` 描述是否仍然准确。

## Git 提交规范

提交信息**使用中文**，格式遵循 [Conventional Commits](https://www.conventionalcommits.org/)：

```text
<type>(<scope>): <中文简述>

<可选正文：说明变更原因、影响范围、注意事项>
```

### type（必选）

| type | 用途 |
|------|------|
| `feat` | 新功能 |
| `fix` | 缺陷修复 |
| `docs` | 仅文档变更 |
| `refactor` | 重构（不改变外部行为） |
| `test` | 测试相关 |
| `chore` | 构建、脚本、依赖等杂项 |

### scope（推荐）

使用工具或目录名，例如：`serial`、`wav`、`lvgl`、`fapiao`、`runbat`。

### 示例

```text
feat(serial): 添加运行时串口与波特率选择

启动时枚举 COM 口并从常用波特率列表选择；无可用串口时友好退出。
同步更新 README、run.bat 与 requirements.txt。
```

```text
fix(wav): 修复多声道 WAV 转 C 数组时字节序错误
```

```text
docs: 补充串口帧校验工具的对比模式说明
```

### 提交时注意

- 标题一行说清「做了什么」，正文说清「为什么」。
- 一次提交只做一类逻辑变更；功能、文档、菜单、依赖清单应放在同一提交中一并完成。
- **仅在用户明确要求时** 才执行 `git commit` / `git push`。
- 不要提交运行时产物（如 `serial_logs/`、临时输出文件、密钥或 `.env`）。

## 代码风格

- 保持改动范围最小，匹配现有脚本风格（命名、注释密度、GUI 模式）。
- 优先复用已有函数与模式，避免过度抽象。
- 注释只解释非显而易见的业务或协议细节。
- 未要求时不要添加与变更无关的测试或文档。

## 常用命令

```bash
python wav_to_c_array.py
python lvgl_font_tool.py
python serial_tools/serial_frame_check.py
python serial_waveform/serial_waveform_gui.py
python fapiao_check.py
python 数据波形生成器.py
```

安装依赖：

```bash
py -m pip install -r requirements.txt
```
