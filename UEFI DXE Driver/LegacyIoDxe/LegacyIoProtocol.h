// LegacyIoProtocol.h
// Protocol definition for Legacy I/O operations

#ifndef __LEGACY_IO_PROTOCOL_H__
#define __LEGACY_IO_PROTOCOL_H__

#define LEGACY_IO_PROTOCOL_GUID {0x12345678, 0x1234, 0x1234, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}}

typedef struct _LEGACY_IO_PROTOCOL LEGACY_IO_PROTOCOL;

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

// Structure containing the Legacy I/O Protocol
struct _LEGACY_IO_PROTOCOL
{
    LEGACY_IO_READ Read;
    LEGACY_IO_WRITE Write;
    UINT16 IoRangeStart;
    UINT16 IoRangeEnd;
};

extern EFI_GUID gLegacyIoProtocolGuid;

#endif