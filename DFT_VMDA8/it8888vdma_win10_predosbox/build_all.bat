@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "CFG=%~1"
if "%CFG%"=="" set "CFG=Debug"
set "PLAT=%~2"
if "%PLAT%"=="" set "PLAT=x64"

if /I not "%PLAT%"=="x64" (
  echo ERROR: This prototype build script currently supports only x64.
  exit /b 1
)

set "ROOT=%~dp0"
set "LOGDIR=%ROOT%build_logs"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"

call "%ROOT%scripts\find_vs.bat"
if errorlevel 1 goto :fail

call "%VSDEV%"
if errorlevel 1 goto :fail

where msbuild >nul 2>nul
if errorlevel 1 (
  echo ERROR: msbuild not found after loading Visual Studio environment.
  goto :fail
)

call "%ROOT%scripts\find_wdk.bat"
if errorlevel 1 goto :fail

echo Using WDK version: %IT8888_WDK_VERSION%
echo WDK km include:   %IT8888_WDK_KM_INC%
echo WDK km lib:       %IT8888_WDK_KM_LIB%
echo KMDF version:     %IT8888_WDK_KMDF_VERSION%
echo KMDF include:     %IT8888_WDK_WDF_INC%
echo KMDF lib:         %IT8888_WDK_WDF_LIB%
echo KMDF macros:      major=%IT8888_WDK_KMDF_MAJOR% minor=%IT8888_WDK_KMDF_MINOR%

echo ================================================================
echo Building IT8888VDMA driver: %CFG%^|%PLAT%
echo ================================================================
msbuild "%ROOT%driver\it8888vdma.vcxproj" /m /t:Build /p:Configuration=%CFG% /p:Platform=%PLAT% /p:SpectreMitigation=false /p:IT8888_WDK_KM_INC="%IT8888_WDK_KM_INC%" /p:IT8888_WDK_SHARED_INC="%IT8888_WDK_SHARED_INC%" /p:IT8888_WDK_UCRT_INC="%IT8888_WDK_UCRT_INC%" /p:IT8888_WDK_KM_LIB="%IT8888_WDK_KM_LIB%" /p:IT8888_WDK_WDF_INC="%IT8888_WDK_WDF_INC%" /p:IT8888_WDK_WDF_LIB="%IT8888_WDK_WDF_LIB%" /p:IT8888_WDK_KMDF_MAJOR=%IT8888_WDK_KMDF_MAJOR% /p:IT8888_WDK_KMDF_MINOR=%IT8888_WDK_KMDF_MINOR% /fl /flp:logfile="%LOGDIR%\driver_%CFG%_%PLAT%.log" /v:minimal
if errorlevel 1 goto :fail

echo ================================================================
echo Building it8888ctl tool: %CFG%^|%PLAT%
echo ================================================================
msbuild "%ROOT%tools\it8888ctl\it8888ctl.vcxproj" /m /t:Build /p:Configuration=%CFG% /p:Platform=%PLAT% /p:SpectreMitigation=false /fl /flp:logfile="%LOGDIR%\tool_%CFG%_%PLAT%.log" /v:minimal
if errorlevel 1 goto :fail

call "%ROOT%scripts\stage_dist.bat" %CFG% %PLAT%
if errorlevel 1 goto :fail

echo.
echo BUILD OK.
echo Logs: "%LOGDIR%"
echo Dist: "%ROOT%dist\%CFG%_%PLAT%"
exit /b 0

:fail
echo.
echo BUILD FAILED. Check logs under "%LOGDIR%".
exit /b 1
