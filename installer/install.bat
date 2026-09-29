@echo off
cd /d "%~dp0"

:: Auto-elevate to admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    exit /b
)

echo === Install SugiIME ===
echo.

set "installer="
for %%f in (*Setup*.exe) do set "installer=%%f"

if "%installer%"=="" (
    echo [ERROR] No installer .exe found.
    echo Make sure this script and the installer .exe are in the same folder.
    pause
    exit /b 1
)

echo Running installer: %installer%
"%installer%"

echo.
echo Installation complete. Please restart or log off to use SugiIME.
pause
