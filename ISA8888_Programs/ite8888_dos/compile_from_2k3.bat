@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

REM Clear environment
SET PATH=C:\WINDOWS\SYSTEM32;C:\WINDOWS;C:\WINDOWS\COMMAND
SET INCLUDE=
SET LIB=

REM Set OpenWatcom paths
SET WATCOM=C:\WATCOM
SET PATH=%WATCOM%\BINW;%PATH%
SET INCLUDE=%WATCOM%\H
SET LIB=%WATCOM%\LIB286;%WATCOM%\LIB286\DOS
SET EDPATH=%WATCOM%\EDDAT
SET WIPFC=%WATCOM%\WIPFC

ECHO OpenWatcom Environment:
ECHO WATCOM=%WATCOM%
ECHO LIB=%LIB%
ECHO.

REM Change to the directory containing the source
CD /D "%~dp0"

ECHO Compiling ITE8888.C...
WCC -bt=dos -ml -ox -we -d2 ITE8888.C
IF NOT EXIST ITE8888.OBJ (
    ECHO Compilation failed - no object file created
    PAUSE
    GOTO :EOF
)
ECHO Compilation successful

ECHO.
ECHO Linking...
WLINK system dos ^
      libpath %WATCOM%\LIB286 ^
      libpath %WATCOM%\LIB286\DOS ^
      library clibl ^
      file ITE8888.OBJ ^
      name ITE8888CFG.EXE

IF NOT EXIST ITE8888CFG.EXE (
    ECHO Linking failed - no executable created
    ECHO.
    ECHO Try running with verbose output:
    ECHO WLINK system dos op verbose file ITE8888.OBJ name ITE8888CFG.EXE
    PAUSE
    GOTO :EOF
)

ECHO.
ECHO Build successful!
DIR ITE8888CFG.EXE

REM Cleanup
IF EXIST ITE8888.OBJ DEL ITE8888.OBJ

ENDLOCAL
PAUSE
