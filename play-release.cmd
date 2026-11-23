@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\play-release.ps1" %*
if errorlevel 1 (
    echo Release build or game failed. See the error above.
    pause
    exit /b 1
)
