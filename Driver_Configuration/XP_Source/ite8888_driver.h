/*
 * ite8888_driver.h
 *
 * ITE8888 PCI-to-ISA Bridge Configuration Driver
 */

#ifndef _ITE8888_DRIVER_H_
#define _ITE8888_DRIVER_H_

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

// GUS OR USER DEFINED
#include "ite8888_config.h"
#include "ite8888_gus_config.h"

// Driver version
#define DRIVER_VERSION_MAJOR    1
#define DRIVER_VERSION_MINOR    0

// Device extension signature  
#define DEVICE_SIGNATURE        'F888'  // '888F' backwards

// ITE8888 PCI IDs
#define ITE8888_VENDOR_ID       0x1283
#define ITE8888_DEVICE_ID       0x8888

// PCI Configuration Register offsets
#define PCI_COMMAND_REGISTER    0x04
#define ITE8888_MISC_CONTROL    0x50
#define ITE8888_RETRY_DISCARD   0x54

// I/O Space registers (Cfg_58h - Cfg_6Fh)
#define ITE8888_IO_SPACE_0      0x58
#define ITE8888_IO_SPACE_1      0x5C
#define ITE8888_IO_SPACE_2      0x60
#define ITE8888_IO_SPACE_3      0x64
#define ITE8888_IO_SPACE_4      0x68
#define ITE8888_IO_SPACE_5      0x6C

// Memory Space registers (Cfg_70h - Cfg_7Fh)
#define ITE8888_MEM_SPACE_0     0x70
#define ITE8888_MEM_SPACE_1     0x74
#define ITE8888_MEM_SPACE_2     0x78
#define ITE8888_MEM_SPACE_3     0x7C

// DMA Channel registers
#define ITE8888_DMA_CHANNEL_01  0x40
#define ITE8888_DMA_CHANNEL_23  0x44
#define ITE8888_DMA_CHANNEL_5   0x48
#define ITE8888_DMA_CHANNEL_67  0x4C

// PCI Command register bits
#define PCI_ENABLE_IO_SPACE     0x0001
#define PCI_ENABLE_MEMORY_SPACE 0x0002
#define PCI_ENABLE_BUS_MASTER   0x0004

// I/O Space register bits
#define IO_SPACE_ENABLE         0x80000000
#define IO_SPACE_ALIAS          0x10000000
#define IO_SPACE_SIZE_SHIFT     24
#define IO_SPACE_SPEED_SHIFT    29

// Memory Space register bits  
#define MEM_SPACE_ENABLE        0x80000000
#define MEM_SPACE_SIZE_SHIFT    24
#define MEM_SPACE_SPEED_SHIFT   29

// DMA register bits
#define DMA_ENABLE              0x80000000
#define DMA_TYPE_F              0x40000000

// Configuration structure for each I/O space
typedef struct _IO_SPACE_CONFIG {
    BOOLEAN Enable;
    ULONG   BaseAddress;
    ULONG   Size;
    ULONG   Speed;
    BOOLEAN Alias;
} IO_SPACE_CONFIG, *PIO_SPACE_CONFIG;

// Configuration structure for each memory space
typedef struct _MEM_SPACE_CONFIG {
    BOOLEAN Enable;
    ULONG   BaseAddress;
    ULONG   Size;
    ULONG   Speed;
} MEM_SPACE_CONFIG, *PMEM_SPACE_CONFIG;

// Configuration structure for DMA channels
typedef struct _DMA_CONFIG {
    BOOLEAN Enable;
    ULONG   Channel;
    BOOLEAN TypeF;
} DMA_CONFIG, *PDMA_CONFIG;

// Device extension
typedef struct _DEVICE_EXTENSION {
    ULONG           Signature;
    PDEVICE_OBJECT  DeviceObject;
    PDEVICE_OBJECT  LowerDeviceObject;
    PDEVICE_OBJECT  PhysicalDeviceObject;
    
    // PCI location
    ULONG           BusNumber;
    ULONG           DeviceNumber;
    ULONG           FunctionNumber;
    
    // Configuration status
    BOOLEAN         BridgeConfigured;
    LARGE_INTEGER   ConfigurationTime;
    
    // Remove lock for PnP
    IO_REMOVE_LOCK  RemoveLock;
    
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// Function prototypes
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS AddDevice(IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT PhysicalDeviceObject);

// IRP dispatch routines
NTSTATUS DispatchDefault(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DispatchPnp(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DispatchPower(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

// PnP handlers
NTSTATUS HandleStartDevice(IN PDEVICE_EXTENSION DeviceExtension, IN PIRP Irp);
NTSTATUS HandleRemoveDevice(IN PDEVICE_EXTENSION DeviceExtension, IN PIRP Irp);
NTSTATUS HandleStopDevice(IN PDEVICE_EXTENSION DeviceExtension, IN PIRP Irp);
// Additional PnP functions
NTSTATUS StartDeviceCompletion(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context);
NTSTATUS GetPciLocation(IN PDEVICE_EXTENSION DeviceExtension);

// Bridge configuration
NTSTATUS ConfigureBridge(IN PDEVICE_EXTENSION DeviceExtension);
NTSTATUS ReadPciConfig(IN PDEVICE_EXTENSION DeviceExtension, IN ULONG Offset, OUT PULONG Value);
NTSTATUS WritePciConfig(IN PDEVICE_EXTENSION DeviceExtension, IN ULONG Offset, IN ULONG Value);
// Additional bridge configuration functions  
NTSTATUS ResetBridge(IN PDEVICE_EXTENSION DeviceExtension);
NTSTATUS VerifyConfiguration(IN PDEVICE_EXTENSION DeviceExtension);

// Utility functions
NTSTATUS CompleteRequest(IN PIRP Irp, IN NTSTATUS Status, IN ULONG_PTR Information);
NTSTATUS ForwardRequest(IN PDEVICE_EXTENSION DeviceExtension, IN PIRP Irp);
VOID DebugPrint(IN ULONG Level, IN PCCHAR Format, ...);

// Debug levels
#define DBG_ERROR   0
#define DBG_WARN    1  
#define DBG_INFO    2
#define DBG_VERBOSE 3

#if ENABLE_DEBUG_PRINTS
#define DbgPrint(level, format, ...) \
    DebugPrint(level, "[ITE8888] " format "\n", __VA_ARGS__)
#else
#define DbgPrint(level, format, ...)
#endif

#endif // _ITE8888_DRIVER_H_