@echo off
chcp 65001 >nul

:menu
cls
echo 请选择要执行的Python脚本：
echo.
echo 1. 执行 script1.py
echo 2. 执行 script2.py
echo 3. 执行 数据波形生成器.py
echo 4. 启动 Grok Launcher
echo 5. 猫猫云 Clash 配置解密
echo 6. 退出
echo.
set /p choice=请输入选择的编号（1-6）：

if "%choice%"=="1" (
    python script1.py
    goto end
)
if "%choice%"=="2" (
    python script2.py
    goto end
)
if "%choice%"=="3" (
    python 数据波形生成器.py
    goto end
)
if "%choice%"=="4" (
    call "%~dp0start_grok.bat"
    goto end
)
if "%choice%"=="5" (
    python maomaoyun_clash.py
    goto end
)
if "%choice%"=="6" (
    goto end
)
echo 无效选择，请重新输入
pause >nul
goto menu

:end
echo 任务完成，按任意键退出...
pause >nul
