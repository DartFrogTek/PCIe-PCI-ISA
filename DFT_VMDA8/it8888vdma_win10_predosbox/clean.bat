@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"

call "%ROOT%scripts\find_vs.bat"
if errorlevel 1 exit /b 1

rem vcvars64.bat is already the x64 environment. Do not pass -arch/-host_arch.
call "%VSDEV%"
if errorlevel 1 exit /b 1

where msbuild >nul 2>nul
if errorlevel 1 (
  echo ERROR: msbuild not found after loading Visual Studio environment.
  exit /b 1
)

msbuild "%ROOT%driver\it8888vdma.vcxproj" /t:Clean /p:Configuration=Debug /p:Platform=x64 /p:SpectreMitigation=false
msbuild "%ROOT%driver\it8888vdma.vcxproj" /t:Clean /p:Configuration=Release /p:Platform=x64 /p:SpectreMitigation=false
msbuild "%ROOT%tools\it8888ctl\it8888ctl.vcxproj" /t:Clean /p:Configuration=Debug /p:Platform=x64 /p:SpectreMitigation=false
msbuild "%ROOT%tools\it8888ctl\it8888ctl.vcxproj" /t:Clean /p:Configuration=Release /p:Platform=x64 /p:SpectreMitigation=false

if exist "%ROOT%build_logs" rmdir /s /q "%ROOT%build_logs"
if exist "%ROOT%dist" rmdir /s /q "%ROOT%dist"
echo Clean complete.
