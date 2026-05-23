@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install-user.ps1" -BuildDir "%~dp0build-input-hardening" -Config DEBUG
pause
