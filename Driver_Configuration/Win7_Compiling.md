## **Windows 7 Compilation** (Target: Windows 7)

### **Requirements:**
- Windows 7 WDK 7.1.0 (`GRMWDK_EN_7600_1.ISO`)
- Visual Studio 2008/2010 (optional, for IDE support)

### **Complete Steps:**

**1. Install WDK:**
```batch
# Mount WDK ISO  
# Run KitSetup.exe
# Install to: C:\WinDDK\7600.16385.1 (default)
# Select: Full Installation
# Install Visual Studio Integration if VS installed
```

**2. Prepare Source Files:**
```batch
# Create project directory
mkdir C:\ITE8888_Driver_Win7
cd C:\ITE8888_Driver_Win7

# Copy all source files (same as above)
```

**3. Create Build Files:**

**sources** file:
```makefile
TARGETNAME=ite8888
TARGETTYPE=DRIVER
TARGETPATH=obj

SOURCES=\
    ite8888_driver.c \
    ite8888_pnp.c \
    ite8888_config.c

# Windows 7 targeting
_NT_TARGET_VERSION=0x601
WIN32_WINNT_VERSION=0x601
NTDDI_VERSION=0x06010000

INCLUDES=$(DDK_INC_PATH)
MSC_WARNING_LEVEL=/W3

# Enable Windows 7 features
C_DEFINES=$(C_DEFINES) -DWIN7_COMPATIBLE=1
```

**makefile**:
```makefile
#
# Standard WDK makefile
#
!INCLUDE $(NTMAKEENV)\makefile.def
```

**4. Build Driver:**
```batch
# Open WDK Build Environment:
# Start Menu -> Windows Driver Kits -> WDK 7600.16385.1
# -> Build Environments -> Windows 7 -> x86 Checked Build Environment

# Navigate to project
cd C:\ITE8888_Driver_Win7

# Clean previous builds
build -c

# Build driver
build -ceZ

# Verify success
echo Build Status: %ERRORLEVEL%
```

**5. Locate Output:**
```batch
# Driver file location:
dir objchk_win7_x86\i386\ite8888.sys

# Check driver properties
dumpbin /headers objchk_win7_x86\i386\ite8888.sys
```

**6. Create Windows 7 Installation Files:**

**ite8888_win7.inf**:
```ini
[Version]
Signature="$Windows NT$"
Class=System
ClassGuid={4D36E97D-E325-11CE-BFC1-08002BE10318}
Provider=%ManufacturerName%
DriverVer=01/01/2025,1.0.0.0
CatalogFile=ite8888.cat

[Manufacturer]
%ManufacturerName%=Standard,NTx86

[Standard.NTx86]
%ITE8888.DeviceDesc%=ITE8888_Device, PCI\VEN_1283&DEV_8888

[ITE8888_Device.NTx86]
CopyFiles=Drivers_Dir
FeatureScore=80

[ITE8888_Device.NTx86.Services]
AddService=ite8888,%SPSVCINST_ASSOCSERVICE%,ITE8888_Service_Inst

[ITE8888_Service_Inst]
DisplayName=%ITE8888.SVCDESC%
ServiceType=1
StartType=3
ErrorControl=1
ServiceBinary=%12%\ite8888.sys
LoadOrderGroup=Extended Base

[Drivers_Dir]
ite8888.sys

[DestinationDirs]
Drivers_Dir = 12

[SourceDisksNames]
1 = %DiskName%,,,""

[SourceDisksFiles]
ite8888.sys = 1,,

[Strings]
SPSVCINST_ASSOCSERVICE=0x00000002
ManufacturerName="ITE8888 Driver"
DiskName="ITE8888 Installation"
ITE8888.DeviceDesc="ITE ITE8888 PCI-to-ISA Bridge"
ITE8888.SVCDESC="ITE8888 Configuration Service"
```

**7. Sign Driver (Required for Windows 7):**
```batch
# Create test certificate (for testing only)
makecert -r -pe -ss PrivateCertStore -n CN=TestDrivers TestCert.cer

# Sign the driver
signtool sign /v /s PrivateCertStore /n TestDrivers objchk_win7_x86\i386\ite8888.sys
```

**8. Install Driver:**
```batch
# Enable test signing mode
bcdedit /set testsigning on
shutdown /r /t 0

# Install driver after reboot
pnputil -i -a ite8888_win7.inf
```
