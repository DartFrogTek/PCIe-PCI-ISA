/*
 * ite8888f.h
 *
 * ITE8888F PCI-to-ISA Bridge definitions for PicoGUS support
 */

#ifndef _ITE8888F_H_
#define _ITE8888F_H_

// ITE8888F PCI Identification
#define ITE8888F_VENDOR_ID      0x1283
#define ITE8888F_DEVICE_ID      0x8888

// PCI Configuration Register Offsets
#define ITE8888F_CMD_REG        0x04    // Command Register
#define ITE8888F_MISC_CTRL      0x50    // Miscellaneous Control
#define ITE8888F_DELAYED_TIMER  0x54    // Delayed Transaction Timer
#define ITE8888F_IO_SPACE_0     0x58    // I/O Space 0 (0x2xx)
#define ITE8888F_IO_SPACE_1     0x5C    // I/O Space 1 (0x3xx)
#define ITE8888F_IO_SPACE_2     0x60    // I/O Space 2 (0x7x6)
#define ITE8888F_MEM_SPACE_0    0x70    // Memory Space 0
#define ITE8888F_DDMA_CH_0_1    0x40    // DDMA Channels 0-1
#define ITE8888F_DDMA_CH_5      0x48    // DDMA Channel 5 + Type F timing
#define ITE8888F_DDMA_CH_6_7    0x4C    // DDMA Channels 6-7

// Command Register Flags
#define ITE8888F_CMD_IO_ENABLE  0x0001
#define ITE8888F_CMD_MEM_ENABLE 0x0002
#define ITE8888F_CMD_BUS_MASTER 0x0004

// Miscellaneous Control Flags
#define ITE8888F_MISC_SUBTRACTIVE_DECODE    0x01
#define ITE8888F_MISC_DELAYED_TRANSACTION   0x02
#define ITE8888F_MISC_IO_RECOVERY_MASK      0x0F00
#define ITE8888F_MISC_IO_RECOVERY_SHIFT     8

// I/O Space Configuration Flags
#define ITE8888F_IO_ENABLE      0x80000000
#define ITE8888F_IO_FAST_DECODE 0x10000000
#define ITE8888F_IO_SIZE_1      0x00000000  // 1 byte
#define ITE8888F_IO_SIZE_8      0x03000000  // 8 bytes
#define ITE8888F_IO_SIZE_16     0x04000000  // 16 bytes
#define ITE8888F_IO_SIZE_32     0x05000000  // 32 bytes

// Memory Space Configuration Flags
#define ITE8888F_MEM_ENABLE     0x80000000
#define ITE8888F_MEM_FAST       0x10000000
#define ITE8888F_MEM_SIZE_32K   0x05000000

// DMA Configuration
#define ITE8888F_DDMA_ENABLE    0x80000000
#define ITE8888F_DDMA_TYPE_F    0x40000000

// Standard GUS I/O Port Offsets
#define GUS_BASE_220            0x220
#define GUS_BASE_240            0x240
#define GUS_BASE_260            0x260
#define GUS_MIDI_OFFSET         0x100   // 0x320 = 0x220 + 0x100
#define GUS_REVISION_OFFSET     0x4E6   // 0x706 = 0x220 + 0x4E6

// Bridge context structure
typedef struct _ITE8888F_CONTEXT {
    PDEVICE_OBJECT      bridge_device;
    ULONG               bus_number;
    ULONG               device_number;
    ULONG               function_number;
    BOOLEAN             bridge_configured;
    ULONG               gus_base_port;      // 0x220, 0x240, or 0x260
    ULONG               dma_channels;
    ULONG               irq_line;
} ITE8888F_CONTEXT, *PITE8888F_CONTEXT;

// Function prototypes
NTSTATUS ITE8888F_FindBridge(PITE8888F_CONTEXT Context);
NTSTATUS ITE8888F_ConfigureBridge(PITE8888F_CONTEXT Context, ULONG BasePort, ULONG DmaChannels, ULONG IrqLine);
NTSTATUS ITE8888F_SetupIoSpaces(PITE8888F_CONTEXT Context, ULONG BasePort);
NTSTATUS ITE8888F_SetupDmaChannels(PITE8888F_CONTEXT Context, ULONG DmaChannels);
NTSTATUS ITE8888F_EnableBridge(PITE8888F_CONTEXT Context);
BOOLEAN ITE8888F_ValidateBridgeConfig(PITE8888F_CONTEXT Context);

// PCI Configuration space access helpers
ULONG ITE8888F_ReadPciConfig(PITE8888F_CONTEXT Context, ULONG Offset);
NTSTATUS ITE8888F_WritePciConfig(PITE8888F_CONTEXT Context, ULONG Offset, ULONG Value);

#endif // _ITE8888F_H_