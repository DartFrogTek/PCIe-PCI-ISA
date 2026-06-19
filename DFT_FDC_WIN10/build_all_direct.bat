@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem DFT_FDC_WIN10 pass-1 direct build
rem This intentionally does NOT use the VS/WDK "WindowsKernelModeDriver10.0"
rem MSBuild platform toolset. The IT8888 build path already proved the better
rem approach on this machine: vcvars64 + explicit WDK include/lib discovery.

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "PLATFORM=%~2"
if "%PLATFORM%"=="" set "PLATFORM=x64"

if /I not "%PLATFORM%"=="x64" (
    echo ERROR: only x64 is supported by this pass-1 build.
    exit /b 1
)

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "VSVCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VSVCVARS%" set "VSVCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VSVCVARS%" set "VSVCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VSVCVARS%" (
    echo ERROR: Could not find VS 2022 vcvars64.bat
    exit /b 1
)

call "%VSVCVARS%"
if errorlevel 1 exit /b 1

set "KITSROOT=%ProgramFiles(x86)%\Windows Kits\10"
if not exist "%KITSROOT%\Include" (
    echo ERROR: Windows 10 WDK/SDK include root not found: "%KITSROOT%\Include"
    exit /b 1
)

set "WDKVER="
for /f "delims=" %%D in ('dir /b /ad "%KITSROOT%\Include\10.*" 2^>nul ^| sort /r') do (
    if not defined WDKVER if exist "%KITSROOT%\Include\%%D\km\ntddk.h" set "WDKVER=%%D"
)
if not defined WDKVER (
    echo ERROR: Could not find WDK km headers under "%KITSROOT%\Include\10.*\km".
    echo        Install the Windows Driver Kit, or verify ntddk.h exists.
    exit /b 1
)

set "WDKINC_KM=%KITSROOT%\Include\%WDKVER%\km"
set "WDKINC_SHARED=%KITSROOT%\Include\%WDKVER%\shared"
set "WDKINC_KMCRT=%KITSROOT%\Include\%WDKVER%\km\crt"
set "WDKLIB_KM=%KITSROOT%\Lib\%WDKVER%\km\x64"

if not exist "%WDKLIB_KM%\ntoskrnl.lib" (
    echo ERROR: WDK km x64 lib path missing ntoskrnl.lib: "%WDKLIB_KM%"
    exit /b 1
)

set "KMDFINC="
if exist "%KITSROOT%\Include\wdf\kmdf\wdf.h" set "KMDFINC=%KITSROOT%\Include\wdf\kmdf"
if not defined KMDFINC (
    for /f "delims=" %%D in ('dir /b /ad "%KITSROOT%\Include\wdf\kmdf" 2^>nul ^| sort /r') do (
        if not defined KMDFINC if exist "%KITSROOT%\Include\wdf\kmdf\%%D\wdf.h" (
            set "KMDFVER=%%D"
            set "KMDFINC=%KITSROOT%\Include\wdf\kmdf\%%D"
        )
    )
)
if not defined KMDFINC (
    echo ERROR: Could not find KMDF wdf.h under "%KITSROOT%\Include\wdf\kmdf".
    echo        Install the KMDF component of the WDK.
    exit /b 1
)
if not defined KMDFVER set "KMDFVER=default"

set "KMDFLIB="
if not "%KMDFVER%"=="default" if exist "%KITSROOT%\Lib\wdf\kmdf\x64\%KMDFVER%\WdfDriverEntry.lib" set "KMDFLIB=%KITSROOT%\Lib\wdf\kmdf\x64\%KMDFVER%\WdfDriverEntry.lib"
if not defined KMDFLIB if not "%KMDFVER%"=="default" if exist "%KITSROOT%\Lib\wdf\kmdf\%KMDFVER%\x64\WdfDriverEntry.lib" set "KMDFLIB=%KITSROOT%\Lib\wdf\kmdf\%KMDFVER%\x64\WdfDriverEntry.lib"
if not defined KMDFLIB if exist "%KITSROOT%\Lib\wdf\kmdf\x64\WdfDriverEntry.lib" set "KMDFLIB=%KITSROOT%\Lib\wdf\kmdf\x64\WdfDriverEntry.lib"
if not defined KMDFLIB (
    for /f "delims=" %%F in ('dir /b /s "%KITSROOT%\Lib\wdf\kmdf\WdfDriverEntry.lib" 2^>nul') do (
        if not defined KMDFLIB echo Found WdfDriverEntry.lib: %%F
        if not defined KMDFLIB set "KMDFLIB=%%F"
    )
)
if not defined KMDFLIB (
    for /f "delims=" %%F in ('dir /b /s "%KITSROOT%\Lib\wdf\kmdf\*\WdfDriverEntry.lib" 2^>nul') do (
        if not defined KMDFLIB echo Found WdfDriverEntry.lib: %%F
        if not defined KMDFLIB set "KMDFLIB=%%F"
    )
)
if not defined KMDFLIB (
    echo ERROR: Could not find WdfDriverEntry.lib under "%KITSROOT%\Lib\wdf\kmdf".
    exit /b 1
)

rem WdfDriverEntry.lib is only the KMDF stub/entry library. The stub also
rem needs WdfLdr.lib, otherwise link.exe fails on WdfVersionBind,
rem WdfLdrQueryInterface, WdfVersionUnbind, etc.
set "KMDFLDRLIB="
for %%P in ("%KMDFLIB%") do set "KMDFLIBDIR=%%~dpP"
if defined KMDFLIBDIR if exist "%KMDFLIBDIR%WdfLdr.lib" set "KMDFLDRLIB=%KMDFLIBDIR%WdfLdr.lib"
if not defined KMDFLDRLIB if not "%KMDFVER%"=="default" if exist "%KITSROOT%\Lib\wdf\kmdf\x64\%KMDFVER%\WdfLdr.lib" set "KMDFLDRLIB=%KITSROOT%\Lib\wdf\kmdf\x64\%KMDFVER%\WdfLdr.lib"
if not defined KMDFLDRLIB if not "%KMDFVER%"=="default" if exist "%KITSROOT%\Lib\wdf\kmdf\%KMDFVER%\x64\WdfLdr.lib" set "KMDFLDRLIB=%KITSROOT%\Lib\wdf\kmdf\%KMDFVER%\x64\WdfLdr.lib"
if not defined KMDFLDRLIB if exist "%KITSROOT%\Lib\wdf\kmdf\x64\WdfLdr.lib" set "KMDFLDRLIB=%KITSROOT%\Lib\wdf\kmdf\x64\WdfLdr.lib"
if not defined KMDFLDRLIB (
    for /f "delims=" %%F in ('dir /b /s "%KITSROOT%\Lib\wdf\kmdf\WdfLdr.lib" 2^>nul') do (
        if not defined KMDFLDRLIB echo Found WdfLdr.lib: %%F
        if not defined KMDFLDRLIB set "KMDFLDRLIB=%%F"
    )
)
if not defined KMDFLDRLIB (
    for /f "delims=" %%F in ('dir /b /s "%KITSROOT%\Lib\wdf\kmdf\*\WdfLdr.lib" 2^>nul') do (
        if not defined KMDFLDRLIB echo Found WdfLdr.lib: %%F
        if not defined KMDFLDRLIB set "KMDFLDRLIB=%%F"
    )
)
if not defined KMDFLDRLIB (
    echo ERROR: Could not find WdfLdr.lib under "%KITSROOT%\Lib\wdf\kmdf".
    echo        WdfDriverEntry.lib was found, but the matching KMDF loader import lib was not.
    exit /b 1
)


set "BO_LIB="
if exist "%WDKLIB_KM%\BufferOverflowFastFailK.lib" set "BO_LIB=%WDKLIB_KM%\BufferOverflowFastFailK.lib"
if not defined BO_LIB if exist "%WDKLIB_KM%\BufferOverflowK.lib" set "BO_LIB=%WDKLIB_KM%\BufferOverflowK.lib"

set "DRV_OUT=%ROOT%\driver\x64\%CONFIG%"
set "DRV_OBJ=%ROOT%\driver\dftfdc\x64\%CONFIG%"
set "TOOL_OUT=%ROOT%\tools\dftfdcctl\x64\%CONFIG%"
set "TOOL_OBJ=%ROOT%\tools\dftfdcctl\dftfdcctl\x64\%CONFIG%"
set "DIST=%ROOT%\dist\%CONFIG%_x64"

if not exist "%DRV_OUT%" mkdir "%DRV_OUT%"
if not exist "%DRV_OBJ%" mkdir "%DRV_OBJ%"
if not exist "%TOOL_OUT%" mkdir "%TOOL_OUT%"
if not exist "%TOOL_OBJ%" mkdir "%TOOL_OBJ%"
if not exist "%DIST%" mkdir "%DIST%"

echo.
echo Using WDK version: %WDKVER%
echo WDK km include: "%WDKINC_KM%"
echo WDK km lib:     "%WDKLIB_KM%"
echo KMDF include:   "%KMDFINC%"
echo KMDF entry lib: "%KMDFLIB%"
echo KMDF loader lib: "%KMDFLDRLIB%"
echo.

set "COMMON_DEFS=/D_AMD64_ /DAMD64 /DWINNT=1 /DNTDDI_VERSION=0x0A000000 /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /DUNICODE /D_UNICODE"
if /I "%CONFIG%"=="Debug" (
    set "CFG_DEFS=/DDBG=1"
    set "OPT_FLAGS=/Od /Zi"
) else (
    set "CFG_DEFS=/DNDEBUG"
    set "OPT_FLAGS=/O2 /Zi"
)

set "KERNEL_INCS=/I"%ROOT%\driver" /I"%ROOT%\include" /I"%WDKINC_KM%" /I"%WDKINC_SHARED%" /I"%WDKINC_KMCRT%" /I"%KMDFINC%""
set "KERNEL_CFLAGS=/nologo /c /TC /W3 /WX- /Zl /GS /GR- /Gz /Oi /kernel /FC %OPT_FLAGS% %COMMON_DEFS% %CFG_DEFS% %KERNEL_INCS%"

set "DRIVER_SRCS=driver.c device.c queue.c interrupt.c trace.c it8888_pci.c it8888_io.c it8888_ddma.c fdc_hw.c fdc_cmd.c fdc_dma.c fdc_media.c fdc_rw.c"
set "OBJLIST="

echo Building dftfdc.sys objects...
for %%S in (%DRIVER_SRCS%) do (
    echo   %%S
    cl %KERNEL_CFLAGS% /Fo"%DRV_OBJ%\%%~nS.obj" /Fd"%DRV_OUT%\dftfdc.pdb" "%ROOT%\driver\%%S"
    if errorlevel 1 exit /b 1
    set "OBJLIST=!OBJLIST! "%DRV_OBJ%\%%~nS.obj""
)

echo Linking dftfdc.sys...
set "LINK_LIBS="%WDKLIB_KM%\ntoskrnl.lib" "%WDKLIB_KM%\hal.lib" "%KMDFLIB%" "%KMDFLDRLIB%""
if defined BO_LIB set "LINK_LIBS=%LINK_LIBS% "%BO_LIB%""

link /nologo /driver /subsystem:native /machine:x64 /entry:FxDriverEntry /nodefaultlib /debug /incremental:no /out:"%DRV_OUT%\dftfdc.sys" /pdb:"%DRV_OUT%\dftfdc.pdb" %OBJLIST% %LINK_LIBS%
if errorlevel 1 exit /b 1

echo Building dftfdcctl.exe...
set "TOOL_DEFS=/DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE /D_CONSOLE"
if /I "%CONFIG%"=="Debug" (
    set "TOOL_OPT=/Od /Zi /MDd"
) else (
    set "TOOL_OPT=/O2 /Zi /MD"
)
cl /nologo /TC /W3 /WX- %TOOL_OPT% %TOOL_DEFS% /I"%ROOT%\include" /Fo"%TOOL_OBJ%\main.obj" /Fd"%TOOL_OUT%\dftfdcctl.pdb" /Fe"%TOOL_OUT%\dftfdcctl.exe" "%ROOT%\tools\dftfdcctl\main.c"
if errorlevel 1 exit /b 1

copy /Y "%DRV_OUT%\dftfdc.sys" "%DIST%\" >nul
if exist "%DRV_OUT%\dftfdc.pdb" copy /Y "%DRV_OUT%\dftfdc.pdb" "%DIST%\" >nul
copy /Y "%ROOT%\driver\dftfdc.inf" "%DIST%\" >nul
copy /Y "%TOOL_OUT%\dftfdcctl.exe" "%DIST%\" >nul
if exist "%TOOL_OUT%\dftfdcctl.pdb" copy /Y "%TOOL_OUT%\dftfdcctl.pdb" "%DIST%\" >nul
if exist "%DRV_OUT%\dftfdc.cat" copy /Y "%DRV_OUT%\dftfdc.cat" "%DIST%\" >nul

echo.
echo Build complete:
echo   %DIST%
echo.
echo Try after install:
echo   "%DIST%\dftfdcctl.exe" version
echo.

endlocal
