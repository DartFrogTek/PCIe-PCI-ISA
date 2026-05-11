@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo ERROR: Run this as Administrator.
  exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall_driver_admin.ps1"
