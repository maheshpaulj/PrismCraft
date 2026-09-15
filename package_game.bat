@echo off
setlocal
cd /d "%~dp0"
echo ========================================================
echo PrismCraft Standalone Exporter
echo ========================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package_game.ps1" %*
echo.
pause
