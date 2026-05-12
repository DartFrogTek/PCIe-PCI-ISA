#include "it8888.h"

NTSTATUS It8888DmaAllocate(PDEVICE_CONTEXT ctx, ULONG size,
                           PIT8888_DMA_INFO outInfo) {
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

VOID It8888DmaFree(PDEVICE_CONTEXT ctx) {
  if (ctx->Dma.CommonBuffer) {
    WdfObjectDelete(ctx->Dma.CommonBuffer);
    RtlZeroMemory(&ctx->Dma, sizeof(ctx->Dma));
    It8888Trace(ctx, IT8888_TRACE_DMA, 0xFFFF0001, 0, 0);
  }
}

NTSTATUS It8888DmaInfo(PDEVICE_CONTEXT ctx, PIT8888_DMA_INFO outInfo) {
  if (!ctx->Dma.CommonBuffer)
    return STATUS_NOT_FOUND;
  RtlZeroMemory(outInfo, sizeof(*outInfo));
  outInfo->BufferId = ctx->Dma.BufferId;
  outInfo->Size = (ULONG)ctx->Dma.Size;
  outInfo->LogicalAddress = ctx->Dma.Logical.QuadPart;
  outInfo->KernelVaForDebug = (ULONGLONG)(ULONG_PTR)ctx->Dma.Va;
  return STATUS_SUCCESS;
}
