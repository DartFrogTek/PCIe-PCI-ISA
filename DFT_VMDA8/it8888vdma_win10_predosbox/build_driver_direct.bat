@echo off
setlocal EnableExtensions EnableDelayedExpansion

set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Debug
set PLAT=%~2
if "%PLAT%"=="" set PLAT=x64

set ROOT=%~dp0
set ROOT=%ROOT:~0,-1%
set LOGDIR=%ROOT%\build_logs
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
set LOG=%LOGDIR%\driver_direct_%CONFIG%_%PLAT%.log

call "%ROOT%\scripts\find_vs.bat" >nul || exit /b 1
if not defined VSDEV (
  echo ERROR: VSDEV not set by scripts\find_vs.bat
  exit /b 1
)

echo Using VS environment: %VSDEV%
call "%VSDEV%" >nul || exit /b 1

call "%ROOT%\scripts\find_wdk.bat" || exit /b 1

echo Using WDK version: %IT8888_WDK_VERSION%
echo KMDF version:     %IT8888_WDK_KMDF_VERSION%  major=%IT8888_WDK_KMDF_MAJOR% minor=%IT8888_WDK_KMDF_MINOR%

set OUTDIR=%ROOT%\driver\x64\%CONFIG%
set OBJDIR=%ROOT%\driver\obj_direct\%CONFIG%_%PLAT%
if not exist "%OUTDIR%" mkdir "%OUTDIR%"
if not exist "%OBJDIR%" mkdir "%OBJDIR%"

set SYS=%OUTDIR%\it8888vdma.sys
set PDB=%OUTDIR%\it8888vdma.pdb

echo Output:           %SYS%

pushd "%ROOT%\driver" || exit /b 1

set INC=/I"%IT8888_WDK_WDF_INC%" /I"%IT8888_WDK_KM_INC%" /I"%IT8888_WDK_SHARED_INC%" /I"%IT8888_WDK_UCRT_INC%"
set DEF=/D_AMD64_ /DAMD64 /D_WIN64 /DWIN64 /DNTDDI_VERSION=0x0A000000 /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DDBG=1 /DDEBUG=1 /DKMDF_VERSION_MAJOR=%IT8888_WDK_KMDF_MAJOR% /DKMDF_VERSION_MINOR=%IT8888_WDK_KMDF_MINOR%
set CFLAGS=/nologo /c /Zi /W3 /WX- /kernel /GS /Gz /GR- /EHs-c- /Oi /Oy- /Fo"%OBJDIR%\\" /Fd"%OBJDIR%\it8888vdma.pdb" %INC% %DEF%

break > "%LOG%"
for %%F in (driver.c device.c queue.c it8888_pci.c it8888_io.c it8888_dma.c vdma8237.c ddma.c trace.c) do (
  echo cl %%F
  echo ==== cl %%F ====>>"%LOG%"
  cl %CFLAGS% %%F >>"%LOG%" 2>&1
  if errorlevel 1 (
    echo ERROR: compile failed: %%F. See "%LOG%"
    popd
    exit /b 1
  )
)

echo link it8888vdma.sys
>>"%LOG%" echo ==== link ==== 

set LIBPATHS=/LIBPATH:"%IT8888_WDK_KM_LIB%" /LIBPATH:"%IT8888_WDK_WDF_LIB%"
set LIBS=ntoskrnl.lib hal.lib wmilib.lib BufferOverflowK.lib WdfDriverEntry.lib WdfLdr.lib wdmguid.lib
set LFLAGS=/nologo /OUT:"%SYS%" /PDB:"%PDB%" /SUBSYSTEM:NATIVE /DRIVER /NODEFAULTLIB /ENTRY:FxDriverEntry /MACHINE:X64 /DEBUG %LIBPATHS%

link %LFLAGS% "%OBJDIR%\driver.obj" "%OBJDIR%\device.obj" "%OBJDIR%\queue.obj" "%OBJDIR%\it8888_pci.obj" "%OBJDIR%\it8888_io.obj" "%OBJDIR%\it8888_dma.obj" "%OBJDIR%\vdma8237.obj" "%OBJDIR%\ddma.obj" "%OBJDIR%\trace.obj" %LIBS% >>"%LOG%" 2>&1
if errorlevel 1 (
  echo ERROR: link failed. See "%LOG%"
  popd
  exit /b 1
)

popd

if not exist "%SYS%" (
  echo ERROR: expected output missing: "%SYS%"
  exit /b 1
)

echo Built: %SYS%
exit /b 0

