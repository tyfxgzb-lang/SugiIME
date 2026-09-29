@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

:: 自动提权到管理员
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    exit /b
)

echo === 安装 SugiIME 日语输入法 ===
echo.

set "installer="
for %%f in (*Setup*.exe) do set "installer=%%f"

if "%installer%"=="" (
    echo [错误] 找不到安装包
    echo 请确认此脚本与安装 .exe 在同一目录。
    pause
    exit /b 1
)

echo 正在运行安装程序：%installer%
"%installer%"

echo.
echo 安装完成。请重启或注销后使用 SugiIME 日语输入法。
pause
