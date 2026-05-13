#include "it8888.h"

NTSTATUS It8888DmaAllocate(PDEVICE_CONTEXT ctx, ULONG size,
                           PIT8888_DMA_INFO outInfo)
{
  if (!ctx->DmaEnabler)
    return STATUS_DEVICE_NOT_READY;
  if (size == 0 || size > 0x1000000)
    return STATUS_INVALID_PARAMETER;
  It8888DmaFree(ctx);
  NTSTATUS st = WdfCommonBufferCreate(
      ctx->DmaEnabler, size, WDF_NO_OBJECT_ATTRIBUTES, &ctx->Dma.CommonBuffer);
  if (!NT_SUCCESS(st))
    return st;
  ctx->Dma.Va = WdfCommonBufferGetAlignedVirtualAddress(ctx->Dma.CommonBuffer);
  ctx->Dma.Logical =
      WdfCommonBufferGetAlignedLogicalAddress(ctx->Dma.CommonBuffer);
  ctx->Dma.Size = size;
  ctx->Dma.BufferId = 1;
  RtlZeroMemory(ctx->Dma.Va, size);
  It8888Trace(ctx, IT8888_TRACE_DMA, size, ctx->Dma.Logical.QuadPart,
              (ULONGLONG)(ULONG_PTR)ctx->Dma.Va);
  return It8888DmaInfo(ctx, outInfo);
}

VOID It8888DmaFree(PDEVICE_CONTEXT ctx)
{
  if (ctx->Dma.CommonBuffer)
  {
    WdfObjectDelete(ctx->Dma.CommonBuffer);
    RtlZeroMemory(&ctx->Dma, sizeof(ctx->Dma));
    It8888Trace(ctx, IT8888_TRACE_DMA, 0xFFFF0001, 0, 0);
  }
}

NTSTATUS It8888DmaInfo(PDEVICE_CONTEXT ctx, PIT8888_DMA_INFO outInfo)
{
  if (!ctx->Dma.CommonBuffer)
    return STATUS_NOT_FOUND;
  RtlZeroMemory(outInfo, sizeof(*outInfo));
  outInfo->BufferId = ctx->Dma.BufferId;
  outInfo->Size = (ULONG)ctx->Dma.Size;
  outInfo->LogicalAddress = ctx->Dma.Logical.QuadPart;
  outInfo->KernelVaForDebug = (ULONGLONG)(ULONG_PTR)ctx->Dma.Va;
  return STATUS_SUCCESS;
}

static NTSTATUS It8888DmaBounds(PDEVICE_CONTEXT ctx, ULONG offset, ULONG count)
{
    if (!ctx->Dma.CommonBuffer || !ctx->Dma.Va) return STATUS_NOT_FOUND;
    if (count == 0) return STATUS_INVALID_PARAMETER;
    if (offset > ctx->Dma.Size) return STATUS_INVALID_PARAMETER;
    if (count > (ctx->Dma.Size - offset)) return STATUS_INVALID_PARAMETER;
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

