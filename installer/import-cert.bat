@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

:: 自动提权到管理员
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    exit /b
)

echo === 导入 SugiIME 测试证书 ===
echo.

if not exist "SugiIME-Test.cer" (
    echo [错误] 找不到 SugiIME-Test.cer
    echo 请确认此脚本与证书文件在同一目录。
    pause
    exit /b 1
)

echo 正在导入到 Trusted Root Certification Authorities...
certutil -addstore Root "SugiIME-Test.cer"
if %errorLevel% neq 0 (
    echo [错误] 导入 Root 失败
    pause
    exit /b 1
)

echo.
echo 正在导入到 Trusted Publishers...
certutil -addstore TrustedPublisher "SugiIME-Test.cer"
if %errorLevel% neq 0 (
    echo [错误] 导入 TrustedPublisher 失败
    pause
    exit /b 1
)

echo.
echo 证书导入成功！接下来运行 install.bat 安装。
pause
