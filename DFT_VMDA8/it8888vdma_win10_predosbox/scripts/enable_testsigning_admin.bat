@echo off
net session >nul 2>&1
if errorlevel 1 (
  echo ERROR: Run this as Administrator.
  exit /b 1
)
echo Enabling Windows test-signing mode. Reboot required.
bcdedit /set testsigning on
if errorlevel 1 exit /b 1
echo OK. Reboot Windows before installing the test-signed driver.
