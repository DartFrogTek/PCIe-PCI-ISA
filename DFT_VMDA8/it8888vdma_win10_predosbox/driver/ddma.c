#include "it8888.h"

static NTSTATUS DdmaBase(PDEVICE_CONTEXT ctx, UCHAR ch, PUSHORT base)
{
  if (ch > 7 || ch == 4)
    return STATUS_INVALID_PARAMETER;
  *base = ctx->DdmaBase[ch];
  if (*base == 0)
    return STATUS_DEVICE_CONFIGURATION_ERROR;
  return STATUS_SUCCESS;
}
static UCHAR BuildCommand(UCHAR dir)
{
  switch (dir)
  {
  case IT8888_DIR_ISA_TO_RAM:
    return 0x04; // 8237 write/device->memory-ish
  case IT8888_DIR_RAM_TO_ISA:
    return 0x08; // 8237 read/memory->device-ish
  default:
    return 0x00;
  }
}
static UCHAR BuildMode(UCHAR channel, UCHAR dir)
{
  UCHAR mode = channel & 3;
  if (dir == IT8888_DIR_ISA_TO_RAM)
    mode |= DMA_TRANSFER_WRITE;
  else if (dir == IT8888_DIR_RAM_TO_ISA)
    mode |= DMA_TRANSFER_READ;
  mode |= 0x40; // single transfer as conservative default
  return mode;
}
static NTSTATUS WriteDdma8(PDEVICE_CONTEXT ctx, USHORT base, UCHAR off,
                           UCHAR val)
{
  return It8888PortWrite(ctx, (USHORT)(base + off), 1, val);
}
static NTSTATUS ReadDdma8(PDEVICE_CONTEXT ctx, USHORT base, UCHAR off,
                          PUCHAR val)
{
  ULONG v;
  NTSTATUS st = It8888PortRead(ctx, (USHORT)(base + off), 1, &v);
  *val = (UCHAR)v;
  return st;
}

VOID It8888DdmaStatus(PDEVICE_CONTEXT ctx, PIT8888_DDMA_STATUS s)
{
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


static VOID DdmaFillCachedStatus(PDEVICE_CONTEXT ctx, PIT8888_DDMA_STATUS status)
{
    RtlZeroMemory(status, sizeof(*status));
    status->Armed = ctx->Ddma.Armed;
    status->Channel = ctx->Ddma.Channel;
    status->Direction = ctx->Ddma.Direction;
    status->LastCommand = ctx->Ddma.LastCommand;
    status->Count = ctx->Ddma.Count;
    status->Flags = ctx->Ddma.Flags;
    status->LogicalAddress = ctx->Ddma.Logical.QuadPart;
    status->Base = ctx->Ddma.Base;
    status->StatusReg = ctx->Ddma.StatusReg;
    status->ModeReg = ctx->Ddma.ModeReg;
    status->CompletionCount = ctx->Ddma.CompletionCount;
    status->ErrorCount = ctx->Ddma.ErrorCount;
    status->LastPciStatus = 0; /* cached-only path: no post-arm PCI config read */
}
NTSTATUS It8888DdmaArm(PDEVICE_CONTEXT ctx, PIT8888_DDMA_REQUEST req, PIT8888_DDMA_STATUS status)
{
    NTSTATUS st = STATUS_SUCCESS;
    PHYSICAL_ADDRESS addr;
    ULONG countMinus1;
    USHORT base = 0;
    UCHAR cmd = 0x00;
    UCHAR mode;

#define TRY_DDMA(x) do { st = (x); if (!NT_SUCCESS(st)) goto out_unlock; } while (0)

    /*
       Accept both allocation backends:
         - WDF common buffer: CommonBuffer != NULL, Va != NULL
         - low ISA/8237 bring-up buffer: CommonBuffer == NULL, Va != NULL
       The DDMA programming path only needs the bus/logical physical address,
       size, and CPU VA for fill/dump helpers.
    */
    if (!ctx->Dma.Va || ctx->Dma.Size == 0)
        return STATUS_DEVICE_NOT_READY;

    if (!req || !status)
        return STATUS_INVALID_PARAMETER;

    if (req->Channel > 7 || req->Channel == 4)
        return STATUS_INVALID_PARAMETER;

    if (req->Direction > 2)
        return STATUS_INVALID_PARAMETER;

    if (req->Count == 0 || req->Count > 0x10000)
        return STATUS_INVALID_PARAMETER;

    if (req->BufferOffset > ctx->Dma.Size)
        return STATUS_INVALID_PARAMETER;

    if (req->Count > (ctx->Dma.Size - req->BufferOffset))
        return STATUS_INVALID_PARAMETER;

    addr.QuadPart = ctx->Dma.Logical.QuadPart + req->BufferOffset;
    countMinus1 = req->Count - 1;
    mode = BuildMode(req->Channel, req->Direction);

    if (req->Flags & IT8888_DDMA_FLAG_DRY_RUN) {
        RtlZeroMemory(&ctx->Ddma, sizeof(ctx->Ddma));
        ctx->Ddma.Armed = 1;
        ctx->Ddma.Channel = req->Channel;
        ctx->Ddma.Direction = req->Direction;
        ctx->Ddma.Count = req->Count;
        ctx->Ddma.Flags = req->Flags;
        ctx->Ddma.Logical = addr;
        ctx->Ddma.Base = ctx->DdmaBase[req->Channel];
        ctx->Ddma.LastCommand = cmd;
        ctx->Ddma.ModeReg = mode;
        ctx->Ddma.StatusReg = 0;
        It8888Trace(ctx, IT8888_TRACE_DDMA,
                    0xD0000000u | req->Channel | ((ULONG)req->Direction << 8),
                    addr.QuadPart, req->Count);
        DdmaFillCachedStatus(ctx, status);
        return STATUS_SUCCESS;
    }

    if (addr.QuadPart > 0xffffffffULL)
        return STATUS_INVALID_PARAMETER;

    if (!(req->Flags & IT8888_DDMA_FLAG_ALLOW_32BIT) &&
        addr.QuadPart >= 0x01000000ULL)
        return STATUS_INVALID_PARAMETER;

    if (!(req->Flags & IT8888_DDMA_FLAG_NO_CFG_INIT)) {
        st = It8888ApplyDefaultInit(ctx);
        if (!NT_SUCCESS(st))
            return st;
    }

    st = DdmaBase(ctx, req->Channel, &base);
    if (!NT_SUCCESS(st))
        return st;

    WdfWaitLockAcquire(ctx->HwLock, NULL);

    if (req->Flags & IT8888_DDMA_FLAG_MASTER_CLEAR)
        TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_MASTERCLR, 0x00));

    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR0,  (UCHAR)(addr.QuadPart)));
    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR1,  (UCHAR)(addr.QuadPart >> 8)));
    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR2,  (UCHAR)(addr.QuadPart >> 16)));
    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_ADDR3,  (UCHAR)(addr.QuadPart >> 24)));
    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_COUNT0, (UCHAR)(countMinus1)));
    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_COUNT1, (UCHAR)(countMinus1 >> 8)));
    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_COMMAND, cmd));
    TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_MODE, mode));

    if (req->Flags & IT8888_DDMA_FLAG_UNMASK)
        TRY_DDMA(WriteDdma8(ctx, base, IT8888_DDMA_REG_MASK, 0x00));

    RtlZeroMemory(&ctx->Ddma, sizeof(ctx->Ddma));
    ctx->Ddma.Armed = 1;
    ctx->Ddma.Channel = req->Channel;
    ctx->Ddma.Direction = req->Direction;
    ctx->Ddma.Count = req->Count;
    ctx->Ddma.Flags = req->Flags;
    ctx->Ddma.Logical = addr;
    ctx->Ddma.Base = base;
    ctx->Ddma.LastCommand = cmd;
    ctx->Ddma.ModeReg = mode;
    ctx->Ddma.StatusReg = 0;

    It8888Trace(ctx, IT8888_TRACE_DDMA,
                req->Channel | ((ULONG)req->Direction << 8),
                addr.QuadPart, req->Count);

out_unlock:
    if (!NT_SUCCESS(st)) {
        ctx->Ddma.ErrorCount++;
        It8888Trace(ctx, IT8888_TRACE_ERROR, 0xDD000001u | req->Channel, (ULONGLONG)st, 0);
    }
    DdmaFillCachedStatus(ctx, status);
    WdfWaitLockRelease(ctx->HwLock);
#undef TRY_DDMA
    return st;
}

NTSTATUS It8888DdmaStart(PDEVICE_CONTEXT ctx, PIT8888_DDMA_STATUS status)
{
    NTSTATUS st = STATUS_SUCCESS;

    if (!status)
        return STATUS_INVALID_PARAMETER;

    WdfWaitLockAcquire(ctx->HwLock, NULL);

    if (!ctx->Ddma.Armed || ctx->Ddma.Base == 0) {
        st = STATUS_DEVICE_NOT_READY;
        goto out_unlock;
    }

    st = WriteDdma8(ctx, ctx->Ddma.Base, IT8888_DDMA_REG_MASK, 0x00);
    if (!NT_SUCCESS(st)) {
        ctx->Ddma.ErrorCount++;
        It8888Trace(ctx, IT8888_TRACE_ERROR, 0xDD000002u | ctx->Ddma.Channel, (ULONGLONG)st, 0);
        goto out_unlock;
    }

    It8888Trace(ctx, IT8888_TRACE_DDMA,
                0x51000000u | ctx->Ddma.Channel | ((ULONG)ctx->Ddma.Direction << 8),
                ctx->Ddma.Logical.QuadPart, ctx->Ddma.Count);

out_unlock:
    DdmaFillCachedStatus(ctx, status);
    WdfWaitLockRelease(ctx->HwLock);
    return st;
}

NTSTATUS It8888DdmaPoll(PDEVICE_CONTEXT ctx, PIT8888_DDMA_STATUS status)
{
    NTSTATUS st = STATUS_SUCCESS;
    UCHAR s = 0;

    if (!status)
        return STATUS_INVALID_PARAMETER;

    WdfWaitLockAcquire(ctx->HwLock, NULL);

    if (ctx->Ddma.Armed && ctx->Ddma.Base != 0) {
        /*
           Only +08 is valid as a status read. Do not read +0B; mode reads
           are undefined on the 8237/DDMA map. ModeReg is our cached write.
        */
        st = ReadDdma8(ctx, ctx->Ddma.Base, IT8888_DDMA_REG_COMMAND, &s);
        if (NT_SUCCESS(st)) {
            ctx->Ddma.StatusReg = s;
        } else {
            ctx->Ddma.ErrorCount++;
            It8888Trace(ctx, IT8888_TRACE_ERROR, 0xDD000003u | ctx->Ddma.Channel, (ULONGLONG)st, 0);
        }
    }

    DdmaFillCachedStatus(ctx, status);

    WdfWaitLockRelease(ctx->HwLock);
    return st;
}

VOID It8888DdmaClear(PDEVICE_CONTEXT ctx)
{
  if (ctx->Ddma.Base)
    WriteDdma8(ctx, ctx->Ddma.Base, IT8888_DDMA_REG_MASTERCLR, 0x00);
  RtlZeroMemory(&ctx->Ddma, sizeof(ctx->Ddma));
  It8888Trace(ctx, IT8888_TRACE_DDMA, 0xFFFF0001, 0, 0);
}

NTSTATUS It8888PanicReset(PDEVICE_CONTEXT ctx)
{
  for (int i = 0; i < 8; i++)
    if (ctx->DdmaBase[i])
      WriteDdma8(ctx, ctx->DdmaBase[i], IT8888_DDMA_REG_MASTERCLR, 0x00);
  It8888DdmaClear(ctx);
  Vdma8237Reset(ctx);
  It8888ClearErrors(ctx);
  return STATUS_SUCCESS;
}

NTSTATUS It8888DdmaRead8Raw(PDEVICE_CONTEXT ctx, PIT8888_DDMA_REG8 op)
{
    if (op->Offset > 0x0F)
        return STATUS_INVALID_PARAMETER;

    USHORT base;
    NTSTATUS st = DdmaBase(ctx, op->Channel, &base);
    if (!NT_SUCCESS(st))
        return st;

    UCHAR value = 0;
    WdfWaitLockAcquire(ctx->HwLock, NULL);
    st = ReadDdma8(ctx, base, op->Offset, &value);
    WdfWaitLockRelease(ctx->HwLock);

    if (!NT_SUCCESS(st))
        return st;

    op->Value = value;
    op->Base = base;
    op->Port = (USHORT)(base + op->Offset);

    It8888Trace(ctx, IT8888_TRACE_DDMA, 0x52000000u | op->Channel | ((ULONG)op->Offset << 8), value, op->Port);
    return STATUS_SUCCESS;
}

NTSTATUS It8888DdmaWrite8Raw(PDEVICE_CONTEXT ctx, PIT8888_DDMA_REG8 op)
{
    if (op->Offset > 0x0F)
        return STATUS_INVALID_PARAMETER;

    USHORT base;
    NTSTATUS st = DdmaBase(ctx, op->Channel, &base);
    if (!NT_SUCCESS(st))
        return st;

    WdfWaitLockAcquire(ctx->HwLock, NULL);
    st = WriteDdma8(ctx, base, op->Offset, op->Value);
    WdfWaitLockRelease(ctx->HwLock);

    if (!NT_SUCCESS(st))
        return st;

    op->Base = base;
    op->Port = (USHORT)(base + op->Offset);

    It8888Trace(ctx, IT8888_TRACE_DDMA, 0x57000000u | op->Channel | ((ULONG)op->Offset << 8), op->Value, op->Port);
    return STATUS_SUCCESS;
}

NTSTATUS It8888DdmaProbe(PDEVICE_CONTEXT ctx, PIT8888_DDMA_PROBE probe)
{
    USHORT base;
    NTSTATUS st = DdmaBase(ctx, probe->Channel, &base);
    if (!NT_SUCCESS(st))
        return st;

    probe->Base = base;
    probe->FirstPort = base;

    UCHAR count = probe->Count;
    if (count == 0 || count > 16)
        count = 16;
    probe->Count = count;

    WdfWaitLockAcquire(ctx->HwLock, NULL);
    for (UCHAR i = 0; i < count; ++i) {
        UCHAR v = 0;
        st = ReadDdma8(ctx, base, i, &v);
        if (!NT_SUCCESS(st)) {
            WdfWaitLockRelease(ctx->HwLock);
            return st;
        }
        probe->Values[i] = v;
    }
    WdfWaitLockRelease(ctx->HwLock);

    It8888Trace(ctx, IT8888_TRACE_DDMA, 0x50524F42u, base, count);
    return STATUS_SUCCESS;
}




