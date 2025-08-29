# Makefile for ITE8888CFG

!ifdef WATCOM
CC = wcl386
CFLAGS = -bt=dos -l=dos4g -w4 -ox
TARGET = ITE8888CFG.EXE
!endif

$(TARGET): ITE8888CFG.C
    $(CC) $(CFLAGS) ITE8888CFG.C -o $(TARGET)

clean:
    del *.obj
    del *.exe

# Usage:
# wmake WATCOM=1    (for OpenWatcom)