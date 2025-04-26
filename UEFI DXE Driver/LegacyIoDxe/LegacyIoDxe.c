// LegacyIoDxe.c
// Implementation of the Legacy I/O Protocol DXE driver

#include <Uefi.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include "LegacyIoProtocol.h"

EFI_GUID gLegacyIoProtocolGuid = LEGACY_IO_PROTOCOL_GUID;

LEGACY_IO_PROTOCOL mLegacyIoProtocol = {
    LegacyIoRead,
    LegacyIoWrite,
    0x0000, // Start of I/O range (0x00)
    0x00FF  // End of I/O range (0xFF)
};

/**
    Read data from an I/O port.

    @param[in]  This Protocol instance pointer
    @param[in]  Port I/O port to read
    @param[in]  Width Width of the data (8, 16, or 32 bits)
    @param[out] Data Buffer to store the read data

    @retval EFI_SUCCESS Data was read successfully
    @retval EFI_DEVICE_ERROR Error occurred during I/O operation
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyIoRead(
    IN LEGACY_IO_PROTOCOL *This,
    IN UINT16 Port,
    IN UINTN Width,
    OUT VOID *Data)
{
    // Check if the port is within our managed range
    if (Port < This->IoRangeStart || Port > This->IoRangeEnd)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Check parameters
    if (Data == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Perform the I/O read based on width
    switch (Width)
    {
    case 1:
        *(UINT8 *)Data = IoRead8(Port);
        break;
    case 2:
        *(UINT16 *)Data = IoRead16(Port);
        break;
    case 4:
        *(UINT32 *)Data = IoRead32(Port);
        break;
    default:
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

/**
    Write data to an I/O port.

    @param[in] This Protocol instance pointer
    @param[in] Port I/O port to write
    @param[in] Width Width of the data (8, 16, or 32 bits)
    @param[in] Data Data to write

    @retval EFI_SUCCESS Data was written successfully
    @retval EFI_DEVICE_ERROR Error occurred during I/O operation
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyIoWrite(
    IN LEGACY_IO_PROTOCOL *This,
    IN UINT16 Port,
    IN UINTN Width,
    IN VOID *Data)
{
    // Check if the port is within our managed range
    if (Port < This->IoRangeStart || Port > This->IoRangeEnd)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Check parameters
    if (Data == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Perform the I/O write based on width
    switch (Width)
    {
    case 1:
        IoWrite8(Port, *(UINT8 *)Data);
        break;
    case 2:
        IoWrite16(Port, *(UINT16 *)Data);
        break;
    case 4:
        IoWrite32(Port, *(UINT32 *)Data);
        break;
    default:
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

/**
    Enable I/O forwarding via PCI root bridge attributes.

    @param  None

    @retval EFI_SUCCESS Forwarding was enabled successfully
    @retval EFI_UNSUPPORTED Forwarding could not be enabled
*/
EFI_STATUS
EnableIoForwarding(
    VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *PciRootBridgeIo;
    UINT64 Attributes;
    UINT64 Supports;
    BOOLEAN ForwardingEnabled = FALSE;

    // Locate all PCI Root Bridge I/O Protocol instances
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciRootBridgeIoProtocolGuid, NULL, &HandleCount, &HandleBuffer);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to locate PCI Root Bridge I/O protocols: %r\n", Status));
        return Status;
    }

    DEBUG((DEBUG_INFO, "Found %d PCI Root Bridge I/O protocols\n", HandleCount));

    // Enable ISA I/O Forwarding for each PCI Root Bridge
    for (Index = 0; Index < HandleCount; Index++)
    {
        Status = gBS->HandleProtocol(HandleBuffer[Index], &gEfiPciRootBridgeIoProtocolGuid, (VOID **)&PciRootBridgeIo);

        if (EFI_ERROR(Status))
        {
            continue;
        }

        // Get current attributes and supported attributes
        Status = PciRootBridgeIo->GetAttributes(PciRootBridgeIo, &Supports, &Attributes);

        if (EFI_ERROR(Status))
        {
            DEBUG((DEBUG_ERROR, "Failed to get attributes for Root Bridge %d: %r\n", Index, Status));
            continue;
        }

        // Check if the root bridge supports ISA I/O forwarding
        if ((Supports & EFI_PCI_ATTRIBUTE_ISA_MOTHERBOARD_IO) == 0)
        {
            DEBUG((DEBUG_INFO, "Root Bridge %d does not support ISA I/O forwarding\n", Index));
            continue;
        }

        // Enable ISA I/O forwarding
        Status = PciRootBridgeIo->SetAttributes(PciRootBridgeIo, Attributes | EFI_PCI_ATTRIBUTE_ISA_MOTHERBOARD_IO, NULL, NULL);

        if (EFI_ERROR(Status))
        {
            DEBUG((DEBUG_ERROR, "Failed to set ISA I/O forwarding for Root Bridge %d: %r\n", Index, Status));
        }
        else
        {
            DEBUG((DEBUG_INFO, "Successfully enabled ISA I/O forwarding for Root Bridge %d\n", Index));
            ForwardingEnabled = TRUE;
        }
    }

    // Free the handle buffer
    gBS->FreePool(HandleBuffer);
    return ForwardingEnabled ? EFI_SUCCESS : EFI_UNSUPPORTED;
}

/**
    Driver entry point for the Legacy I/O Protocol DXE driver.

    @param[in] ImageHandle Image handle for this driver
    @param[in] SystemTable Pointer to the EFI System Table

    @retval EFI_SUCCESS Protocol was installed successfully
    @retval Others Error installing protocol
*/
EFI_STATUS
EFIAPI
LegacyIoDxeDriverEntryPoint(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE Handle;

    DEBUG((DEBUG_INFO, "Legacy I/O Protocol Driver Entry Point\n"));

    // Try to enable ISA I/O forwarding via PCI attributes
    Status = EnableIoForwarding();
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_WARN, "Failed to enable I/O forwarding via PCI attributes: %r\n", Status));
        DEBUG((DEBUG_INFO, "Continuing with protocol installation regardless...\n"));
    }

    // Install the Legacy I/O Protocol on a new handle
    Handle = NULL;
    Status = gBS->InstallProtocolInterface(&Handle, &gLegacyIoProtocolGuid, EFI_NATIVE_INTERFACE, &mLegacyIoProtocol);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to install Legacy I/O Protocol: %r\n", Status));
        return Status;
    }

    DEBUG((DEBUG_INFO, "Legacy I/O Protocol installed successfully\n"));
    return EFI_SUCCESS;
}