// LegacyIoProtocol.h
// Extended Protocol definition for Legacy I/O and DMA operations

#ifndef __LEGACY_IO_PROTOCOL_H__
#define __LEGACY_IO_PROTOCOL_H__

// Original protocol GUID - remains unchanged for compatibility
#define LEGACY_IO_PROTOCOL_GUID {0x12345678, 0x1234, 0x1234, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}}

// New protocol GUID for the extended version
#define LEGACY_IO_PROTOCOL_EX_GUID {0x87654321, 0x4321, 0x4321, {0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12}}

typedef struct _LEGACY_IO_PROTOCOL LEGACY_IO_PROTOCOL;
typedef struct _LEGACY_IO_PROTOCOL_EX LEGACY_IO_PROTOCOL_EX;

// DMA Buffer structure
typedef struct
{
    VOID *Buffer;
    UINTN Length;
    BOOLEAN IsPhysical;
    EFI_PHYSICAL_ADDRESS PhysicalAddress;
} LEGACY_DMA_BUFFER;

// DMA Channel structure
typedef struct
{
    UINT8 Channel;
    UINT8 Mode;
    UINT16 Count;
    EFI_PHYSICAL_ADDRESS Address;
    BOOLEAN IsActive;
} LEGACY_DMA_CHANNEL;

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
typedef EFI_STATUS(EFIAPI *LEGACY_IO_READ)(
    IN LEGACY_IO_PROTOCOL *This,
    IN UINT16 Port,
    IN UINTN Width,
    OUT VOID *Data);

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
typedef EFI_STATUS(EFIAPI *LEGACY_IO_WRITE)(
    IN LEGACY_IO_PROTOCOL *This,
    IN UINT16 Port,
    IN UINTN Width,
    IN VOID *Data);

/**
    Allocate a DMA buffer suitable for legacy ISA DMA operations.

    @param[in]  This Protocol instance pointer
    @param[in]  Size Size of the buffer to allocate
    @param[out] DmaBuffer Allocated DMA buffer information

    @retval EFI_SUCCESS Buffer allocated successfully
    @retval EFI_OUT_OF_RESOURCES Not enough resources to allocate buffer
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
typedef EFI_STATUS(EFIAPI *LEGACY_DMA_ALLOCATE_BUFFER)(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINTN Size,
    OUT LEGACY_DMA_BUFFER *DmaBuffer);

/**
    Free a previously allocated DMA buffer.

    @param[in] This Protocol instance pointer
    @param[in] DmaBuffer DMA buffer to free

    @retval EFI_SUCCESS Buffer freed successfully
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
typedef EFI_STATUS(EFIAPI *LEGACY_DMA_FREE_BUFFER)(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN LEGACY_DMA_BUFFER *DmaBuffer);

/**
    Program an ISA DMA channel for operation.

    @param[in] This Protocol instance pointer
    @param[in] Channel DMA channel to program (0-7)
    @param[in] Mode DMA mode (read/write/verify)
    @param[in] DmaBuffer DMA buffer to use
    @param[in] Count Transfer count (in bytes)
    @param[in] AutoInitialize Whether to auto-initialize the channel

    @retval EFI_SUCCESS DMA channel programmed successfully
    @retval EFI_DEVICE_ERROR Error programming the DMA controller
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
typedef EFI_STATUS(EFIAPI *LEGACY_DMA_PROGRAM_CHANNEL)(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel,
    IN UINT8 Mode,
    IN LEGACY_DMA_BUFFER *DmaBuffer,
    IN UINT16 Count,
    IN BOOLEAN AutoInitialize);

/**
    Start a DMA transfer on a programmed channel.

    @param[in] This Protocol instance pointer
    @param[in] Channel DMA channel to start (0-7)

    @retval EFI_SUCCESS DMA transfer started successfully
    @retval EFI_NOT_READY DMA channel not properly programmed
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
typedef EFI_STATUS(EFIAPI *LEGACY_DMA_START)(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel);

/**
    Check the status of a DMA channel.

    @param[in]  This Protocol instance pointer
    @param[in]  Channel DMA channel to check (0-7)
    @param[out] BytesRemaining Number of bytes remaining in the transfer
    @param[out] IsActive Whether the channel is still active

    @retval EFI_SUCCESS Status obtained successfully
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
typedef EFI_STATUS(EFIAPI *LEGACY_DMA_STATUS)(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel,
    OUT UINT16 *BytesRemaining,
    OUT BOOLEAN *IsActive);

/**
    Stop a DMA transfer on a channel.

    @param[in] This Protocol instance pointer
    @param[in] Channel DMA channel to stop (0-7)

    @retval EFI_SUCCESS DMA transfer stopped successfully
    @retval EFI_INVALID_PARAMETER Invalid parameter
*/
typedef EFI_STATUS(EFIAPI *LEGACY_DMA_STOP)(
    IN LEGACY_IO_PROTOCOL_EX *This,
    IN UINT8 Channel);

// Structure containing the original Legacy I/O Protocol
struct _LEGACY_IO_PROTOCOL
{
    LEGACY_IO_READ Read;
    LEGACY_IO_WRITE Write;
    UINT16 IoRangeStart;
    UINT16 IoRangeEnd;
};

// Structure containing the extended Legacy I/O Protocol with DMA support
struct _LEGACY_IO_PROTOCOL_EX
{
    // Original I/O functions for backward compatibility
    LEGACY_IO_READ Read;
    LEGACY_IO_WRITE Write;
    UINT16 IoRangeStart;
    UINT16 IoRangeEnd;

    // Extended DMA functions
    LEGACY_DMA_ALLOCATE_BUFFER AllocateDmaBuffer;
    LEGACY_DMA_FREE_BUFFER FreeDmaBuffer;
    LEGACY_DMA_PROGRAM_CHANNEL ProgramDmaChannel;
    LEGACY_DMA_START StartDma;
    LEGACY_DMA_STATUS GetDmaStatus;
    LEGACY_DMA_STOP StopDma;

    // Additional DMA-related data
    LEGACY_DMA_CHANNEL DmaChannels[8]; // Information about each DMA channel
};

extern EFI_GUID gLegacyIoProtocolGuid;
extern EFI_GUID gLegacyIoProtocolExGuid;

#endif