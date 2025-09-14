@echo off
echo Building ITE8888 DOS Configuration Utility
echo ========================================

REM Set OpenWatcom environment (adjust path as needed)
if not defined WATCOM (
    echo ERROR: WATCOM environment variable not set
    echo Please run OWSETENV.BAT from your OpenWatcom installation
    pause
    exit /b 1
)

REM Clean previous build
if exist ite8888.exe del ite8888.exe
if exist *.obj del *.obj
if exist *.err del *.err

echo.
echo Compiling core module...
wcl -c -bt=dos -3r -ox -w4 -we -zq ite8888_core.c
if errorlevel 1 goto error

echo Compiling DOS shell...
wcl -c -bt=dos -3r -ox -w4 -we -zq ite8888_dos.c
if errorlevel 1 goto error

echo Linking executable...
wcl -bt=dos -3r -ox -fe=ite8888.exe ite8888_core.obj ite8888_dos.obj
if errorlevel 1 goto error

echo.
echo Build successful! Created ite8888.exe
echo File size:
dir ite8888.exe | find ".exe"

echo.
echo Testing executable...
ite8888.exe -help
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
echo Build complete. You can now run ite8888.exe in DOS or DOSBox.
pause