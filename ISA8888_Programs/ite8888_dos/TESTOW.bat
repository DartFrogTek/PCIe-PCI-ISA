@ECHO OFF
SETLOCAL

REM Clear environment completely
SET PATH=C:\WINDOWS\SYSTEM32;C:\WINDOWS;C:\WINDOWS\COMMAND
SET INCLUDE=
SET LIB=

REM Set only OpenWatcom
SET WATCOM=C:\WATCOM
SET PATH=C:\WATCOM\BINW;%PATH%
SET INCLUDE=C:\WATCOM\H

REM Create simple test
ECHO #include ^<stdio.h^> > owtest.c
ECHO int main(){printf("OpenWatcom works!\n");return 0;} >> owtest.c

REM Compile test
C:\WATCOM\BINW\WCC.EXE -bt=dos owtest.c
IF EXIST owtest.obj (
    C:\WATCOM\BINW\WLINK.EXE system dos file owtest.obj name owtest.exe
    IF EXIST owtest.exe (
        ECHO OpenWatcom is working correctly!
        owtest.exe
    )
    DEL owtest.obj owtest.c owtest.exe 2>nul
) ELSE (
    ECHO OpenWatcom environment problem
)

ENDLOCAL
PAUSE