@echo off
setlocal

set SRC=dist\it8dos.exe
set DSTDIR=C:\dosfiles
set DST=%DSTDIR%\it8dos.exe

if not exist "%SRC%" (
    echo ERROR: "%SRC%" not found.
    echo Run build_watcom.bat first.
    exit /b 1
)

if not exist "%DSTDIR%" (
    echo Creating "%DSTDIR%"...
    mkdir "%DSTDIR%"
    if errorlevel 1 exit /b 1
)

echo Copying "%SRC%" to "%DST%"...
copy /Y "%SRC%" "%DST%"
if errorlevel 1 exit /b 1

echo OK: copied to "%DST%"
endlocal