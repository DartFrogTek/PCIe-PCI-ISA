@echo off
setlocal EnableExtensions
net session >nul 2>&1
if errorlevel 1 (
  echo ERROR: Run this as Administrator.
  exit /b 1
)
set "CFG=%~1"
if "%CFG%"=="" set "CFG=Debug"
set "PLAT=%~2"
if "%PLAT%"=="" set "PLAT=x64"
set "ROOT=%~dp0..\"
set "DIST=%ROOT%dist\%CFG%_%PLAT%"
set "INF=%DIST%\it8888vdma.inf"
if not exist "%INF%" (
  echo ERROR: INF not found: "%INF%"
  echo Run build_all.bat %CFG% %PLAT% first.
  exit /b 1
)
echo Installing IT8888VDMA driver from "%INF%"
pnputil /add-driver "%INF%" /install
if errorlevel 1 exit /b 1

echo.
echo Current matching devices:
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-PnpDevice -PresentOnly | ? { $_.InstanceId -like 'PCI\\VEN_1283&DEV_8888*' -or $_.FriendlyName -like '*IT8888*' } | Format-Table -AutoSize"
echo.
echo Try: "%DIST%\it8888ctl.exe" info
