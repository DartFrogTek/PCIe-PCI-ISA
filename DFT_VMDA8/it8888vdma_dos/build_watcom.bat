@echo off
rem Open Watcom 1.9/2.0 real-mode DOS build
rem Keeps object files, error files, and final EXE out of the source dir.

setlocal

set OBJDIR=build\obj
set ERRDIR=build\err
set DISTDIR=dist
set EXE=%DISTDIR%\it8dos.exe

if /I "%1"=="clean" goto clean
if /I "%1"=="rebuild" goto rebuild

goto build

:rebuild
call %0 clean
if errorlevel 1 goto fail

goto build

:build
if not exist build mkdir build
if not exist %OBJDIR% mkdir %OBJDIR%
if not exist %ERRDIR% mkdir %ERRDIR%
if not exist %DISTDIR% mkdir %DISTDIR%

echo Building objects...
echo CPU target: 386 real-mode DOS, using -3
wcl -q -c -ml -3 -bt=dos -fo=%OBJDIR%\dosmain.obj   -fr=%ERRDIR%\dosmain.err   dosmain.c
if errorlevel 1 goto fail
wcl -q -c -ml -3 -bt=dos -fo=%OBJDIR%\dos_pci.obj   -fr=%ERRDIR%\dos_pci.err   dos_pci.c
if errorlevel 1 goto fail
wcl -q -c -ml -3 -bt=dos -fo=%OBJDIR%\dos_io.obj    -fr=%ERRDIR%\dos_io.err    dos_io.c
if errorlevel 1 goto fail
wcl -q -c -ml -3 -bt=dos -fo=%OBJDIR%\dos_dma.obj   -fr=%ERRDIR%\dos_dma.err   dos_dma.c
if errorlevel 1 goto fail
wcl -q -c -ml -3 -bt=dos -fo=%OBJDIR%\dos_ddma.obj  -fr=%ERRDIR%\dos_ddma.err  dos_ddma.c
if errorlevel 1 goto fail
wcl -q -c -ml -3 -bt=dos -fo=%OBJDIR%\dos_vdma.obj  -fr=%ERRDIR%\dos_vdma.err  dos_vdma.c
if errorlevel 1 goto fail

echo Linking %EXE%...
wcl -q -ml -3 -bt=dos -lr -fe=%EXE% ^
  %OBJDIR%\dosmain.obj ^
  %OBJDIR%\dos_pci.obj ^
  %OBJDIR%\dos_io.obj ^
  %OBJDIR%\dos_dma.obj ^
  %OBJDIR%\dos_ddma.obj ^
  %OBJDIR%\dos_vdma.obj
if errorlevel 1 goto fail

echo.
echo Build OK: %EXE%
goto done

:clean
echo Cleaning build output...
if exist %OBJDIR%\*.obj del /q %OBJDIR%\*.obj
if exist %ERRDIR%\*.err del /q %ERRDIR%\*.err
if exist %DISTDIR%\it8dos.exe del /q %DISTDIR%\it8dos.exe
rem Remove common accidental source-dir outputs from the old one-line build.
if exist *.obj del /q *.obj
if exist *.err del /q *.err
if exist it8dos.exe del /q it8dos.exe
if exist it8dos.map del /q it8dos.map
echo Clean OK.
goto done

:fail
echo.
echo Build FAILED. Check %ERRDIR% for .err files.
exit /b 1

:done
endlocal
