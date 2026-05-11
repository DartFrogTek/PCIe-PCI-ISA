@echo off
setlocal EnableExtensions
set CFG=%~1
if "%CFG%"=="" set CFG=Release
set PLAT=%~2
if "%PLAT%"=="" set PLAT=x64

set ROOT=%~dp0
if "%ROOT:~-1%"=="\" set ROOT=%ROOT:~0,-1%
set DIST=%ROOT%\dist\%CFG%_%PLAT%
set DRIVER_OUT=%ROOT%\driver\%PLAT%\%CFG%
set TOOL_OUT=%ROOT%\tools\it8888ctl\%PLAT%\%CFG%

if not exist "%DIST%" mkdir "%DIST%"

if exist "%DRIVER_OUT%\it8888vdma.sys" copy /Y "%DRIVER_OUT%\it8888vdma.sys" "%DIST%\" >nul
if exist "%DRIVER_OUT%\it8888vdma.pdb" copy /Y "%DRIVER_OUT%\it8888vdma.pdb" "%DIST%\" >nul
if exist "%ROOT%\driver\it8888vdma.inf" copy /Y "%ROOT%\driver\it8888vdma.inf" "%DIST%\" >nul
if exist "%ROOT%\driver\public.h" copy /Y "%ROOT%\driver\public.h" "%DIST%\" >nul
if exist "%TOOL_OUT%\it8888ctl.exe" copy /Y "%TOOL_OUT%\it8888ctl.exe" "%DIST%\" >nul
if exist "%TOOL_OUT%\it8888ctl.pdb" copy /Y "%TOOL_OUT%\it8888ctl.pdb" "%DIST%\" >nul

echo Staged direct outputs to "%DIST%"
dir "%DIST%" /b

if not exist "%DIST%\it8888vdma.sys" (
  echo ERROR: missing it8888vdma.sys. Run: build_driver_direct.bat %CFG% %PLAT%
  exit /b 1
)
exit /b 0
