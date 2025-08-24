/*
 * ite8888_driver.c
 *
 * ITE8888 PCI-to-ISA Bridge Configuration Driver - Main Implementation
 */

#include "ite8888_driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#pragma alloc_text(PAGE, AddDevice)
#pragma alloc_text(PAGE, HandleStartDevice)
#pragma alloc_text(PAGE, HandleRemoveDevice)
#pragma alloc_text(PAGE, HandleStopDevice)
#pragma alloc_text(PAGE, ConfigureBridge)
#endif

// Static configuration tables built from defines
static const IO_SPACE_CONFIG IoSpaceConfig[6] = {
    { IO_SPACE_0_ENABLE, IO_SPACE_0_BASE, IO_SPACE_0_SIZE, IO_SPACE_0_SPEED, IO_SPACE_0_ALIAS },
    { IO_SPACE_1_ENABLE, IO_SPACE_1_BASE, IO_SPACE_1_SIZE, IO_SPACE_1_SPEED, IO_SPACE_1_ALIAS },
    { IO_SPACE_2_ENABLE, IO_SPACE_2_BASE, IO_SPACE_2_SIZE, IO_SPACE_2_SPEED, IO_SPACE_2_ALIAS },
    { IO_SPACE_3_ENABLE, IO_SPACE_3_BASE, IO_SPACE_3_SIZE, IO_SPACE_3_SPEED, IO_SPACE_3_ALIAS },
    { IO_SPACE_4_ENABLE, IO_SPACE_4_BASE, IO_SPACE_4_SIZE, IO_SPACE_4_SPEED, IO_SPACE_4_ALIAS },
    { IO_SPACE_5_ENABLE, IO_SPACE_5_BASE, IO_SPACE_5_SIZE, IO_SPACE_5_SPEED, IO_SPACE_5_ALIAS }
};

static const MEM_SPACE_CONFIG MemSpaceConfig[4] = {
    { MEM_SPACE_0_ENABLE, MEM_SPACE_0_BASE, MEM_SPACE_0_SIZE, MEM_SPACE_0_SPEED },
    { MEM_SPACE_1_ENABLE, MEM_SPACE_1_BASE, MEM_SPACE_1_SIZE, MEM_SPACE_1_SPEED },
    { MEM_SPACE_2_ENABLE, MEM_SPACE_2_BASE, MEM_SPACE_2_SIZE, MEM_SPACE_2_SPEED },
    { MEM_SPACE_3_ENABLE, MEM_SPACE_3_BASE, MEM_SPACE_3_SIZE, MEM_SPACE_3_SPEED }
};

static const DMA_CONFIG DmaConfig[2] = {
    { DMA_CHAN_1_ENABLE, DMA_CHAN_1_CHANNEL, DMA_CHAN_1_TYPE_F },
    { DMA_CHAN_5_ENABLE, DMA_CHAN_5_CHANNEL, DMA_CHAN_5_TYPE_F }
};

/*
 * Driver Entry Point
 */
NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG i;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint(DBG_INFO, "ITE8888 Bridge Driver Loading - Version %d.%d for %ws", 
             DRIVER_VERSION_MAJOR, DRIVER_VERSION_MINOR, CARD_NAME);

    // Set up dispatch routines
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = DispatchDefault;
    }

    // Override specific dispatch routines
    DriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;

    // Set AddDevice and Unload routines
    DriverObject->DriverExtension->AddDevice = AddDevice;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint(DBG_INFO, "ITE8888 Bridge Driver Loaded Successfully");

    return status;
}

/*
 * Driver Unload
 */
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(DriverObject);
    
    DbgPrint(DBG_INFO, "ITE8888 Bridge Driver Unloading");
}

/*
 * Add Device - Called when ITE8888 device is detected
 */
NTSTATUS AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    PDEVICE_EXTENSION deviceExtension;
    PDEVICE_OBJECT lowerDeviceObject;

    PAGED_CODE();

    DbgPrint(DBG_INFO, "AddDevice called for ITE8888 device %p", PhysicalDeviceObject);

    // Create our device object
    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        NULL,  // No name - we're a PnP driver
        FILE_DEVICE_BUS_EXTENDER,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint(DBG_ERROR, "Failed to create device object: 0x%08X", status);
        return status;
    }

    // Initialize device extension
    deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;
    RtlZeroMemory(deviceExtension, sizeof(DEVICE_EXTENSION));

    deviceExtension->Signature = DEVICE_SIGNATURE;
    deviceExtension->DeviceObject = deviceObject;
    deviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
    
    // Initialize remove lock
    IoInitializeRemoveLock(&deviceExtension->RemoveLock, 'F888', 0, 0);

    // Attach to the device stack
    lowerDeviceObject = IoAttachDeviceToDeviceStack(deviceObject, PhysicalDeviceObject);
    if (lowerDeviceObject == NULL) {
        DbgPrint(DBG_ERROR, "Failed to attach to device stack");
        IoDeleteDevice(deviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }

    deviceExtension->LowerDeviceObject = lowerDeviceObject;

    // Set device object flags
    deviceObject->Flags |= lowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
    deviceObject->DeviceType = lowerDeviceObject->DeviceType;
    deviceObject->Characteristics = lowerDeviceObject->Characteristics;

    // Clear the initializing flag
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint(DBG_INFO, "ITE8888 device %p successfully attached to stack", deviceObject);

    return STATUS_SUCCESS;
}

/*
 * Default IRP Dispatch - Pass through for most IRPs
 */
NTSTATUS DispatchDefault(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status;

    // Acquire remove lock
    status = IoAcquireRemoveLock(&deviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        return CompleteRequest(Irp, status, 0);
    }

    // Forward to lower driver
    status = ForwardRequest(deviceExtension, Irp);
    
    IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp);
    return status;
}

/*
 * PnP IRP Dispatch
 */
NTSTATUS DispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;

    status = IoAcquireRemoveLock(&deviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        return CompleteRequest(Irp, status, 0);
    }

    DbgPrint(DBG_VERBOSE, "PnP IRP: Minor Function 0x%02X", irpStack->MinorFunction);

    switch (irpStack->MinorFunction) {
        case IRP_MN_START_DEVICE:
            status = HandleStartDevice(deviceExtension, Irp);
            break;

        case IRP_MN_REMOVE_DEVICE:
            status = HandleRemoveDevice(deviceExtension, Irp);
            break;

        case IRP_MN_STOP_DEVICE:
            status = HandleStopDevice(deviceExtension, Irp);
            break;

        default:
            // Pass through to lower driver
            status = ForwardRequest(deviceExtension, Irp);
            break;
    }

    IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp);
    return status;
}

/*
 * Power IRP Dispatch
 */
NTSTATUS DispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status;

    status = IoAcquireRemoveLock(&deviceExtension->RemoveLock, Irp);
    if (!NT_SUCCESS(status)) {
        PoStartNextPowerIrp(Irp);
        return CompleteRequest(Irp, status, 0);
    }

    // For now, just pass power IRPs through
    PoStartNextPowerIrp(Irp);
    status = ForwardRequest(deviceExtension, Irp);
    
    IoReleaseRemoveLock(&deviceExtension->RemoveLock, Irp);
    return status;
}

/*
 * Utility function to complete IRP
 */
NTSTATUS CompleteRequest(
    IN PIRP Irp,
    IN NTSTATUS Status,
    IN ULONG_PTR Information
)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

/*
 * Utility function to forward IRP
 */
NTSTATUS ForwardRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
)
{
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
}

/*
 * Debug print function
 */
VOID DebugPrint(
    IN ULONG Level,
    IN PCCHAR Format,
    ...
)
{
#if ENABLE_DEBUG_PRINTS
    va_list args;
    char buffer[512];
    
    if (Level <= DEBUG_LEVEL_DEFAULT) {
        va_start(args, Format);
        RtlStringCbVPrintfA(buffer, sizeof(buffer), Format, args);
        va_end(args);
        
        DbgPrint(buffer);
    }
#endif
    
    UNREFERENCED_PARAMETER(Level);
    UNREFERENCED_PARAMETER(Format);
}