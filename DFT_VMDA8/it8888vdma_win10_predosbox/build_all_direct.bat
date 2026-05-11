@echo off
setlocal EnableExtensions EnableDelayedExpansion

set CFG=%~1
if "%CFG%"=="" set CFG=Debug
set PLAT=%~2
if "%PLAT%"=="" set PLAT=x64

set ROOT=%~dp0
if "%ROOT:~-1%"=="\" set ROOT=%ROOT:~0,-1%
set DIST=%ROOT%\dist\%CFG%_%PLAT%

rem First build tool/project using the existing MSBuild path. This may still warn about .sys staging;
rem the direct driver build below is authoritative for the .sys.
call "%ROOT%\build_all.bat" %CFG% %PLAT%
if errorlevel 1 goto fail

rem Build real kernel .sys directly.
call "%ROOT%\build_driver_direct.bat" %CFG% %PLAT%
if errorlevel 1 goto fail

if not exist "%DIST%" mkdir "%DIST%"

set DRIVER_OUT=%ROOT%\driver\%PLAT%\%CFG%
set TOOL_OUT=%ROOT%\tools\it8888ctl\%PLAT%\%CFG%

if exist "%DRIVER_OUT%\it8888vdma.sys" copy /Y "%DRIVER_OUT%\it8888vdma.sys" "%DIST%\" >nul
if exist "%DRIVER_OUT%\it8888vdma.pdb" copy /Y "%DRIVER_OUT%\it8888vdma.pdb" "%DIST%\" >nul
if exist "%ROOT%\driver\it8888vdma.inf" copy /Y "%ROOT%\driver\it8888vdma.inf" "%DIST%\" >nul
if exist "%ROOT%\driver\public.h" copy /Y "%ROOT%\driver\public.h" "%DIST%\" >nul
if exist "%TOOL_OUT%\it8888ctl.exe" copy /Y "%TOOL_OUT%\it8888ctl.exe" "%DIST%\" >nul
if exist "%TOOL_OUT%\it8888ctl.pdb" copy /Y "%TOOL_OUT%\it8888ctl.pdb" "%DIST%\" >nul

echo.
echo Direct staged files:
dir "%DIST%" /b

if not exist "%DIST%\it8888vdma.sys" (
    echo ERROR: direct driver sys was not staged: "%DIST%\it8888vdma.sys"
    echo Expected source: "%DRIVER_OUT%\it8888vdma.sys"
    goto fail
)
if not exist "%DIST%\it8888ctl.exe" (
    echo ERROR: tool exe was not staged: "%DIST%\it8888ctl.exe"
    goto fail
)
if not exist "%DIST%\it8888vdma.inf" (
    echo ERROR: INF was not staged: "%DIST%\it8888vdma.inf"
    goto fail
)

echo.
echo BUILD DIRECT OK.
echo Dist: "%DIST%"
exit /b 0

:fail
echo.
echo BUILD DIRECT FAILED.
exit /b 1
