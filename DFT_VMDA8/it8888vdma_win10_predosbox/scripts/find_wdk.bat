@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Finds newest installed Windows 10/11 WDK kernel-mode and KMDF include/lib paths.
set "KITS=C:\Program Files (x86)\Windows Kits\10"
if not exist "%KITS%\Include" (
  echo ERROR: Windows Kits 10 Include directory not found: "%KITS%\Include"
  exit /b 1
)

set "BEST="
for /f "delims=" %%D in ('dir /b /ad "%KITS%\Include" 2^>nul ^| sort /r') do (
  if exist "%KITS%\Include\%%D\km\ntddk.h" (
    set "BEST=%%D"
    goto :found_wdk
  )
)

:found_wdk
if "%BEST%"=="" (
  echo ERROR: ntddk.h was not found under "%KITS%\Include\*\km".
  echo Install the Windows Driver Kit ^(WDK^) and the Visual Studio WDK integration.
  exit /b 1
)

rem KMDF headers are usually under Include\wdf\kmdf\<version>\wdf.h, not Include\<sdk>\km.
set "KMDF_INC="
set "KMDF_VER="
if exist "%KITS%\Include\wdf\kmdf" (
  for /f "delims=" %%K in ('dir /b /ad "%KITS%\Include\wdf\kmdf" 2^>nul ^| sort /r') do (
    if exist "%KITS%\Include\wdf\kmdf\%%K\wdf.h" (
      set "KMDF_VER=%%K"
      set "KMDF_MAJOR=!KMDF_VER:~0,1!"
      set "KMDF_MINOR=!KMDF_VER:1.=!"
      set "KMDF_INC=%KITS%\Include\wdf\kmdf\%%K"
      goto :found_kmdf_inc
    )
  )
)

:found_kmdf_inc
if "%KMDF_INC%"=="" (
  echo ERROR: wdf.h was not found under "%KITS%\Include\wdf\kmdf\*".
  echo Install the WDK KMDF headers / Visual Studio WDK integration.
  echo Quick check: dir "%KITS%\Include\wdf\kmdf\*\wdf.h"
  exit /b 1
)

rem KMDF WdfDriverEntry.lib is usually under Lib\wdf\kmdf\<arch>\<version>\WdfDriverEntry.lib.
set "KMDF_LIB="
if exist "%KITS%\Lib\wdf\kmdf\x64" (
  for /f "delims=" %%K in ('dir /b /ad "%KITS%\Lib\wdf\kmdf\x64" 2^>nul ^| sort /r') do (
    if exist "%KITS%\Lib\wdf\kmdf\x64\%%K\WdfDriverEntry.lib" (
      set "KMDF_LIB=%KITS%\Lib\wdf\kmdf\x64\%%K"
      goto :found_kmdf_lib
    )
  )
)

:found_kmdf_lib
if "%KMDF_LIB%"=="" (
  echo ERROR: WdfDriverEntry.lib was not found under "%KITS%\Lib\wdf\kmdf\x64\*".
  echo Install KMDF libraries from the WDK.
  echo Quick check: dir "%KITS%\Lib\wdf\kmdf\x64\*\WdfDriverEntry.lib"
  exit /b 1
)

endlocal & (
  set "IT8888_WDK_VERSION=%BEST%"
  set "IT8888_WDK_KM_INC=%KITS%\Include\%BEST%\km"
  set "IT8888_WDK_SHARED_INC=%KITS%\Include\%BEST%\shared"
  set "IT8888_WDK_UCRT_INC=%KITS%\Include\%BEST%\ucrt"
  set "IT8888_WDK_KM_LIB=%KITS%\Lib\%BEST%\km\x64"
  set "IT8888_WDK_WDF_INC=%KMDF_INC%"
  set "IT8888_WDK_WDF_LIB=%KMDF_LIB%"
  set "IT8888_WDK_KMDF_VERSION=%KMDF_VER%"
  set "IT8888_WDK_KMDF_MAJOR=%KMDF_MAJOR%"
  set "IT8888_WDK_KMDF_MINOR=%KMDF_MINOR%"
)
exit /b 0
