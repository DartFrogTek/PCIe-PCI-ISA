@echo off
setlocal EnableExtensions

set "CONFIG=%~1"
set "PLAT=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"
if "%PLAT%"=="" set "PLAT=x64"

set "ROOT=%~dp0..\"
for %%I in ("%ROOT%") do set "ROOT=%%~fI\"
set "DIST=%ROOT%dist\%CONFIG%_%PLAT%"

if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"

set "DRVOUT=%ROOT%driver\%PLAT%\%CONFIG%"
set "TOOLOUT=%ROOT%tools\it8888ctl\%PLAT%\%CONFIG%"

if exist "%DRVOUT%\it8888vdma.sys" copy /y "%DRVOUT%\it8888vdma.sys" "%DIST%\" >nul
if exist "%DRVOUT%\it8888vdma.pdb" copy /y "%DRVOUT%\it8888vdma.pdb" "%DIST%\" >nul
if exist "%DRVOUT%\it8888vdma.cat" copy /y "%DRVOUT%\it8888vdma.cat" "%DIST%\" >nul
if exist "%ROOT%driver\it8888vdma.inf" copy /y "%ROOT%driver\it8888vdma.inf" "%DIST%\" >nul
if exist "%ROOT%driver\public.h" copy /y "%ROOT%driver\public.h" "%DIST%\" >nul

if exist "%TOOLOUT%\it8888ctl.exe" copy /y "%TOOLOUT%\it8888ctl.exe" "%DIST%\" >nul
if exist "%TOOLOUT%\it8888ctl.pdb" copy /y "%TOOLOUT%\it8888ctl.pdb" "%DIST%\" >nul

if not exist "%DIST%\it8888vdma.sys" (
  echo WARNING: it8888vdma.sys was not staged from "%DRVOUT%".
  echo Search with: dir . -Recurse -Filter it8888vdma.sys
) else (
  echo staged it8888vdma.sys
)

if not exist "%DIST%\it8888ctl.exe" (
  echo WARNING: it8888ctl.exe was not staged from "%TOOLOUT%".
) else (
  echo staged it8888ctl.exe
)

echo Staged files in "%DIST%"
endlocal
