@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo ERROR: Run this as Administrator.
  exit /b 1
)
set "CFG=%~1"
if "%CFG%"=="" set "CFG=Debug"
set "PLAT=%~2"
if "%PLAT%"=="" set "PLAT=x64"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make_test_cert_and_sign_admin.ps1" -Configuration "%CFG%" -Platform "%PLAT%"
