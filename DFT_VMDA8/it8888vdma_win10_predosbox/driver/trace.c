#include "it8888.h"

VOID It8888TraceInit(PDEVICE_CONTEXT ctx)
{
  WDF_OBJECT_ATTRIBUTES attrs;
  WDF_OBJECT_ATTRIBUTES_INIT(&attrs);
  attrs.ParentObject = ctx->Device;
  WdfSpinLockCreate(&attrs, &ctx->Trace.Lock);
  It8888TraceClear(ctx);
}

VOID It8888Trace(PDEVICE_CONTEXT ctx, ULONG type, ULONG a, ULONGLONG b,
                 ULONGLONG c)
{
  if (!ctx || !ctx->Trace.Lock)
    return;
  LARGE_INTEGER qpc = KeQueryPerformanceCounter(NULL);
  WdfSpinLockAcquire(ctx->Trace.Lock);
  ULONG idx = ctx->Trace.Head++ % IT8888_TRACE_RING_SIZE;
  IT8888_TRACE_ENTRY *e = &ctx->Trace.Entries[idx];
  e->Sequence = ++ctx->Trace.Seq;
  e->Qpc = (ULONGLONG)qpc.QuadPart;
  e->Type = type;
  e->A = a;
  e->B = b;
  e->C = c;
  if (ctx->Trace.Count < IT8888_TRACE_RING_SIZE)
    ctx->Trace.Count++;
  else
    ctx->Trace.Dropped++;
  WdfSpinLockRelease(ctx->Trace.Lock);
}

VOID It8888TraceClear(PDEVICE_CONTEXT ctx)
{
  if (!ctx)
    return;
  if (ctx->Trace.Lock)
    WdfSpinLockAcquire(ctx->Trace.Lock);
  RtlZeroMemory(ctx->Trace.Entries, sizeof(ctx->Trace.Entries));
  ctx->Trace.Head = ctx->Trace.Count = ctx->Trace.Dropped = 0;
  ctx->Trace.Seq = 0;
  if (ctx->Trace.Lock)
    WdfSpinLockRelease(ctx->Trace.Lock);
}

VOID It8888TraceGet(PDEVICE_CONTEXT ctx, PIT8888_TRACE_PACKET pkt)
{
  RtlZeroMemory(pkt, sizeof(*pkt));
  if (!ctx->Trace.Lock)
    return;
  WdfSpinLockAcquire(ctx->Trace.Lock);
  ULONG n = ctx->Trace.Count;
  if (n > IT8888_TRACE_MAX_USER)
    n = IT8888_TRACE_MAX_USER;
  ULONG start =
      (ctx->Trace.Head + IT8888_TRACE_RING_SIZE - n) % IT8888_TRACE_RING_SIZE;
  for (ULONG i = 0; i < n; i++)
    pkt->Entries[i] = ctx->Trace.Entries[(start + i) % IT8888_TRACE_RING_SIZE];
  pkt->Count = n;
  pkt->Dropped = ctx->Trace.Dropped;
  WdfSpinLockRelease(ctx->Trace.Lock);
}
