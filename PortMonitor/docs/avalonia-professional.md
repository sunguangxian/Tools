# Avalonia 专业暗色版说明

- 主题：FluentTheme Dark + Compact + Visual Studio 色板（共享库 [`shared/St.Theme`](../shared/St.Theme/)（`avares://St.Theme/Themes/ProfessionalDark.axaml`））
- 布局：CommMonitor 风格（菜单 / 图标工具栏 / 左树+新建会话 / 筛选 / 主视图 / 底栏详情·发送·统计·端口事件·注入说明）
- 注入：UI 占位；真正注入用 `SerialTapHook.dll`（C++ MinHook，无需 EV 签名）
- 单文件：`publish/win-x64-single/PortMonitor.exe`
