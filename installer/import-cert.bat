@echo off
chcp 65001 >nul
cd /d "%~dp0"

:: ============================================================
::  SugiIME 测试证书导入脚本
::  将 SugiIME-Test.cer 导入到"受信任的根证书颁发机构"
::  和"受信任的发布者"，以便安装包能通过 Windows 签名校验。
:: ============================================================

:: ---- 自动提权到管理员 ----
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo 请求管理员权限...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ========================================
echo   SugiIME 测试证书导入
echo ========================================
echo.

:: ---- 查找证书文件 ----
set "CER_FILE=%~1"
if "%CER_FILE%"=="" set "CER_FILE=SugiIME-Test.cer"

if not exist "%CER_FILE%" (
    echo [错误] 找不到证书文件：%CER_FILE%
    echo.
    echo 请将 SugiIME-Test.cer 放在本脚本同目录，
    echo 或将 .cer 文件拖到本脚本上运行。
    echo.
    pause
    exit /b 1
)

echo 证书文件：%CER_FILE%
echo.

:: ---- 导入到受信任的根证书颁发机构 ----
echo [1/2] 导入到"受信任的根证书颁发机构"...
certutil -addstore Root "%CER_FILE%"
if %errorLevel% neq 0 (
    echo.
    echo [错误] 导入根证书失败。
    pause
    exit /b 1
)
echo 成功。
echo.

:: ---- 导入到受信任的发布者 ----
echo [2/2] 导入到"受信任的发布者"...
certutil -addstore TrustedPublisher "%CER_FILE%"
if %errorLevel% neq 0 (
    echo.
    echo [错误] 导入受信任发布者失败。
    pause
    exit /b 1
)
echo 成功。
echo.

echo ========================================
echo   证书导入完成！
echo   现在可以运行 install.bat 安装输入法。
echo ========================================
echo.
pause
