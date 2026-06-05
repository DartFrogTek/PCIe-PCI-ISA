#include <stdio.h>
#include <stdlib.h>
#if defined(__WATCOMC__)
#include <malloc.h>
#endif
#include "dos_it8888.h"
#include <string.h>

static u32 far_phys(void IT8_FAR *p) {
#if defined(__WATCOMC__)
  return ((u32)FP_SEG(p) << 4) + (u32)FP_OFF(p);
#else
  return 0;
#endif
}

IT8_STATUS it8_dma_alloc(IT8_CONTEXT *ctx, u32 size) {
#if defined(__WATCOMC__)
  void IT8_FAR *p;
  if (!ctx || size == 0 || size > 0x10000ul)
    return IT8_BAD_PARAM;
  it8_dma_free(ctx);
  p = _fmalloc((size_t)size);
  if (!p)
    return IT8_NO_MEMORY;
  _fmemset(p, 0, (size_t)size);
  ctx->dma.ptr = p;
  ctx->dma.phys = far_phys(p);
  ctx->dma.size = size;
  if (ctx->dma.phys >= 0x01000000ul) {
    it8_dma_free(ctx);
    return IT8_RANGE;
  }
  return IT8_OK;
#else
  (void)ctx;
  (void)size;
  return IT8_NO_MEMORY;
#endif
}

void it8_dma_free(IT8_CONTEXT *ctx) {
#if defined(__WATCOMC__)
  if (ctx && ctx->dma.ptr)
    _ffree(ctx->dma.ptr);
#endif
  if (ctx)
    memset(&ctx->dma, 0, sizeof(ctx->dma));
}

void it8_dma_fill(IT8_CONTEXT *ctx, u32 off, u32 count, u8 val) {
#if defined(__WATCOMC__)
  if (!ctx || !ctx->dma.ptr || off > ctx->dma.size ||
      count > ctx->dma.size - off)
    return;
  _fmemset((u8 IT8_FAR *)ctx->dma.ptr + off, val, (size_t)count);
#else
  (void)ctx;
  (void)off;
  (void)count;
  (void)val;
#endif
}

void it8_dma_write_bytes(IT8_CONTEXT *ctx, u32 off, const u8 *data, u32 count) {
#if defined(__WATCOMC__)
  if (!ctx || !ctx->dma.ptr || !data || off > ctx->dma.size ||
      count > ctx->dma.size - off)
    return;
  _fmemcpy((u8 IT8_FAR *)ctx->dma.ptr + off, data, (size_t)count);
#else
  (void)ctx;
  (void)off;
  (void)data;
  (void)count;
#endif
}

void it8_dma_dump(IT8_CONTEXT *ctx, u32 off, u32 count) {
#if defined(__WATCOMC__)
  u32 i;
  u8 IT8_FAR *p;
  if (!ctx || !ctx->dma.ptr || off > ctx->dma.size ||
      count > ctx->dma.size - off)
    return;
  p = (u8 IT8_FAR *)ctx->dma.ptr + off;
  for (i = 0; i < count; ++i) {
    if ((i & 15) == 0)
      printf("\n%05lX: ", off + i);
    printf("%02X ", p[i]);
  }
  printf("\n");
#else
  (void)ctx;
  (void)off;
  (void)count;
#endif
}
