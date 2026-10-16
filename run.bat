@echo off
chcp 65001 >nul

:menu
cls
echo 请选择要执行的工具：
echo.
echo 1. WAV 与 C 数组互转
echo 2. LVGL 字体助手
echo 3. 串口波形显示
echo 4. 串口长度帧校验
echo 5. 数据波形绘图
echo 6. 发票查重
echo 7. 启动 Grok / Codex / Gemini
echo 8. 猫猫云 Clash 配置解密
echo 9. USB 串口挂到 WSL
echo 0. 退出
echo.
set /p choice=请输入选择的编号（0-9）：

if "%choice%"=="1" (
    python wav_to_c_array.py
    goto end
)
if "%choice%"=="2" (
    python lvgl_font_tool.py
    goto end
)
if "%choice%"=="3" (
    python serial_waveform/serial_waveform_gui.py
    goto end
)
if "%choice%"=="4" (
    python serial_tools/serial_frame_check.py
    goto end
)
if "%choice%"=="5" (
    python 数据波形生成器.py
    goto end
)
if "%choice%"=="6" (
    python fapiao_check.py
    goto end
)
if "%choice%"=="7" (
    call "%~dp0start_grok.bat"
    goto end
)
if "%choice%"=="8" (
    python maomaoyun_clash.py
    goto end
)
if "%choice%"=="9" (
    powershell -ExecutionPolicy Bypass -File "%~dp0usb_serial-wsl.ps1"
    goto end
)
if "%choice%"=="0" (
    goto end
)
echo 无效选择，请重新输入
pause >nul
goto menu

:end
echo 任务完成，按任意键退出...
pause >nul
