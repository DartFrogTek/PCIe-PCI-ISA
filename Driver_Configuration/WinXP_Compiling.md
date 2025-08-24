## **Windows XP Compilation** (Target: Windows XP)

### **Requirements:**
- Windows Server 2003 SP1 DDK (`WINDDK_3790.1830_usa_ddk.iso`)

### **Complete Steps:**

**1. Install DDK:**
```batch
# Mount DDK ISO
# Run setup.exe
# Install to: C:\WINDDK\3790 (default)
# Select: Full installation
```

**2. Prepare Source Files:**
```batch
# Create project directory
mkdir C:\ITE8888_Driver
cd C:\ITE8888_Driver

# Copy all source files:
# ite8888_driver.h
# ite8888_driver.c  
# ite8888_pnp.c
# ite8888_config.c
# ite8888_config.h (or ite8888_gus_config.h for GUS)
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

# Windows XP targeting
_NT_TARGET_VERSION=0x501
WIN32_WINNT_VERSION=0x501

INCLUDES=$(DDK_INC_PATH)
MSC_WARNING_LEVEL=/W3
```

**makefile**:
```makefile
#
# DO NOT EDIT - Standard DDK makefile
#
!INCLUDE $(NTMAKEENV)\makefile.def
```

**4. Build Driver:**
```batch
# Open DDK Build Environment:
# Start Menu -> Development Kits -> Windows DDK 
# -> Build Environments -> Windows XP -> x86 Checked Build Environment

# Navigate to project
cd C:\ITE8888_Driver

# Build driver
build -ceZ

# Check for errors
type build.log | find "error"
```

**5. Locate Output:**
```batch
# Driver file location:
dir objchk_wxp_x86\i386\ite8888.sys

# Copy for installation
copy objchk_wxp_x86\i386\ite8888.sys C:\Windows\System32\drivers\
```

**6. Create Installation Files:**

**ite8888_xp.inf**:
```ini
[Version]
Signature="$Windows NT$"
Class=System
ClassGuid={4D36E97D-E325-11CE-BFC1-08002BE10318}
Provider=%ManufacturerName%
DriverVer=01/01/2025,1.0.0.0
CatalogFile=ite8888.cat

[Manufacturer]
%ManufacturerName%=Standard

[Standard]
%ITE8888.DeviceDesc%=ITE8888_Device, PCI\VEN_1283&DEV_8888

[ITE8888_Device]
CopyFiles=Drivers_Dir

[ITE8888_Device.Services]
AddService=ite8888,%SPSVCINST_ASSOCSERVICE%,ITE8888_Service_Inst

[ITE8888_Service_Inst]
DisplayName=%ITE8888.SVCDESC%
ServiceType=1
StartType=3
ErrorControl=1
ServiceBinary=%12%\ite8888.sys

[Drivers_Dir]
ite8888.sys

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

**7. Install Driver:**
```batch
# Method 1: Device Manager
# Device Manager -> Unknown Device -> Update Driver -> Have Disk

# Method 2: Command line
rundll32 setupapi,InstallHinfSection DefaultInstall 132 ite8888_xp.inf
```
