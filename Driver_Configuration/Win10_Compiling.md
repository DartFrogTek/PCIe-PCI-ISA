## **Windows 10 Compilation** (Target: Windows 10)

### **Requirements:**
- Visual Studio 2019 or 2022 Community
- Windows 10 WDK (version 10.0.19041.0 or later)
- Windows 10 SDK

### **Complete Steps:**

**1. Install Development Tools:**
```batch
# Download and install Visual Studio 2019/2022 Community
# Workloads to select:
# - Desktop development with C++
# - Game development with C++ (for DirectX SDK)

# Download and install Windows 10 WDK
# Must match Visual Studio version
# Install to default location

# Download and install Windows 10 SDK
# Usually installed automatically with WDK
```

**2. Create Visual Studio Project:**
```batch
# Open Visual Studio
# File -> New -> Project
# Visual C++ -> Windows Driver -> WDM -> Empty WDM Driver Project
# Name: ITE8888_Driver_Win10
# Location: C:\Projects\
```

**3. Configure Project:**
```cpp
// Right-click project -> Properties
// Configuration: All Configurations
// Platform: All Platforms

// Driver Settings -> General:
// Target OS Version: Windows 10
// Target Platform: Desktop
// Driver Type: WDM

// C/C++ -> General:
// Warning Level: Level3 (/W3)
// Treat Warnings As Errors: No

// C/C++ -> Preprocessor -> Preprocessor Definitions:
WIN32_LEAN_AND_MEAN
_WIN32_WINNT=0x0A00
NTDDI_VERSION=0x0A000000
WIN10_COMPATIBLE=1
```

**4. Add Source Files:**
```cpp
// Add all source files to project:
// Solution Explorer -> Source Files -> Add -> Existing Item
// Select all .c files

// Add all header files:
// Solution Explorer -> Header Files -> Add -> Existing Item  
// Select all .h files
```

**5. Update Source for Windows 10:**

**ite8888_driver.h** additions:
```cpp
// Add Windows 10 specific includes
#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

// Windows 10 compatibility
#if (NTDDI_VERSION >= NTDDI_WIN10)
#define WIN10_FEATURES_ENABLED 1
#include <ntintsafe.h>
#endif
```

**6. Build Driver:**
```batch
# Method 1: Visual Studio
# Build -> Build Solution (Ctrl+Shift+B)

# Method 2: MSBuild command line
# Open Developer Command Prompt for VS 2019/2022
cd C:\Projects\ITE8888_Driver_Win10
msbuild ITE8888_Driver_Win10.vcxproj /p:Configuration=Debug /p:Platform=x64
```

**7. Locate Output:**
```batch
# Driver files location:
# x64\Debug\ite8888.sys (64-bit debug)
# x64\Release\ite8888.sys (64-bit release)
# Win32\Debug\ite8888.sys (32-bit debug)
# Win32\Release\ite8888.sys (32-bit release)
```

**8. Create Windows 10 Installation Files:**

**ite8888_win10.inf**:
```ini
[Version]
Signature="$Windows NT$"
Class=System
ClassGuid={4D36E97D-E325-11CE-BFC1-08002BE10318}
Provider=%ManufacturerName%
DriverVer=01/01/2025,1.0.0.0
CatalogFile=ite8888.cat
PnpLockdown=1

[Manufacturer]
%ManufacturerName%=Standard,NTamd64,NTx86

[Standard.NTamd64]
%ITE8888.DeviceDesc%=ITE8888_Device, PCI\VEN_1283&DEV_8888

[Standard.NTx86] 
%ITE8888.DeviceDesc%=ITE8888_Device, PCI\VEN_1283&DEV_8888

[ITE8888_Device.NTamd64]
CopyFiles=Drivers_Dir
FeatureScore=80

[ITE8888_Device.NTx86]
CopyFiles=Drivers_Dir
FeatureScore=80

[ITE8888_Device.NTamd64.Services]
AddService=ite8888,%SPSVCINST_ASSOCSERVICE%,ITE8888_Service_Inst

[ITE8888_Device.NTx86.Services]
AddService=ite8888,%SPSVCINST_ASSOCSERVICE%,ITE8888_Service_Inst

[ITE8888_Service_Inst]
DisplayName=%ITE8888.SVCDESC%
ServiceType=1
StartType=3
ErrorControl=1
ServiceBinary=%13%\ite8888.sys
LoadOrderGroup=Extended Base

[Drivers_Dir]
ite8888.sys

[DestinationDirs]
Drivers_Dir = 13

[SourceDisksNames]
1 = %DiskName%,,,""

[SourceDisksFiles.NTamd64]
ite8888.sys = 1,,

[SourceDisksFiles.NTx86]
ite8888.sys = 1,,

[Strings]
SPSVCINST_ASSOCSERVICE=0x00000002
ManufacturerName="ITE8888 Driver"
DiskName="ITE8888 Installation"
ITE8888.DeviceDesc="ITE ITE8888 PCI-to-ISA Bridge"
ITE8888.SVCDESC="ITE8888 Configuration Service"
```

**9. Sign Driver (Required for Windows 10):**
```batch
# Create test certificate
makecert -r -pe -ss PrivateCertStore -n "CN=Test Driver Certificate" TestCert.cer

# Sign the driver
signtool sign /v /s PrivateCertStore /n "Test Driver Certificate" /t http://timestamp.verisign.com/scripts/timestamp.dll x64\Release\ite8888.sys

# Create catalog file
inf2cat /driver:. /os:10_X64

# Sign catalog
signtool sign /v /s PrivateCertStore /n "Test Driver Certificate" /t http://timestamp.verisign.com/scripts/timestamp.dll ite8888.cat
```

**10. Install Driver:**
```batch
# Enable test signing mode
bcdedit /set testsigning on
shutdown /r /t 0

# Install driver after reboot
pnputil /add-driver ite8888_win10.inf /install

# Verify installation
pnputil /enum-drivers | findstr ite8888
```

**11. Debug Driver (if needed):**
```batch
# Enable driver verifier
verifier /standard /driver ite8888.sys

# View debug output
# Download DebugView from Microsoft Sysinternals
# Run as Administrator to see kernel debug output
```
