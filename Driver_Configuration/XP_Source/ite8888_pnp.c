/*
 * ITE8888_pnp.c
 *
 * PnP Event Handlers for ITE8888 Bridge Configuration Driver
 */

#include "ite8888_driver.h"

/*
 * Handle IRP_MN_START_DEVICE
 */
NTSTATUS HandleStartDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
)
{
    NTSTATUS status;
    KEVENT event;
    
    PAGED_CODE();

    DbgPrint(DBG_INFO, "Starting ITE8888 bridge device");

    // Initialize completion event
    KeInitializeEvent(&event, NotificationEvent, FALSE);

    // Copy current stack location to next
    IoCopyCurrentIrpStackLocationToNext(Irp);

    // Set completion routine
    IoSetCompletionRoutine(Irp, StartDeviceCompletion, &event, TRUE, TRUE, TRUE);

    // Pass IRP down first
    status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
    
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = Irp->IoStatus.Status;
    }

    if (NT_SUCCESS(status)) {
        // Lower driver succeeded - now configure our bridge
        status = GetPciLocation(DeviceExtension);
        
        if (NT_SUCCESS(status)) {
            DbgPrint(DBG_INFO, "Found ITE8888 at Bus=%d, Device=%d, Function=%d",
                     DeviceExtension->BusNumber,
                     DeviceExtension->DeviceNumber, 
                     DeviceExtension->FunctionNumber);

            // Configure the bridge
            status = ConfigureBridge(DeviceExtension);
            if (NT_SUCCESS(status)) {
                DeviceExtension->BridgeConfigured = TRUE;
                KeQuerySystemTime(&DeviceExtension->ConfigurationTime);
                DbgPrint(DBG_INFO, "ITE8888 bridge configured successfully for %ws", CARD_NAME);
            } else {
                DbgPrint(DBG_ERROR, "Failed to configure ITE8888 bridge: 0x%08X", status);
            }
        } else {
            DbgPrint(DBG_ERROR, "Failed to get PCI location: 0x%08X", status);
        }
    }

    // Complete the IRP
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}

/*
 * Handle IRP_MN_REMOVE_DEVICE
 */
NTSTATUS HandleRemoveDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
)
{
    NTSTATUS status;
    
    PAGED_CODE();

    DbgPrint(DBG_INFO, "Removing ITE8888 bridge device");

    // Reset bridge configuration if we configured it
    if (DeviceExtension->BridgeConfigured) {
        ResetBridge(DeviceExtension);
        DeviceExtension->BridgeConfigured = FALSE;
    }

    // Wait for all I/O to complete
    IoReleaseRemoveLockAndWait(&DeviceExtension->RemoveLock, Irp);

    // Pass IRP down
    IoSkipCurrentIrpStackLocation(Irp);
    status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);

    // Detach and delete our device object
    IoDetachDevice(DeviceExtension->LowerDeviceObject);
    IoDeleteDevice(DeviceExtension->DeviceObject);

    DbgPrint(DBG_INFO, "ITE8888 bridge device removed successfully");

    return status;
}

/*
 * Handle IRP_MN_STOP_DEVICE  
 */
NTSTATUS HandleStopDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
)
{
    PAGED_CODE();

    DbgPrint(DBG_INFO, "Stopping ITE8888 bridge device");

    // Reset bridge configuration
    if (DeviceExtension->BridgeConfigured) {
        ResetBridge(DeviceExtension);
        DeviceExtension->BridgeConfigured = FALSE;
    }

    // Pass IRP down
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
}

/*
 * Synchronous IRP completion routine
 */
NTSTATUS StartDeviceCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
)
{
    PKEVENT event = (PKEVENT)Context;
    
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    
    KeSetEvent(event, 0, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

/*
 * Get PCI location information
 */
NTSTATUS GetPciLocation(
    IN PDEVICE_EXTENSION DeviceExtension
)
{
    NTSTATUS status;
    ULONG busNumber, deviceNumber, functionNumber;
    PCI_SLOT_NUMBER slotNumber;
    ULONG vendorDevice;
    ULONG bytesRead;

    PAGED_CODE();

    // Search all PCI buses for ITE8888
    for (busNumber = 0; busNumber < 256; busNumber++) {
        for (deviceNumber = 0; deviceNumber < 32; deviceNumber++) {
            for (functionNumber = 0; functionNumber < 8; functionNumber++) {
                
                slotNumber.u.AsULONG = 0;
                slotNumber.u.bits.DeviceNumber = deviceNumber;
                slotNumber.u.bits.FunctionNumber = functionNumber;

                // Read Vendor ID and Device ID
                bytesRead = HalGetBusData(
                    PCIConfiguration,
                    busNumber,
                    slotNumber.u.AsULONG,
                    &vendorDevice,
                    sizeof(ULONG)
                );

                if (bytesRead == sizeof(ULONG)) {
                    USHORT vendorId = (USHORT)(vendorDevice & 0xFFFF);
                    USHORT deviceId = (USHORT)((vendorDevice >> 16) & 0xFFFF);

                    if (vendorId == ITE8888_VENDOR_ID && deviceId == ITE8888_DEVICE_ID) {
                        // Found ITE8888!
                        DeviceExtension->BusNumber = busNumber;
                        DeviceExtension->DeviceNumber = deviceNumber;
                        DeviceExtension->FunctionNumber = functionNumber;
                        return STATUS_SUCCESS;
                    }
                }

                // If function 0 doesn't exist, don't check other functions
                if (functionNumber == 0 && vendorDevice == 0xFFFFFFFF) {
                    break;
                }
            }
        }
    }

    return STATUS_DEVICE_NOT_FOUND;
}