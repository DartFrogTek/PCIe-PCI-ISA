#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/PciRootBridgeIo.h>

/**
    Driver entry point.

    @param[in] ImageHandle The firmware allocated handle for the EFI image.
    @param[in] SystemTable A pointer to the EFI System Table.

    @retval EFI_SUCCESS The entry point is executed successfully.
    @retval other Some error occurred when executing this entry point.
**/
EFI_STATUS
EFIAPI
IsaIoForwardingDxeDriverEntryPoint(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN NumberOfHandles;
    UINTN Index;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *PciRootBridgeIo;

    // Locate all PCI Root Bridge I/O protocols
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciRootBridgeIoProtocolGuid, NULL, &NumberOfHandles, &HandleBuffer);
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to locate PCI Root Bridge I/O protocols: %r\n", Status));
        return Status;
    }
    DEBUG((DEBUG_INFO, "Found %d PCI Root Bridge I/O protocols\n", NumberOfHandles));

    // Set ISA Motherboard I/O attribute for each PCI Root Bridge
    for (Index = 0; Index < NumberOfHandles; Index++)
    {
        Status = gBS->HandleProtocol(HandleBuffer[Index], &gEfiPciRootBridgeIoProtocolGuid, (VOID **)&PciRootBridgeIo);
        if (EFI_ERROR(Status))
        {
            continue;
        }
        DEBUG((DEBUG_INFO, "Setting ISA Motherboard I/O attribute for PCI Root Bridge %d\n", Index));

        // Enable ISA Motherboard I/O forwarding
        Status = PciRootBridgeIo->Attributes(PciRootBridgeIo, EfiPciAttributeOperationEnable, EFI_PCI_ATTRIBUTE_ISA_MOTHERBOARD_IO, NULL);
        if (EFI_ERROR(Status))
        {
            DEBUG((DEBUG_ERROR, "Failed to set ISA Motherboard I/O attribute: %r\n", Status));
        }
        else
        {
            DEBUG((DEBUG_INFO, "Successfully set ISA Motherboard I/O attribute\n"));
        }
    }

    // Free the handle buffer
    gBS->FreePool(HandleBuffer);

    return EFI_SUCCESS;
}