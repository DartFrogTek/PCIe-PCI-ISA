@echo off
rem Finds a Visual Studio environment batch file and exposes VSDEV to caller.
rem Prefer vcvars64.bat because it is stable for plain x64 native builds.
setlocal EnableExtensions EnableDelayedExpansion

set "VSDEV="
set "VSWHERE1=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSWHERE2=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE1%" set "VSWHERE=%VSWHERE1%"
if not defined VSWHERE if exist "%VSWHERE2%" set "VSWHERE=%VSWHERE2%"

if defined VSWHERE (
  for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "VSDEV=%%I\VC\Auxiliary\Build\vcvars64.bat"
  )
)

if not defined VSDEV (
  for %%D in (
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  ) do (
    if exist "%%~D" set "VSDEV=%%~D"
  )
)

if not defined VSDEV (
  echo ERROR: Could not find Visual Studio vcvars64.bat.
  echo Install Visual Studio 2022 with Desktop C++ and Windows 10 WDK integration.
  endlocal & exit /b 1
)

echo Using VS environment: %VSDEV%
endlocal & set "VSDEV=%VSDEV%" & exit /b 0
