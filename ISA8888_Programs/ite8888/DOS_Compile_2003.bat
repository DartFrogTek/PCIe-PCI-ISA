@echo off
echo Building ITE8888 DOS Configuration Utility
echo ========================================

REM Debug current directory and files
echo Current directory: %CD%
echo Files in directory:
dir *.c
dir *.h
echo.

REM Check if specific files exist
if exist ite8888_core.c (
    echo Found: ite8888_core.c
) else (
    echo Missing: ite8888_core.c
)

if exist ite8888_dos.c (
    echo Found: ite8888_dos.c  
) else (
    echo Missing: ite8888_dos.c
)

if exist ite8888_core.h (
    echo Found: ite8888_core.h
) else (
    echo Missing: ite8888_core.h
)

echo.
pause

REM Set OpenWatcom environment (adjust path as needed)
if not defined WATCOM (
    echo ERROR: WATCOM environment variable not set
    echo Please run OWSETENV.BAT from your OpenWatcom installation
    pause
    exit /b 1
)

REM Clean previous build
if exist ITE8888.exe del ITE8888.exe
if exist *.obj del *.obj
if exist *.err del *.err

echo.
echo Compiling core module...
wcl -c -bt=dos -3r -ox -w4 -we -zq -v "C:\\ISA\\ite8888\\ITE8888_CORE.C"
if errorlevel 1 goto error

echo Compiling DOS shell...
wcl -c -bt=dos -3r -ox -w4 -we -zq -v "C:\\ISA\\ite8888\\ITE8888_DOS.C"
if errorlevel 1 goto error

echo Linking executable...
wcl -bt=dos -3r -ox -fe="C:\\ISA\\ite8888\\ITE8888.exe" "C:\\ISA\\ite8888\\ITE8888_CORE.obj" "C:\\ISA\\ite8888\\ITE8888_DOS.obj"
if errorlevel 1 goto error

echo.
echo Build successful! Created ITE8888.exe
echo File size:
dir ITE8888.exe | find ".exe"

echo.
echo Testing executable...
ITE8888.exe -help
if errorlevel 1 (
    echo Warning: Executable test failed
) else (
    echo Executable test passed
)

goto end

:error
echo.
echo ERROR: Build failed!
if exist *.err type *.err
pause
exit /b 1

:end
echo.
echo Build complete. You can now run ITE8888.exe in DOS or DOSBox.
pause
