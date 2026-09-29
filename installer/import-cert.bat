@echo off
cd /d "%~dp0"

:: Auto-elevate to admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    exit /b
)

echo === Import SugiIME Test Certificate ===
echo.

if not exist "SugiIME-Test.cer" (
    echo [ERROR] SugiIME-Test.cer not found.
    echo Make sure this script and the .cer file are in the same folder.
    pause
    exit /b 1
)

echo Importing to Trusted Root Certification Authorities...
certutil -addstore Root "SugiIME-Test.cer"
if %errorLevel% neq 0 (
    echo [ERROR] Failed to import to Root store.
    pause
    exit /b 1
)

echo.
echo Importing to Trusted Publishers...
certutil -addstore TrustedPublisher "SugiIME-Test.cer"
if %errorLevel% neq 0 (
    echo [ERROR] Failed to import to TrustedPublisher store.
    pause
    exit /b 1
)

echo.
echo Certificate imported successfully. Now run install.bat
pause
