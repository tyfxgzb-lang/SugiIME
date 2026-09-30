@echo off
chcp 65001 >nul
cd /d "%~dp0"

:: ============================================================
::  SugiIME 安装启动脚本
::  自动查找同目录下的安装包 .exe 并以管理员权限运行。
:: ============================================================

:: ---- 自动提权到管理员 ----
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo 请求管理员权限...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ========================================
echo   安装 SugiIME 水杉日语输入法
echo ========================================
echo.

:: ---- 查找安装包 ----
set "installer="
for %%f in (*Setup*.exe) do set "installer=%%f"

if "%installer%"=="" (
    echo [错误] 找不到安装包（*Setup*.exe）。
    echo 请确认安装包 .exe 与本脚本在同一文件夹。
    echo.
    pause
    exit /b 1
)

echo 找到安装包：%installer%
echo 启动安装程序...
echo.

"%installer%"

echo.
echo ========================================
echo   安装完成。
echo   请注销或重启后使用 SugiIME。
echo ========================================
echo.
pause
