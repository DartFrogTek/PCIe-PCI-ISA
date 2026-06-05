#include "dos_it8888.h"
#include <stdio.h>
#include <string.h>

static IT8_STATUS ddma_base(IT8_CONTEXT *ctx, u8 ch, u16 *base) {
  if (!ctx || !base || ch > 7 || ch == 4)
    return IT8_BAD_PARAM;
  *base = ctx->DdmaBase[ch];
  if (*base == 0)
    return IT8_HW;
  return IT8_OK;
}

static u8 build_command(u8 dir) {
  switch (dir) {
  case IT8888_DIR_ISA_TO_RAM:
    return 0x04u;
  case IT8888_DIR_RAM_TO_ISA:
    return 0x08u;
  default:
    return 0x00u;
  }
}

static u8 build_mode(u8 channel, u8 dir) {
  u8 mode = channel & 3u;
  if (dir == IT8888_DIR_ISA_TO_RAM)
    mode |= DMA_TRANSFER_WRITE;
  else if (dir == IT8888_DIR_RAM_TO_ISA)
    mode |= DMA_TRANSFER_READ;
  mode |= 0x40u; /* single transfer */
  return mode;
}

static void fill_status(IT8_CONTEXT *ctx, IT8_DDMA_STATUS *s) {
  if (!s)
    return;
  memset(s, 0, sizeof(*s));
  s->Armed = ctx->ddma.Armed;
  s->Channel = ctx->ddma.Channel;
  s->Direction = ctx->ddma.Direction;
  s->LastCommand = ctx->ddma.LastCommand;
  s->Count = ctx->ddma.Count;
  s->Flags = ctx->ddma.Flags;
  s->LogicalAddress = ctx->ddma.Logical;
  s->Base = ctx->ddma.Base;
  s->StatusReg = ctx->ddma.StatusReg;
  s->ModeReg = ctx->ddma.ModeReg;
  s->CompletionCount = ctx->ddma.CompletionCount;
  s->ErrorCount = ctx->ddma.ErrorCount;
  s->LastPciStatus =
      (u16)(it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, 0x04, 4) >>
            16);
}

IT8_STATUS it8_ddma_arm(IT8_CONTEXT *ctx, const IT8_DDMA_REQUEST *req,
                        IT8_DDMA_STATUS *status) {
  u32 addr, count_minus_1;
  u16 base = 0;
  u8 cmd, mode;
  IT8_STATUS e;

  if (!ctx || !req || !status)
    return IT8_BAD_PARAM;
  if (!ctx->dma.ptr || ctx->dma.size == 0)
    return IT8_HW;
  if (req->Channel > 7 || req->Channel == 4)
    return IT8_BAD_PARAM;
  if (req->Direction > 2)
    return IT8_BAD_PARAM;
  if (req->Count == 0 || req->Count > 0x10000ul)
    return IT8_BAD_PARAM;
  if (req->BufferOffset > ctx->dma.size ||
      req->Count > ctx->dma.size - req->BufferOffset)
    return IT8_BAD_PARAM;

  addr = ctx->dma.phys + req->BufferOffset;
  /* addr is u32; range above 4 GiB is impossible in this build. */
  if (!(req->Flags & IT8888_DDMA_FLAG_ALLOW_32BIT) && addr >= 0x01000000ul)
    return IT8_RANGE;

  count_minus_1 = req->Count - 1;
  cmd = build_command(req->Direction);
  mode = build_mode(req->Channel, req->Direction);

  if (!(req->Flags & IT8888_DDMA_FLAG_NO_CFG_INIT)) {
    e = it8_apply_default_init(ctx);
    if (e != IT8_OK)
      return e;
  }
  e = ddma_base(ctx, req->Channel, &base);
  if (e != IT8_OK)
    return e;

  if (req->Flags & IT8888_DDMA_FLAG_MASTER_CLEAR)
    it8_out8((u16)(base + IT8888_DDMA_REG_MASTERCLR), 0x00);
  it8_out8((u16)(base + IT8888_DDMA_REG_ADDR0), (u8)(addr));
  it8_out8((u16)(base + IT8888_DDMA_REG_ADDR1), (u8)(addr >> 8));
  it8_out8((u16)(base + IT8888_DDMA_REG_ADDR2), (u8)(addr >> 16));
  it8_out8((u16)(base + IT8888_DDMA_REG_ADDR3), (u8)(addr >> 24));
  it8_out8((u16)(base + IT8888_DDMA_REG_COUNT0), (u8)(count_minus_1));
  it8_out8((u16)(base + IT8888_DDMA_REG_COUNT1), (u8)(count_minus_1 >> 8));
  it8_out8((u16)(base + IT8888_DDMA_REG_COMMAND), cmd);
  it8_out8((u16)(base + IT8888_DDMA_REG_MODE), mode);
  if (req->Flags & IT8888_DDMA_FLAG_UNMASK)
    it8_out8((u16)(base + IT8888_DDMA_REG_MASK), 0x00);

  memset(&ctx->ddma, 0, sizeof(ctx->ddma));
  ctx->ddma.Armed = 1;
  ctx->ddma.Channel = req->Channel;
  ctx->ddma.Direction = req->Direction;
  ctx->ddma.Count = req->Count;
  ctx->ddma.Flags = req->Flags;
  ctx->ddma.Logical = addr;
  ctx->ddma.Base = base;
  ctx->ddma.LastCommand = cmd;
  ctx->ddma.ModeReg = mode;
  fill_status(ctx, status);
  return IT8_OK;
}

IT8_STATUS it8_ddma_start(IT8_CONTEXT *ctx, IT8_DDMA_STATUS *st) {
  if (!ctx || !st)
    return IT8_BAD_PARAM;
  if (!ctx->ddma.Armed || ctx->ddma.Base == 0)
    return IT8_HW;
  it8_out8((u16)(ctx->ddma.Base + IT8888_DDMA_REG_MASK), 0x00);
  fill_status(ctx, st);
  return IT8_OK;
}

IT8_STATUS it8_ddma_poll(IT8_CONTEXT *ctx, IT8_DDMA_STATUS *st) {
  if (!ctx || !st)
    return IT8_BAD_PARAM;
  if (ctx->ddma.Armed && ctx->ddma.Base != 0) {
    ctx->ddma.StatusReg =
        it8_in8((u16)(ctx->ddma.Base + IT8888_DDMA_REG_COMMAND));
  }
  fill_status(ctx, st);
  return IT8_OK;
}

void it8_ddma_clear(IT8_CONTEXT *ctx) {
  if (!ctx)
    return;
  if (ctx->ddma.Base)
    it8_out8((u16)(ctx->ddma.Base + IT8888_DDMA_REG_MASTERCLR), 0x00);
  memset(&ctx->ddma, 0, sizeof(ctx->ddma));
}

void it8_ddma_status(IT8_CONTEXT *ctx, IT8_DDMA_STATUS *st) {
  if (ctx && st)
    fill_status(ctx, st);
}

IT8_STATUS it8_ddma_r8(IT8_CONTEXT *ctx, u8 channel, u8 off, u8 *value,
                       u16 *port) {
  u16 base;
  IT8_STATUS e = ddma_base(ctx, channel, &base);
  if (e != IT8_OK)
    return e;
  if (off > 0x0F)
    return IT8_BAD_PARAM;
  if (value)
    *value = it8_in8((u16)(base + off));
  if (port)
    *port = (u16)(base + off);
  return IT8_OK;
}

IT8_STATUS it8_ddma_w8(IT8_CONTEXT *ctx, u8 channel, u8 off, u8 value,
                       u16 *port) {
  u16 base;
  IT8_STATUS e = ddma_base(ctx, channel, &base);
  if (e != IT8_OK)
    return e;
  if (off > 0x0F)
    return IT8_BAD_PARAM;
  it8_out8((u16)(base + off), value);
  if (port)
    *port = (u16)(base + off);
  return IT8_OK;
}

void it8_print_ddma_status(const IT8_DDMA_STATUS *s) {
  if (!s)
    return;
  printf("armed=%u ch=%u dir=%u base=%04X mode=%02X cmd/status=%02X count=%lu "
         "phys=%08lX flags=%08lX pci_st=%04X\n",
         s->Armed, s->Channel, s->Direction, s->Base, s->ModeReg, s->StatusReg,
         s->Count, s->LogicalAddress, s->Flags, s->LastPciStatus);
}
