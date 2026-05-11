# IT8888VDMA Windows 10 build scripts

These scripts assume a Windows 10 x64 development machine with:

- Visual Studio 2022 with Desktop C++ workload
- Windows 10 SDK
- Windows Driver Kit / WDK integrated into Visual Studio
- Administrator shell for signing/install scripts

From a normal `cmd.exe` or PowerShell in the project root:

### Debug

```bat
clean.bat
build_all_direct.bat Debug x64
```

or:

```bat
clean.bat
build_driver_direct.bat Debug x64
```

### Release

```bat
clean.bat
build_all_direct.bat Release x64
```

or:

```bat
clean.bat
build_driver_direct.bat Release x64
```
