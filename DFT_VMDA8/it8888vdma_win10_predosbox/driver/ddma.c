#include "it8888.h"

static NTSTATUS DdmaBase(PDEVICE_CONTEXT ctx, UCHAR ch, PUSHORT base) {
  if (ch > 7 || ch == 4)
    return STATUS_INVALID_PARAMETER;
  *base = ctx->DdmaBase[ch];
  if (*base == 0)
    return STATUS_DEVICE_CONFIGURATION_ERROR;
  return STATUS_SUCCESS;
}
static UCHAR BuildCommand(UCHAR dir) {
  switch (dir) {
  case IT8888_DIR_ISA_TO_RAM:
    return 0x04; // 8237 write/device->memory-ish
  case IT8888_DIR_RAM_TO_ISA:
    return 0x08; // 8237 read/memory->device-ish
  default:
    return 0x00;
  }
}
static UCHAR BuildMode(UCHAR channel, UCHAR dir) {
  UCHAR mode = channel & 3;
  if (dir == IT8888_DIR_ISA_TO_RAM)
    mode |= DMA_TRANSFER_WRITE;
  else if (dir == IT8888_DIR_RAM_TO_ISA)
    mode |= DMA_TRANSFER_READ;
  mode |= 0x40; // single transfer as conservative default
  return mode;
}
static NTSTATUS WriteDdma8(PDEVICE_CONTEXT ctx, USHORT base, UCHAR off,
                           UCHAR val) {
  return It8888PortWrite(ctx, (USHORT)(base + off), 1, val);
}
static NTSTATUS ReadDdma8(PDEVICE_CONTEXT ctx, USHORT base, UCHAR off,
                          PUCHAR val) {
  ULONG v;
  NTSTATUS st = It8888PortRead(ctx, (USHORT)(base + off), 1, &v);
  *val = (UCHAR)v;
  return st;
}

VOID It8888DdmaStatus(PDEVICE_CONTEXT ctx, PIT8888_DDMA_STATUS s) {
  RtlZeroMemory(s, sizeof(*s));
  s->Armed = ctx->Ddma.Armed;
  s->Channel = ctx->Ddma.Channel;
  s->Direction = ctx->Ddma.Direction;
  s->LastCommand = ctx->Ddma.LastCommand;
  s->Count = ctx->Ddma.Count;
  s->Flags = ctx->Ddma.Flags;
  s->LogicalAddress = ctx->Ddma.Logical.QuadPart;
  s->Base = ctx->Ddma.Base;
  s->StatusReg = ctx->Ddma.StatusReg;
  s->ModeReg = ctx->Ddma.ModeReg;
  s->CompletionCount = ctx->Ddma.CompletionCount;
  s->ErrorCount = ctx->Ddma.ErrorCount;
  ULONG cs = 0;
  if (NT_SUCCESS(It8888PciRead(ctx, 0x04, 4, &cs)))
    s->LastPciStatus = cs >> 16;
}

NTSTATUS It8888DdmaArm(PDEVICE_CONTEXT ctx, PIT8888_DDMA_REQUEST req,
                       PIT8888_DDMA_STATUS status) {
  if (!ctx->Dma.CommonBuffer)
    return STATUS_DEVICE_NOT_READY;
  if (req->Channel > 7 || req->Channel == 4)
    return STATUS_INVALID_PARAMETER;
  if (req->Count == 0 || req->BufferOffset + req->Count > ctx->Dma.Size)
    return STATUS_INVALID_PARAMETER;
  USHORT base;
  NTSTATUS st = DdmaBase(ctx, req->Channel, &base);
  if (!NT_SUCCESS(st))
    return st;
  PHYSICAL_ADDRESS addr;
  addr.QuadPart = ctx->Dma.Logical.QuadPart + req->BufferOffset;
  ULONG countMinus1 = req->Count - 1;
  UCHAR cmd = BuildCommand(req->Direction);
  UCHAR mode = BuildMode(req->Channel, req->Direction);

  WdfWaitLockAcquire(ctx->HwLock, NULL);
  if (!(req->Flags & IT8888_DDMA_FLAG_NO_CFG_INIT))
    It8888ApplyDefaultInit(ctx);
  if (req->Flags & IT8888_DDMA_FLAG_DRY_RUN)
    goto record;

  if (req->Flags & IT8888_DDMA_FLAG_MASTER_CLEAR)
    WriteDdma8(ctx, base, IT8888_DDMA_REG_MASTERCLR, 0x00);
  WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR0, (UCHAR)(addr.QuadPart));
  WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR1, (UCHAR)(addr.QuadPart >> 8));
  WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR2, (UCHAR)(addr.QuadPart >> 16));
  WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR3, (UCHAR)(addr.QuadPart >> 24));
  WriteDdma8(ctx, base, IT8888_DDMA_REG_COUNT0, (UCHAR)(countMinus1));
  WriteDdma8(ctx, base, IT8888_DDMA_REG_COUNT1, (UCHAR)(countMinus1 >> 8));
  WriteDdma8(ctx, base, IT8888_DDMA_REG_COMMAND, cmd);
  WriteDdma8(ctx, base, IT8888_DDMA_REG_MODE, mode);
  if (req->Flags & IT8888_DDMA_FLAG_UNMASK)
    WriteDdma8(ctx, base, IT8888_DDMA_REG_MASK, 0x00);
  if (req->Flags & IT8888_DDMA_FLAG_SOFT_REQUEST)
    WriteDdma8(ctx, base, IT8888_DDMA_REG_REQUEST, 0x04 | (req->Channel & 3));

record:
  ctx->Ddma.Armed = 1;
  ctx->Ddma.Channel = req->Channel;
  ctx->Ddma.Direction = req->Direction;
  ctx->Ddma.Count = req->Count;
  ctx->Ddma.Flags = req->Flags;
  ctx->Ddma.Logical = addr;
  ctx->Ddma.Base = base;
  ctx->Ddma.LastCommand = cmd;
  ctx->Ddma.ModeReg = mode;
  ReadDdma8(ctx, base, IT8888_DDMA_REG_COMMAND, &ctx->Ddma.StatusReg);
  It8888Trace(ctx, IT8888_TRACE_DDMA,
              req->Channel | ((ULONG)req->Direction << 8), addr.QuadPart,
              req->Count);
  if (req->Flags & IT8888_DDMA_FLAG_POLL_AFTER) {
    IT8888_DDMA_STATUS tmp;
    It8888DdmaPoll(ctx, &tmp);
  }
  It8888DdmaStatus(ctx, status);
  WdfWaitLockRelease(ctx->HwLock);
  return STATUS_SUCCESS;
}

NTSTATUS It8888DdmaStart(PDEVICE_CONTEXT ctx, PIT8888_DDMA_STATUS status) {
  if (!ctx->Ddma.Armed)
    return STATUS_DEVICE_NOT_READY;
  if (ctx->Ddma.Flags & IT8888_DDMA_FLAG_SOFT_REQUEST) {
    WriteDdma8(ctx, ctx->Ddma.Base, IT8888_DDMA_REG_REQUEST,
               0x04 | (ctx->Ddma.Channel & 3));
  }
  return It8888DdmaPoll(ctx, status);
}

NTSTATUS It8888DdmaPoll(PDEVICE_CONTEXT ctx, PIT8888_DDMA_STATUS status) {
  if (!ctx->Ddma.Armed)
    return STATUS_DEVICE_NOT_READY;
  UCHAR s = 0, m = 0;
  ReadDdma8(ctx, ctx->Ddma.Base, IT8888_DDMA_REG_COMMAND, &s);
  ReadDdma8(ctx, ctx->Ddma.Base, IT8888_DDMA_REG_MODE, &m);
  ctx->Ddma.StatusReg = s;
  ctx->Ddma.ModeReg = m;
  ULONG cs = 0;
  It8888PciRead(ctx, 0x04, 4, &cs);
  if (cs & 0xF9000000u)
    ctx->Ddma.ErrorCount++;
  It8888DdmaStatus(ctx, status);
  It8888Trace(ctx, IT8888_TRACE_DDMA, 0x80000000u | ctx->Ddma.Channel, s, cs);
  return STATUS_SUCCESS;
}

VOID It8888DdmaClear(PDEVICE_CONTEXT ctx) {
  if (ctx->Ddma.Base)
    WriteDdma8(ctx, ctx->Ddma.Base, IT8888_DDMA_REG_MASTERCLR, 0x00);
  RtlZeroMemory(&ctx->Ddma, sizeof(ctx->Ddma));
  It8888Trace(ctx, IT8888_TRACE_DDMA, 0xFFFF0001, 0, 0);
}

NTSTATUS It8888PanicReset(PDEVICE_CONTEXT ctx) {
  for (int i = 0; i < 8; i++)
    if (ctx->DdmaBase[i])
      WriteDdma8(ctx, ctx->DdmaBase[i], IT8888_DDMA_REG_MASTERCLR, 0x00);
  It8888DdmaClear(ctx);
  Vdma8237Reset(ctx);
  It8888ClearErrors(ctx);
  return STATUS_SUCCESS;
}
