#pragma once
#ifndef IT8888VDMA_PUBLIC_H
#define IT8888VDMA_PUBLIC_H

/*
   Shared user/kernel IOCTL contract.

   IMPORTANT:
   - Kernel-mode builds must NOT include the user-mode header winioctl.h.
     It pulls in UM-only typedefs such as DWORD and causes cascades of syntax
     errors when compiled after ntddk.h/wdf.h.
   - In kernel mode, ntddk.h/wdm.h already define CTL_CODE, METHOD_BUFFERED,
     FILE_ANY_ACCESS, etc. it8888.h includes ntddk.h before this header.
   - In user mode, include Windows.h/winioctl.h and stdint.h.
*/

#if defined(_KERNEL_MODE) || defined(_NTDDK_) || defined(_WDMDDK_) || \
    defined(_NTDDK_INCLUDED_) || defined(_WDF_H_)
/* Kernel mode: fixed-width aliases backed by WDK types. */
typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdint.h>
#include <windows.h>
#include <winioctl.h>
#endif

#define IT8888_DEVICE_DOS_NAME "\\\\.\\IT8888VDMA"
#define IT8888_NT_DEVICE_NAME L"\\Device\\IT8888VDMA"
#define IT8888_DOS_DEVICE_NAME L"\\DosDevices\\IT8888VDMA"

#define IT8888_VENDOR_ID_DEFAULT 0x1283
#define IT8888_DEVICE_ID_DEFAULT 0x8888

#define FILE_DEVICE_IT8888VDMA 0x8888

#define IT8888_IOCTL(index, method, access) \
  CTL_CODE(FILE_DEVICE_IT8888VDMA, (index), (method), (access))

#define IOCTL_IT8888_GET_INFO \
  IT8888_IOCTL(0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_CFG_READ \
  IT8888_IOCTL(0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_CFG_WRITE \
  IT8888_IOCTL(0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_PORT_READ \
  IT8888_IOCTL(0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_PORT_WRITE \
  IT8888_IOCTL(0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_INIT_DEFAULT \
  IT8888_IOCTL(0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DMA_ALLOC \
  IT8888_IOCTL(0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DMA_FREE \
  IT8888_IOCTL(0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DMA_INFO \
  IT8888_IOCTL(0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DEBUG_DUMP \
  IT8888_IOCTL(0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_IT8888_8237_RESET \
  IT8888_IOCTL(0x820, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_8237_OUT \
  IT8888_IOCTL(0x821, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_8237_IN \
  IT8888_IOCTL(0x822, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_8237_SNAPSHOT \
  IT8888_IOCTL(0x823, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_8237_PREPARE \
  IT8888_IOCTL(0x824, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_IT8888_DDMA_ARM \
  IT8888_IOCTL(0x840, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DDMA_START \
  IT8888_IOCTL(0x841, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DDMA_POLL \
  IT8888_IOCTL(0x842, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DDMA_STATUS \
  IT8888_IOCTL(0x843, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_DDMA_CLEAR \
  IT8888_IOCTL(0x844, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_IT8888_IRQ_STATUS \
  IT8888_IOCTL(0x860, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_IRQ_ACK \
  IT8888_IOCTL(0x861, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_WAIT_IRQ \
  IT8888_IOCTL(0x862, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_IT8888_TRACE_GET \
  IT8888_IOCTL(0x880, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_TRACE_CLEAR \
  IT8888_IOCTL(0x881, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_PANIC_RESET \
  IT8888_IOCTL(0x882, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IT8888_CLEAR_ERRORS \
  IT8888_IOCTL(0x883, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IT8888_DDMA_FLAG_DRY_RUN 0x00000001u
#define IT8888_DDMA_FLAG_MASTER_CLEAR 0x00000040u
#define IT8888_DDMA_FLAG_UNMASK 0x00000080u
#define IT8888_DDMA_FLAG_SOFT_REQUEST 0x00000100u
#define IT8888_DDMA_FLAG_NO_CFG_INIT 0x00000200u
#define IT8888_DDMA_FLAG_POLL_AFTER 0x00000400u

#define IT8888_DIR_VERIFY 0u
#define IT8888_DIR_ISA_TO_RAM 1u
#define IT8888_DIR_RAM_TO_ISA 2u

#define IT8888_TRACE_MAX_USER 128
#define IT8888_TRACE_RING_SIZE 512

#pragma pack(push, 1)

typedef struct IT8888_INFO
{
  uint16_t VendorId;
  uint16_t DeviceId;
  uint8_t RevisionId;
  uint8_t Bus;
  uint8_t Device;
  uint8_t Function;
  uint8_t Started;
  uint32_t PciCommandStatus;
  uint32_t Cfg40;
  uint32_t Cfg44;
  uint32_t Cfg48;
  uint32_t Cfg4C;
  uint32_t Cfg50;
  uint32_t Cfg54;
  uint16_t DdmaBase[8];
  uint64_t DmaLogicalAddress;
  uint32_t DmaSize;
  uint32_t IrqCount;
  uint32_t LastPciStatus;
} IT8888_INFO, *PIT8888_INFO;

typedef struct IT8888_CFG_ACCESS
{
  uint16_t Offset;
  uint8_t Width;
  uint8_t Reserved;
  uint32_t Value;
} IT8888_CFG_ACCESS, *PIT8888_CFG_ACCESS;

typedef struct IT8888_PORT_ACCESS
{
  uint16_t Port;
  uint8_t Width;
  uint8_t Reserved;
  uint32_t Value;
} IT8888_PORT_ACCESS, *PIT8888_PORT_ACCESS;

typedef struct IT8888_DMA_ALLOC_REQUEST
{
  uint32_t Size;
  uint32_t Reserved;
} IT8888_DMA_ALLOC_REQUEST, *PIT8888_DMA_ALLOC_REQUEST;

typedef struct IT8888_DMA_INFO
{
  uint32_t BufferId;
  uint32_t Size;
  uint64_t LogicalAddress;
  uint64_t KernelVaForDebug;
} IT8888_DMA_INFO, *PIT8888_DMA_INFO;

typedef struct IT8888_8237_PORT_OP
{
  uint16_t Port;
  uint8_t Value;
  uint8_t Reserved;
} IT8888_8237_PORT_OP, *PIT8888_8237_PORT_OP;

typedef struct IT8888_8237_CHANNEL_SNAPSHOT
{
  uint16_t BaseAddr;
  uint16_t CurAddr;
  uint16_t BaseCount;
  uint16_t CurCount;
  uint8_t Page;
  uint8_t Mode;
  uint8_t Masked;
  uint8_t TerminalCount;
  uint32_t LegacyAddress;
  uint32_t ByteCount;
  uint8_t Direction;
  uint8_t Is16Bit;
  uint16_t Reserved;
} IT8888_8237_CHANNEL_SNAPSHOT, *PIT8888_8237_CHANNEL_SNAPSHOT;

typedef struct IT8888_8237_SNAPSHOT
{
  IT8888_8237_CHANNEL_SNAPSHOT Ch[8];
  uint8_t Command0;
  uint8_t Command1;
  uint8_t Status0;
  uint8_t Status1;
  uint8_t Mask0;
  uint8_t Mask1;
  uint8_t FlipFlop0;
  uint8_t FlipFlop1;
} IT8888_8237_SNAPSHOT, *PIT8888_8237_SNAPSHOT;

typedef struct IT8888_8237_PREPARE
{
  uint8_t Channel;
  uint8_t Direction;
  uint8_t Is16Bit;
  uint8_t Masked;
  uint32_t LegacyAddress;
  uint32_t ByteCount;
  uint8_t Mode;
  uint8_t Reserved[3];
} IT8888_8237_PREPARE, *PIT8888_8237_PREPARE;

typedef struct IT8888_DDMA_REQUEST
{
  uint8_t Channel;
  uint8_t Direction;
  uint16_t Reserved0;
  uint32_t BufferOffset;
  uint32_t Count;
  uint32_t Flags;
} IT8888_DDMA_REQUEST, *PIT8888_DDMA_REQUEST;

typedef struct IT8888_DDMA_STATUS
{
  uint8_t Armed;
  uint8_t Channel;
  uint8_t Direction;
  uint8_t StatusReg;
  uint8_t ModeReg;
  uint8_t LastCommand;
  uint16_t Base;
  uint32_t Count;
  uint32_t Flags;
  uint64_t LogicalAddress;
  uint32_t CompletionCount;
  uint32_t ErrorCount;
  uint32_t LastPciStatus;
} IT8888_DDMA_STATUS, *PIT8888_DDMA_STATUS;

typedef struct IT8888_IRQ_STATUS
{
  uint32_t IrqCount;
  uint32_t Pending;
  uint32_t LastVector;
  uint32_t LastStatus;
} IT8888_IRQ_STATUS, *PIT8888_IRQ_STATUS;

typedef struct IT8888_WAIT_IRQ_REQUEST
{
  uint32_t TimeoutMs;
  uint32_t Reserved;
} IT8888_WAIT_IRQ_REQUEST, *PIT8888_WAIT_IRQ_REQUEST;

typedef struct IT8888_TRACE_ENTRY
{
  uint64_t Sequence;
  uint32_t Type;
  uint32_t A;
  uint64_t B;
  uint64_t C;
  uint64_t Qpc;
} IT8888_TRACE_ENTRY, *PIT8888_TRACE_ENTRY;

typedef struct IT8888_TRACE_PACKET
{
  uint32_t Count;
  uint32_t Dropped;
  IT8888_TRACE_ENTRY Entries[IT8888_TRACE_MAX_USER];
} IT8888_TRACE_PACKET, *PIT8888_TRACE_PACKET;

#pragma pack(pop)

#define IT8888_TRACE_INFO 0u
#define IT8888_TRACE_CFG_READ 1u
#define IT8888_TRACE_CFG_WRITE 2u
#define IT8888_TRACE_IO_READ 3u
#define IT8888_TRACE_IO_WRITE 4u
#define IT8888_TRACE_DMA 5u
#define IT8888_TRACE_VDMA 6u
#define IT8888_TRACE_DDMA 7u
#define IT8888_TRACE_IRQ 8u
#define IT8888_TRACE_ERROR 9u

/* Backward-compatible aliases used by the driver implementation. */
#define IT8888_TRACE_PORT_READ IT8888_TRACE_IO_READ
#define IT8888_TRACE_PORT_WRITE IT8888_TRACE_IO_WRITE

#endif /* IT8888VDMA_PUBLIC_H */
