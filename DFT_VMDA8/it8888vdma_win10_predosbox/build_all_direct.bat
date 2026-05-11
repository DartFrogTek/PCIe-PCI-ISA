@echo off
setlocal EnableExtensions
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Debug
set PLAT=%~2
if "%PLAT%"=="" set PLAT=x64

REM First use your existing MSBuild path for the user-mode tool.
call "%~dp0build_all.bat" %CONFIG% %PLAT%
REM Ignore missing .sys from vcxproj path; now force direct .sys link.
call "%~dp0build_driver_direct.bat" %CONFIG% %PLAT%
exit /b %ERRORLEVEL%
