@echo off

set DOSBOX=C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe
set DOSROOT=C:\dosfiles

if not exist "%DOSBOX%" (
    echo ERROR: DOSBox not found:
    echo "%DOSBOX%"
    pause
    exit /b 1
)

if not exist "%DOSROOT%" (
    echo ERROR: DOS root not found:
    echo "%DOSROOT%"
    pause
    exit /b 1
)

start "" "%DOSBOX%" ^
  -c "mount c C:\dosfiles" ^
  -c "c:" ^
  -c "dir"