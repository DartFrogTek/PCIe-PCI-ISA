#ifndef DOS_IT8888_H
#define DOS_IT8888_H

/*
   IT8888 DOS bring-up utility/library.

   Target: real-mode DOS, Open Watcom C large model preferred.
   This is a direct DOS port of the Win10 IT8888 VDMA/DDMA bring-up logic:
     - no KMDF/WDM/WDF common buffers
     - no IOCTLs
     - direct PCI config through PCI BIOS when available, CF8/CFC fallback
     - direct in/out port access
     - DMA buffer is ordinary DOS conventional memory, so physical addr < 1 MiB

   Build, Open Watcom:
     wcl -ml -0 -bt=dos -fe=it8dos.exe dosmain.c dos_pci.c dos_io.c dos_dma.c
   dos_ddma.c dos_vdma.c
*/

#include <stddef.h>

#if defined(__WATCOMC__)
#include <conio.h>
#include <dos.h>
#include <i86.h>
#define IT8_FAR __far
#else
#define IT8_FAR
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

typedef enum IT8_STATUS {
  IT8_OK = 0,
  IT8_ERR = 1,
  IT8_BAD_PARAM = 2,
  IT8_NOT_FOUND = 3,
  IT8_NO_MEMORY = 4,
  IT8_RANGE = 5,
  IT8_HW = 6
} IT8_STATUS;

#define IT8888_VENDOR_ID_DEFAULT 0x1283u
#define IT8888_DEVICE_ID_DEFAULT 0x8888u

#define PCI_COMMAND_IO_SPACE 0x0001u
#define PCI_COMMAND_MEMORY_SPACE 0x0002u
#define PCI_COMMAND_BUS_MASTER 0x0004u

#define IT8888_CFG_CH01 0x40u
#define IT8888_CFG_CH23 0x44u
#define IT8888_CFG_CH45 0x48u
#define IT8888_CFG_CH67 0x4Cu
#define IT8888_CFG_50 0x50u
#define IT8888_CFG_54 0x54u

#define IT8888_DDMA_REG_ADDR0 0x0u
#define IT8888_DDMA_REG_ADDR1 0x1u
#define IT8888_DDMA_REG_ADDR2 0x2u
#define IT8888_DDMA_REG_ADDR3 0x3u
#define IT8888_DDMA_REG_COUNT0 0x4u
#define IT8888_DDMA_REG_COUNT1 0x5u
#define IT8888_DDMA_REG_COMMAND 0x8u
#define IT8888_DDMA_REG_REQUEST 0x9u
#define IT8888_DDMA_REG_MODE 0xBu
#define IT8888_DDMA_REG_MASTERCLR 0xDu
#define IT8888_DDMA_REG_MASK 0xFu

#define DMA_MODE_CHANNEL_MASK 0x03u
#define DMA_MODE_TRANSFER_MASK 0x0Cu
#define DMA_MODE_AUTO_INIT 0x10u
#define DMA_MODE_ADDRESS_DEC 0x20u
#define DMA_MODE_TYPE_MASK 0xC0u
#define DMA_TRANSFER_VERIFY 0x00u
#define DMA_TRANSFER_WRITE 0x04u
#define DMA_TRANSFER_READ 0x08u

#define IT8888_DIR_VERIFY 0u
#define IT8888_DIR_ISA_TO_RAM 1u
#define IT8888_DIR_RAM_TO_ISA 2u

#define IT8888_DDMA_FLAG_DRY_RUN 0x00000001ul
#define IT8888_DDMA_FLAG_MASTER_CLEAR 0x00000040ul
#define IT8888_DDMA_FLAG_UNMASK 0x00000080ul
#define IT8888_DDMA_FLAG_SOFT_REQUEST 0x00000100ul
#define IT8888_DDMA_FLAG_NO_CFG_INIT 0x00000200ul
#define IT8888_DDMA_FLAG_POLL_AFTER 0x00000400ul
#define IT8888_DDMA_FLAG_ALLOW_32BIT 0x00001000ul
#define IT8888_DDMA_FLAG_NO_READBACK_ARM 0x00002000ul

typedef struct IT8_PCI_DEV {
  u8 bus;
  u8 dev;
  u8 fn;
  u16 vendor;
  u16 device;
  u8 revision;
} IT8_PCI_DEV;

typedef struct IT8_DMA_BUFFER {
  void IT8_FAR *ptr;
  u32 phys;
  u32 size;
} IT8_DMA_BUFFER;

typedef struct VDMA8237_CHANNEL {
  u16 BaseAddr;
  u16 CurAddr;
  u16 BaseCount;
  u16 CurCount;
  u8 Page;
  u8 Mode;
  u8 Masked;
  u8 TerminalCount;
} VDMA8237_CHANNEL;

typedef struct VDMA8237_STATE {
  VDMA8237_CHANNEL Ch[8];
  u8 Command0, Command1;
  u8 Status0, Status1;
  u8 Mask0, Mask1;
  u8 FlipFlop0, FlipFlop1;
} VDMA8237_STATE;

typedef struct IT8_DDMA_STATE {
  u8 Armed;
  u8 Channel;
  u8 Direction;
  u8 LastCommand;
  u32 Count;
  u32 Flags;
  u32 Logical;
  u16 Base;
  u8 StatusReg;
  u8 ModeReg;
  u32 CompletionCount;
  u32 ErrorCount;
} IT8_DDMA_STATE;

typedef struct IT8_CONTEXT {
  IT8_PCI_DEV pci;
  u16 DdmaBase[8];
  IT8_DMA_BUFFER dma;
  VDMA8237_STATE vdma;
  IT8_DDMA_STATE ddma;
} IT8_CONTEXT;

typedef struct IT8_DDMA_REQUEST {
  u8 Channel;
  u8 Direction;
  u32 BufferOffset;
  u32 Count;
  u32 Flags;
} IT8_DDMA_REQUEST;

typedef struct IT8_DDMA_STATUS {
  u8 Armed;
  u8 Channel;
  u8 Direction;
  u8 StatusReg;
  u8 ModeReg;
  u8 LastCommand;
  u16 Base;
  u32 Count;
  u32 Flags;
  u32 LogicalAddress;
  u32 CompletionCount;
  u32 ErrorCount;
  u16 LastPciStatus;
} IT8_DDMA_STATUS;

/* DOS I/O */
u8 it8_in8(u16 port);
u16 it8_in16(u16 port);
u32 it8_in32(u16 port);
void it8_out8(u16 port, u8 v);
void it8_out16(u16 port, u16 v);
void it8_out32(u16 port, u32 v);

/* PCI config */
int it8_pci_find(u16 vendor, u16 device, IT8_PCI_DEV *out);
u32 it8_pci_read(u8 bus, u8 dev, u8 fn, u8 off, u8 width);
void it8_pci_write(u8 bus, u8 dev, u8 fn, u8 off, u8 width, u32 val);
IT8_STATUS it8_enable_command_bits(IT8_CONTEXT *ctx);
IT8_STATUS it8_apply_default_init(IT8_CONTEXT *ctx);
void it8_refresh_ddma_bases(IT8_CONTEXT *ctx);
IT8_STATUS it8_bridge_set_iowin(u8 bus, u8 dev, u8 fn, u32 base, u32 limit);
void it8_dump_cfg(const IT8_PCI_DEV *d);

/* DMA buffer */
IT8_STATUS it8_dma_alloc(IT8_CONTEXT *ctx, u32 size);
void it8_dma_free(IT8_CONTEXT *ctx);
void it8_dma_fill(IT8_CONTEXT *ctx, u32 off, u32 count, u8 val);
void it8_dma_write_bytes(IT8_CONTEXT *ctx, u32 off, const u8 *data, u32 count);
void it8_dma_dump(IT8_CONTEXT *ctx, u32 off, u32 count);

/* DDMA */
IT8_STATUS it8_ddma_arm(IT8_CONTEXT *ctx, const IT8_DDMA_REQUEST *req,
                        IT8_DDMA_STATUS *st);
IT8_STATUS it8_ddma_start(IT8_CONTEXT *ctx, IT8_DDMA_STATUS *st);
IT8_STATUS it8_ddma_poll(IT8_CONTEXT *ctx, IT8_DDMA_STATUS *st);
void it8_ddma_clear(IT8_CONTEXT *ctx);
void it8_ddma_status(IT8_CONTEXT *ctx, IT8_DDMA_STATUS *st);
IT8_STATUS it8_ddma_r8(IT8_CONTEXT *ctx, u8 channel, u8 off, u8 *value,
                       u16 *port);
IT8_STATUS it8_ddma_w8(IT8_CONTEXT *ctx, u8 channel, u8 off, u8 value,
                       u16 *port);

/* virtual 8237 capture */
void vdma8237_reset(IT8_CONTEXT *ctx);
IT8_STATUS vdma8237_out(IT8_CONTEXT *ctx, u16 port, u8 value);
IT8_STATUS vdma8237_in(IT8_CONTEXT *ctx, u16 port, u8 *value);
u32 vdma8237_legacy_addr(IT8_CONTEXT *ctx, u8 ch);
u32 vdma8237_byte_count(IT8_CONTEXT *ctx, u8 ch);
u8 vdma8237_direction(IT8_CONTEXT *ctx, u8 ch);

/* helpers */
void it8_zero_context(IT8_CONTEXT *ctx);
void it8_print_ddma_status(const IT8_DDMA_STATUS *s);

#endif
