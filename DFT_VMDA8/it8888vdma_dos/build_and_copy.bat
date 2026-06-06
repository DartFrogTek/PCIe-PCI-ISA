@echo off
call build_watcom.bat
if errorlevel 1 exit /b 1

call copy_to_dosfiles.bat
if errorlevel 1 exit /b 1