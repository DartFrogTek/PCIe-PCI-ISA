// LegacyIoDxeEx.c
// Implementation of the Extended Legacy I/O Protocol DXE driver with DMA support

#include <Uefi.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include "LegacyIoProtocol.h"

EFI_GUID gLegacyIoProtocolGuid = LEGACY_IO_PROTOCOL_GUID;
EFI_GUID gLegacyIoProtocolExGuid = LEGACY_IO_PROTOCOL_EX_GUID;

// ISA DMA controller port definitions
#define DMA_ADDR_PORT0 0x00 // Channel 0 address port
#define DMA_ADDR_PORT1 0x02 // Channel 1 address port
#define DMA_ADDR_PORT2 0x04 // Channel 2 address port
#define DMA_ADDR_PORT3 0x06 // Channel 3 address port
#define DMA_ADDR_PORT4 0xC0 // Channel 4 address port
#define DMA_ADDR_PORT5 0xC4 // Channel 5 address port
#define DMA_ADDR_PORT6 0xC8 // Channel 6 address port
#define DMA_ADDR_PORT7 0xCC // Channel 7 address port

#define DMA_COUNT_PORT0 0x01 // Channel 0 count port
#define DMA_COUNT_PORT1 0x03 // Channel 1 count port
#define DMA_COUNT_PORT2 0x05 // Channel 2 count port
#define DMA_COUNT_PORT3 0x07 // Channel 3 count port
#define DMA_COUNT_PORT4 0xC2 // Channel 4 count port
#define DMA_COUNT_PORT5 0xC6 // Channel 5 count port
#define DMA_COUNT_PORT6 0xCA // Channel 6 count port
#define DMA_COUNT_PORT7 0xCE // Channel 7 count port

#define DMA_PAGE_PORT0 0x87 // Channel 0 page port
#define DMA_PAGE_PORT1 0x83 // Channel 1 page port
#define DMA_PAGE_PORT2 0x81 // Channel 2 page port
#define DMA_PAGE_PORT3 0x82 // Channel 3 page port
#define DMA_PAGE_PORT4 0x8F // Channel 4 page port
#define DMA_PAGE_PORT5 0x8B // Channel 5 page port
#define DMA_PAGE_PORT6 0x89 // Channel 6 page port
#define DMA_PAGE_PORT7 0x8A // Channel 7 page port

#define DMA_CMD_PORT0 0x08   // Command port for 8-bit DMA
#define DMA_CMD_PORT1 0xD0   // Command port for 16-bit DMA
#define DMA_STAT_PORT0 0x08  // Status port for 8-bit DMA
#define DMA_STAT_PORT1 0xD0  // Status port for 16-bit DMA
#define DMA_MASK_PORT0 0x0A  // Mask port for 8-bit DMA
#define DMA_MASK_PORT1 0xD4  // Mask port for 16-bit DMA
#define DMA_MODE_PORT0 0x0B  // Mode port for 8-bit DMA
#define DMA_MODE_PORT1 0xD6  // Mode port for 16-bit DMA
#define DMA_CLEAR_PORT0 0x0C // Clear flip-flop port for 8-bit DMA
#define DMA_CLEAR_PORT1 0xD8 // Clear flip-flop port for 16-bit DMA

// DMA modes
#define DMA_MODE_DEMAND 0x00
#define DMA_MODE_SINGLE 0x40
#define DMA_MODE_BLOCK 0x80
#define DMA_MODE_CASCADE 0xC0

#define DMA_MODE_VERIFY 0x00
#define DMA_MODE_WRITE 0x04 // Write to memory
#define DMA_MODE_READ 0x08  // Read from memory
#define DMA_MODE_AUTO 0x10  // Auto-initialize

// Populated by driver
LEGACY_IO_PROTOCOL_EX mLegacyIoProtocolEx = {
    LegacyIoRead,
    LegacyIoWrite,
    0x0000, // Start of I/O range (0x00)
    0x00FF, // End of I/O range (0xFF)
    LegacyDmaAllocateBuffer,
    LegacyDmaFreeBuffer,
    LegacyDmaProgramChannel,
    LegacyDmaStart,
    LegacyDmaStatus,
    LegacyDmaStop,
    {0} // DMA Channels - zeroed initially
};

/**
    Read data from an I/O port.

    @param[in] This Protocol instance pointer
    @param[in] Port I/O port to read
    @param[in] Width Width of the data (8, 16, or 32 bits)
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
    // Implementation same as in original driver
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
    // Implementation same as in original driver
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
    Allocate a DMA buffer suitable for legacy ISA DMA operations.

    @param[in]  This Protocol instance pointer
    @param[in]  Size Size of the buffer to allocate
    @param[out] DmaBuffer Allocated DMA buffer information

    @retval EFI_SUCCESS Buffer allocated successfully
    @retval EFI_OUT_OF_RESOURCES Not enough resources to allocate buffer
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyDmaAllocateBuffer(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINTN Size,
    OUT LEGACY_DMA_BUFFER *DmaBuffer)
{
    EFI_STATUS Status;
    VOID *Buffer;
    EFI_PHYSICAL_ADDRESS PhysicalAddress;

    // Check parameters
    if (DmaBuffer == NULL || Size == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    // ISA DMA requires the buffer to be below 16MB and aligned
    // Note: Allocate an aligned buffer to ensure it crosses no page boundaries
    Status = gBS->AllocatePool(EfiBootServicesData, Size, &Buffer);

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    // Get the physical address
    PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Buffer;

    // Ensure the buffer is below 16MB (required for ISA DMA)
    if (PhysicalAddress > 0xFFFFFF)
    {
        DEBUG((DEBUG_ERROR, "DMA buffer allocated above 16MB, not usable for ISA DMA\n"));
        gBS->FreePool(Buffer);
        return EFI_OUT_OF_RESOURCES;
    }

    // Zero the buffer
    ZeroMem(Buffer, Size);

    // Populate the DMA buffer structure
    DmaBuffer->Buffer = Buffer;
    DmaBuffer->Length = Size;
    DmaBuffer->IsPhysical = TRUE;
    DmaBuffer->PhysicalAddress = PhysicalAddress;

    return EFI_SUCCESS;
}

/**
    Free a previously allocated DMA buffer.

    @param[in] This Protocol instance pointer
    @param[in] DmaBuffer DMA buffer to free

    @retval EFI_SUCCESS Buffer freed successfully
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyDmaFreeBuffer(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN LEGACY_DMA_BUFFER *DmaBuffer)
{
    // Check parameters
    if (DmaBuffer == NULL || DmaBuffer->Buffer == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Free the buffer
    gBS->FreePool(DmaBuffer->Buffer);

    // Clear the DMA buffer structure
    DmaBuffer->Buffer = NULL;
    DmaBuffer->Length = 0;
    DmaBuffer->IsPhysical = FALSE;
    DmaBuffer->PhysicalAddress = 0;

    return EFI_SUCCESS;
}

/**
    Get the appropriate DMA ports for a given channel.

    @param[in]  Channel DMA channel (0-7)
    @param[out] AddrPort Address register port
    @param[out] CountPort Count register port
    @param[out] PagePort Page register port
    @param[out] CmdPort Command register port
    @param[out] MaskPort Mask register port
    @param[out] ModePort Mode register port
    @param[out] ClearPort Clear flip-flop port
    @param[out] StatPort Status register port
*/
VOID GetDmaPorts(
    IN UINT8 Channel,
    OUT UINT16 *AddrPort,
    OUT UINT16 *CountPort,
    OUT UINT16 *PagePort,
    OUT UINT16 *CmdPort,
    OUT UINT16 *MaskPort,
    OUT UINT16 *ModePort,
    OUT UINT16 *ClearPort,
    OUT UINT16 *StatPort)
{
    // Determine if this is an 8-bit or 16-bit DMA channel
    BOOLEAN Is16BitChannel = (Channel >= 4);

    // Set common ports based on channel type
    if (Is16BitChannel)
    {
        *CmdPort = DMA_CMD_PORT1;
        *StatPort = DMA_STAT_PORT1;
        *MaskPort = DMA_MASK_PORT1;
        *ModePort = DMA_MODE_PORT1;
        *ClearPort = DMA_CLEAR_PORT1;
    }
    else
    {
        *CmdPort = DMA_CMD_PORT0;
        *StatPort = DMA_STAT_PORT0;
        *MaskPort = DMA_MASK_PORT0;
        *ModePort = DMA_MODE_PORT0;
        *ClearPort = DMA_CLEAR_PORT0;
    }

    // Set channel-specific ports
    switch (Channel)
    {
    case 0:
        *AddrPort = DMA_ADDR_PORT0;
        *CountPort = DMA_COUNT_PORT0;
        *PagePort = DMA_PAGE_PORT0;
        break;
    case 1:
        *AddrPort = DMA_ADDR_PORT1;
        *CountPort = DMA_COUNT_PORT1;
        *PagePort = DMA_PAGE_PORT1;
        break;
    case 2:
        *AddrPort = DMA_ADDR_PORT2;
        *CountPort = DMA_COUNT_PORT2;
        *PagePort = DMA_PAGE_PORT2;
        break;
    case 3:
        *AddrPort = DMA_ADDR_PORT3;
        *CountPort = DMA_COUNT_PORT3;
        *PagePort = DMA_PAGE_PORT3;
        break;
    case 4:
        *AddrPort = DMA_ADDR_PORT4;
        *CountPort = DMA_COUNT_PORT4;
        *PagePort = DMA_PAGE_PORT4;
        break;
    case 5:
        *AddrPort = DMA_ADDR_PORT5;
        *CountPort = DMA_COUNT_PORT5;
        *PagePort = DMA_PAGE_PORT5;
        break;
    case 6:
        *AddrPort = DMA_ADDR_PORT6;
        *CountPort = DMA_COUNT_PORT6;
        *PagePort = DMA_PAGE_PORT6;
        break;
    case 7:
        *AddrPort = DMA_ADDR_PORT7;
        *CountPort = DMA_COUNT_PORT7;
        *PagePort = DMA_PAGE_PORT7;
        break;
    default:
        // Invalid channel - set ports to 0
        *AddrPort = 0;
        *CountPort = 0;
        *PagePort = 0;
        break;
    }
}

/**
Program an ISA DMA channel for operation.

    @param[in] This Protocol instance pointer
    @param[in] Channel DMA channel to program(0-7)
    @param[in] Mode DMA mode (read/write/verify)
    @param[in] DmaBuffer DMA buffer to use
    @param[in] Count Transfer count (in bytes)
    @param[in] AutoInitialize Whether to auto-initialize the channel

    @retval EFI_SUCCESS DMA channel programmed successfully
    @retval EFI_DEVICE_ERROR Error programming the DMA controller
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyDmaProgramChannel(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel,
    IN UINT8 Mode,
    IN LEGACY_DMA_BUFFER *DmaBuffer,
    IN UINT16 Count,
    IN BOOLEAN AutoInitialize)
{
    UINT16 AddrPort;
    UINT16 CountPort;
    UINT16 PagePort;
    UINT16 CmdPort;
    UINT16 MaskPort;
    UINT16 ModePort;
    UINT16 ClearPort;
    UINT16 StatPort;
    UINT8 DmaMode;
    UINT32 PhysicalAddr;
    UINT8 Page;
    UINT16 TransferCount;

    // Check parameters
    if (Channel > 7 || DmaBuffer == NULL || DmaBuffer->Buffer == NULL || Count == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Get the DMA controller ports for this channel
    GetDmaPorts(Channel, &AddrPort, &CountPort, &PagePort, &CmdPort, &MaskPort, &ModePort, &ClearPort, &StatPort);

    // Ensure the transfer size is appropriate for this channel type
    // Channels 0-3 are 8-bit, 4-7 are 16-bit
    BOOLEAN Is16BitChannel = (Channel >= 4);

    if (Is16BitChannel)
    {
        // 16-bit DMA counts in words, not bytes
        if (Count % 2 != 0)
        {
            DEBUG((DEBUG_WARN, "16-bit DMA transfer with odd byte count %d, rounding down\n", Count));
            Count = Count & ~0x1; // Clear the lowest bit
        }

        // Convert byte count to word count for 16-bit channels
        TransferCount = Count / 2;
    }
    else
    {
        TransferCount = Count;
    }

    // For 16-bit channels, the address must be even
    PhysicalAddr = (UINT32)DmaBuffer->PhysicalAddress;
    if (Is16BitChannel && (PhysicalAddr & 0x1))
    {
        DEBUG((DEBUG_ERROR, "16-bit DMA buffer not 2-byte aligned: 0x%x\n", PhysicalAddr));
        return EFI_INVALID_PARAMETER;
    }

    // ISA DMA can only access memory below 16MB
    if (PhysicalAddr > 0xFFFFFF)
    {
        DEBUG((DEBUG_ERROR, "DMA buffer above 16MB boundary: 0x%x\n", PhysicalAddr));
        return EFI_INVALID_PARAMETER;
    }

    // Prepare the DMA mode value
    DmaMode = (UINT8)(Channel & 0x03); // Lower 2 bits are the channel
    DmaMode |= (UINT8)(Mode & 0x0C);   // Bits 2-3 are the transfer type
    DmaMode |= DMA_MODE_SINGLE;        // Use single transfer mode

    if (AutoInitialize)
    {
        DmaMode |= DMA_MODE_AUTO;
    }

    // Mask (disable) the DMA channel
    IoWrite8(MaskPort, (UINT8)(0x04 | (Channel & 0x03))); // Bit 2 set = mask, bits 1-0 = channel

    // Clear the byte pointer flip-flop
    IoWrite8(ClearPort, 0xFF);

    // Program the mode register
    IoWrite8(ModePort, DmaMode);

    // Program the address registeR(low and high bytes)
    IoWrite8(AddrPort, (UINT8)(PhysicalAddr & 0xFF));
    IoWrite8(AddrPort, (UINT8)((PhysicalAddr >> 8) & 0xFF));

    // Program the page register
    Page = (UINT8)((PhysicalAddr >> 16) & 0xFF);
    IoWrite8(PagePort, Page);

    // Program the count registeR(low and high bytes)
    // Note: ISA DMA counts from 1 to N, so we subtract 1
    IoWrite8(CountPort, (UINT8)((TransferCount - 1) & 0xFF));
    IoWrite8(CountPort, (UINT8)(((TransferCount - 1) >> 8) & 0xFF));

    // Store the DMA channel information
    This->DmaChannels[Channel].Channel = Channel;
    This->DmaChannels[Channel].Mode = DmaMode;
    This->DmaChannels[Channel].Count = Count;
    This->DmaChannels[Channel].Address = PhysicalAddr;
    This->DmaChannels[Channel].IsActive = FALSE;

    DEBUG((DEBUG_INFO, "DMA Channel %d programmed: Addr=0x%x, Count=%d, Mode=0x%x\n",
           Channel, PhysicalAddr, Count, DmaMode));

    return EFI_SUCCESS;
}

/**
    Start a DMA transfer on a programmed channel.

    @param[in] This Protocol instance pointer
    @param[in] Channel DMA channel to start (0-7)

    @retval EFI_SUCCESS DMA transfer started successfully
    @retval EFI_NOT_READY DMA channel not properly programmed
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyDmaStart(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel)
{
    UINT16 MaskPort;
    UINT16 AddrPort;
    UINT16 CountPort;
    UINT16 PagePort;
    UINT16 CmdPort;
    UINT16 ModePort;
    UINT16 ClearPort;
    UINT16 StatPort;

    // Check parameters
    if (Channel > 7)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Verify this channel has been programmed
    if (This->DmaChannels[Channel].Count == 0)
    {
        return EFI_NOT_READY;
    }

    // Get the DMA controller ports for this channel
    GetDmaPorts(Channel, &AddrPort, &CountPort, &PagePort, &CmdPort, &MaskPort, &ModePort, &ClearPort, &StatPort);

    // Unmask (enable) the DMA channel
    IoWrite8(MaskPort, (UINT8)(Channel & 0x03)); // Bit 2 clear = unmask, bits 1-0 = channel

    // Mark the channel as active
    This->DmaChannels[Channel].IsActive = TRUE;

    DEBUG((DEBUG_INFO, "DMA Channel %d started\n", Channel));

    return EFI_SUCCESS;
}

/**
    Check the status of a DMA channel.

    @param[in]  This Protocol instance pointer
    @param[in]  Channel DMA channel to check (0-7)
    @param[out] BytesRemaining Number of bytes remaining in the transfer
    @param[out] IsActive Whether the channel is still active

    @retval EFI_SUCCESS Status obtained successfully
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyDmaStatus(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel,
    OUT UINT16 *BytesRemaining,
    OUT BOOLEAN *IsActive)
{
    UINT16 AddrPort;
    UINT16 CountPort;
    UINT16 PagePort;
    UINT16 CmdPort;
    UINT16 MaskPort;
    UINT16 ModePort;
    UINT16 ClearPort;
    UINT16 StatPort;
    UINT8 Status;
    UINT16 Count;
    UINT8 ChannelBit;
    BOOLEAN Is16BitChannel;

    // Check parameters
    if (Channel > 7 || BytesRemaining == NULL || IsActive == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Get the DMA controller ports for this channel
    GetDmaPorts(Channel, &AddrPort, &CountPort, &PagePort, &CmdPort, &MaskPort, &ModePort, &ClearPort, &StatPort);

    // Determine which bit to check in the status register
    ChannelBit = (UINT8)(1 << (Channel & 0x03));

    // Check if this is a 16-bit channel
    Is16BitChannel = (Channel >= 4);

    // Read the status register - a set bit means the channel is done
    Status = IoRead8(StatPort);

    // Clear the flip-flop
    IoWrite8(ClearPort, 0xFF);

    // Read the current count
    Count = IoRead8(CountPort);
    Count |= (UINT16)(IoRead8(CountPort) << 8);

    // Convert count to bytes remaining (count is from 0 to N-1)
    Count++;

    // For 16-bit channels, convert word count to byte count
    if (Is16BitChannel)
    {
        Count *= 2;
    }

    // DMA is active if its request bit is clear in the status register
    *IsActive = ((Status & ChannelBit) == 0);
    *BytesRemaining = Count;

    // Update the stored state
    This->DmaChannels[Channel].IsActive = *IsActive;

    return EFI_SUCCESS;
}

/**
    Stop a DMA transfer on a channel.

    @param[in] This Protocol instance pointer
    @param[in] Channel DMA channel to stop (0-7)

    @retval EFI_SUCCESS DMA transfer stopped successfully
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
EFI_STATUS
EFIAPI
LegacyDmaStop(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel)
{
    UINT16 AddrPort;
    UINT16 CountPort;
    UINT16 PagePort;
    UINT16 CmdPort;
    UINT16 MaskPort;
    UINT16 ModePort;
    UINT16 ClearPort;
    UINT16 StatPort;

    // Check parameters
    if (Channel > 7)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Get the DMA controller ports for this channel
    GetDmaPorts(Channel, &AddrPort, &CountPort, &PagePort, &CmdPort, &MaskPort, &ModePort, &ClearPort, &StatPort);

    // Mask (disable) the DMA channel
    IoWrite8(MaskPort, (UINT8)(0x04 | (Channel & 0x03))); // Bit 2 set = mask, bits 1-0 = channel

    // Mark the channel as inactive
    This->DmaChannels[Channel].IsActive = FALSE;

    DEBUG((DEBUG_INFO, "DMA Channel %d stopped\n", Channel));

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
    Driver entry point for the Extended Legacy I/O Protocol DXE driver.

    @param[in] ImageHandle Image handle for this driver
    @param[in] SystemTable Pointer to the EFI System Table

    @retval EFI_SUCCESS Protocol was installed successfully
    @retval Others Error installing protocol
*/
EFI_STATUS
EFIAPI
LegacyIoDxeExDriverEntryPoint(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE Handle;

    DEBUG((DEBUG_INFO, "Extended Legacy I/O Protocol Driver Entry Point\n"));

    // Try to enable ISA I/O forwarding via PCI attributes
    Status = EnableIoForwardinG();
    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_WARN, "Failed to enable I/O forwarding via PCI attributes: %r\n", Status));
        DEBUG((DEBUG_INFO, "Continuing with protocol installation regardless...\n"));
    }

    // Zero initialize all DMA channels
    ZeroMem(mLegacyIoProtocolEx.DmaChannels, sizeof(LEGACY_DMA_CHANNEL) * 8);

    // Install the Legacy I/O Protocol and Legacy I/O Ex Protocol on a new handle
    Handle = NULL;
    Status = gBS->InstallMultipleProtocolInterfaces(&Handle, &gLegacyIoProtocolGuid, (VOID *)&mLegacyIoProtocolEx, &gLegacyIoProtocolExGuid, (VOID *)&mLegacyIoProtocolEx, NULL);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to install Legacy I/O Protocols: %r\n", Status));
        return Status;
    }

    DEBUG((DEBUG_INFO, "Legacy I/O Protocols installed successfully\n"));
    return EFI_SUCCESS;
}