#include "it8888.h"

#ifndef IT8888_DMA_ALLOC_FLAG_BELOW_16M
#define IT8888_DMA_ALLOC_FLAG_BELOW_16M 0x00000001u
#endif

static VOID It8888DmaFreeStorage(PDEVICE_CONTEXT ctx)
{
  if (ctx->Dma.LowContiguous)
  {
    if (ctx->Dma.Va)
      MmFreeContiguousMemory(ctx->Dma.Va);
  }
  else
  {
    if (ctx->Dma.CommonBuffer)
      WdfObjectDelete(ctx->Dma.CommonBuffer);
  }

  RtlZeroMemory(&ctx->Dma, sizeof(ctx->Dma));
}

static NTSTATUS It8888DmaFillInfoFromContext(PDEVICE_CONTEXT ctx,
                                             PIT8888_DMA_INFO outInfo)
{
  if (!outInfo)
    return STATUS_INVALID_PARAMETER;

  if (!ctx->Dma.Va || ctx->Dma.Size == 0)
    return STATUS_NOT_FOUND;

  RtlZeroMemory(outInfo, sizeof(*outInfo));
  outInfo->BufferId = ctx->Dma.BufferId;
  outInfo->Size = (ULONG)ctx->Dma.Size;
  outInfo->LogicalAddress = ctx->Dma.Logical.QuadPart;
  outInfo->KernelVaForDebug = (ULONGLONG)(ULONG_PTR)ctx->Dma.Va;
  return STATUS_SUCCESS;
}

static NTSTATUS It8888DmaAllocateLow16M(PDEVICE_CONTEXT ctx, ULONG size)
{
  PHYSICAL_ADDRESS low;
  PHYSICAL_ADDRESS high;
  PHYSICAL_ADDRESS boundary;
  PHYSICAL_ADDRESS pa;
  PVOID va;

  low.QuadPart = 0;
  high.QuadPart = 0x00ffffffULL;
  boundary.QuadPart = 0;

  va = MmAllocateContiguousMemorySpecifyCache(size, low, high, boundary, MmCached);
  if (!va)
    return STATUS_INSUFFICIENT_RESOURCES;

  pa = MmGetPhysicalAddress(va);
  if (pa.QuadPart > 0x00ffffffULL)
  {
    MmFreeContiguousMemory(va);
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  RtlZeroMemory(va, size);

  ctx->Dma.CommonBuffer = NULL;
  ctx->Dma.Va = va;
  ctx->Dma.Logical = pa;
  ctx->Dma.Size = size;
  ctx->Dma.BufferId = 1;
  ctx->Dma.LowContiguous = TRUE;

  return STATUS_SUCCESS;
}

static NTSTATUS It8888DmaAllocateWdfCommon(PDEVICE_CONTEXT ctx, ULONG size)
{
  NTSTATUS st;

  if (!ctx->DmaEnabler)
    return STATUS_DEVICE_NOT_READY;

  st = WdfCommonBufferCreate(ctx->DmaEnabler, size,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &ctx->Dma.CommonBuffer);
  if (!NT_SUCCESS(st))
    return st;

  ctx->Dma.Va = WdfCommonBufferGetAlignedVirtualAddress(ctx->Dma.CommonBuffer);
  ctx->Dma.Logical =
      WdfCommonBufferGetAlignedLogicalAddress(ctx->Dma.CommonBuffer);
  ctx->Dma.Size = size;
  ctx->Dma.BufferId = 1;
  ctx->Dma.LowContiguous = FALSE;

  RtlZeroMemory(ctx->Dma.Va, size);
  return STATUS_SUCCESS;
}

NTSTATUS It8888DmaAllocate(PDEVICE_CONTEXT ctx, ULONG size, ULONG flags,
                           PIT8888_DMA_INFO outInfo)
{
  NTSTATUS st;

  if (size == 0 || size > 0x1000000)
    return STATUS_INVALID_PARAMETER;

  It8888DmaFree(ctx);

  if (flags & IT8888_DMA_ALLOC_FLAG_BELOW_16M)
    st = It8888DmaAllocateLow16M(ctx, size);
  else
    st = It8888DmaAllocateWdfCommon(ctx, size);

  if (!NT_SUCCESS(st))
    return st;

  It8888Trace(ctx, IT8888_TRACE_DMA,
              (flags & IT8888_DMA_ALLOC_FLAG_BELOW_16M) ? 0x4C4F574Du : size,
              ctx->Dma.Logical.QuadPart,
              (ULONGLONG)(ULONG_PTR)ctx->Dma.Va);

  return It8888DmaInfo(ctx, outInfo);
}

VOID It8888DmaFree(PDEVICE_CONTEXT ctx)
{
  if (ctx->Dma.Va || ctx->Dma.CommonBuffer)
  {
    It8888DmaFreeStorage(ctx);
    It8888Trace(ctx, IT8888_TRACE_DMA, 0xFFFF0001, 0, 0);
  }
}

NTSTATUS It8888DmaInfo(PDEVICE_CONTEXT ctx, PIT8888_DMA_INFO outInfo)
{
  return It8888DmaFillInfoFromContext(ctx, outInfo);
}

static NTSTATUS It8888DmaBounds(PDEVICE_CONTEXT ctx, ULONG offset, ULONG count)
{
  if (!ctx->Dma.Va || ctx->Dma.Size == 0)
    return STATUS_NOT_FOUND;
  if (count == 0)
    return STATUS_INVALID_PARAMETER;
  if (offset > ctx->Dma.Size)
    return STATUS_INVALID_PARAMETER;
  if (count > (ctx->Dma.Size - offset))
    return STATUS_INVALID_PARAMETER;
  return STATUS_SUCCESS;
}
NTSTATUS It8888DmaFill(PDEVICE_CONTEXT ctx, PIT8888_DMA_MEMOP op)
{
    NTSTATUS st = It8888DmaBounds(ctx, op->Offset, op->Count);
    if (!NT_SUCCESS(st)) return st;
    RtlFillMemory((PUCHAR)ctx->Dma.Va + op->Offset, op->Count, op->Value);
    It8888Trace(ctx, IT8888_TRACE_DMA, 0xF1110000u | op->Value, ctx->Dma.Logical.QuadPart + op->Offset, op->Count);
    return STATUS_SUCCESS;
}

NTSTATUS It8888DmaWrite(PDEVICE_CONTEXT ctx, PIT8888_DMA_WRITE wr)
{
    if (!wr)
        return STATUS_INVALID_PARAMETER;
    if (wr->Count == 0 || wr->Count > IT8888_DMA_WRITE_MAX)
        return STATUS_INVALID_PARAMETER;

    NTSTATUS st = It8888DmaBounds(ctx, wr->Offset, wr->Count);
    if (!NT_SUCCESS(st))
        return st;

    RtlCopyMemory((PUCHAR)ctx->Dma.Va + wr->Offset, wr->Data, wr->Count);

    It8888Trace(ctx,
                IT8888_TRACE_DMA,
                0x57524954u, /* WRIT */
                ctx->Dma.Logical.QuadPart + wr->Offset,
                wr->Count);
    return STATUS_SUCCESS;
}
NTSTATUS It8888DmaDump(PDEVICE_CONTEXT ctx, PIT8888_DMA_DUMP dump)
{
    ULONG count = dump->Count;
    if (count > IT8888_DMA_DUMP_MAX) return STATUS_BUFFER_TOO_SMALL;
    NTSTATUS st = It8888DmaBounds(ctx, dump->Offset, count);
    if (!NT_SUCCESS(st)) return st;
    RtlCopyMemory(dump->Data, (PUCHAR)ctx->Dma.Va + dump->Offset, count);
    dump->Count = count;
    It8888Trace(ctx, IT8888_TRACE_DMA, 0xD00D0000u, ctx->Dma.Logical.QuadPart + dump->Offset, count);
    return STATUS_SUCCESS;
}

NTSTATUS It8888DmaCheck(PDEVICE_CONTEXT ctx, PIT8888_DMA_CHECK chk)
{
    NTSTATUS st = It8888DmaBounds(ctx, chk->Offset, chk->Count);
    if (!NT_SUCCESS(st)) return st;
    PUCHAR p = (PUCHAR)ctx->Dma.Va + chk->Offset;
    ULONG mismatches = 0;
    ULONG first = 0xFFFFFFFFu;
    UCHAR firstActual = 0;
    for (ULONG i = 0; i < chk->Count; ++i) {
        if (p[i] != chk->Expected) {
            if (mismatches == 0) { first = chk->Offset + i; firstActual = p[i]; }
            ++mismatches;
        }
    }
    chk->MismatchCount = mismatches;
    chk->FirstMismatchOffset = first;
    chk->FirstActual = firstActual;
    It8888Trace(ctx, IT8888_TRACE_DMA, 0xC0DE0000u | chk->Expected, mismatches, first);
    return STATUS_SUCCESS;
}



