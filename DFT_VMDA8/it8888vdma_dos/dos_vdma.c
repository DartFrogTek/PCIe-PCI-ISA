#include "dos_it8888.h"
#include <string.h>

static u8 channel_from_mask_port(u8 val) { return val & 3u; }
static u8 mask_bit(u8 val) { return (val & 4u) ? 1u : 0u; }

u8 vdma8237_direction(IT8_CONTEXT *ctx, u8 ch) {
  u8 mode;
  if (!ctx || ch > 7)
    return IT8888_DIR_VERIFY;
  mode = ctx->vdma.Ch[ch].Mode;
  switch (mode & DMA_MODE_TRANSFER_MASK) {
  case DMA_TRANSFER_READ:
    return IT8888_DIR_RAM_TO_ISA;
  case DMA_TRANSFER_WRITE:
    return IT8888_DIR_ISA_TO_RAM;
  default:
    return IT8888_DIR_VERIFY;
  }
}

u32 vdma8237_legacy_addr(IT8_CONTEXT *ctx, u8 ch) {
  u32 a;
  if (!ctx || ch > 7)
    return 0;
  a = ((u32)ctx->vdma.Ch[ch].Page << 16) | ctx->vdma.Ch[ch].CurAddr;
  if (ch >= 5 && ch <= 7)
    a <<= 1;
  return a;
}

u32 vdma8237_byte_count(IT8_CONTEXT *ctx, u8 ch) {
  u32 c;
  if (!ctx || ch > 7)
    return 0;
  c = (u32)ctx->vdma.Ch[ch].CurCount + 1;
  if (ch >= 5 && ch <= 7)
    c <<= 1;
  return c;
}

static u8 *page_reg(IT8_CONTEXT *ctx, u16 port) {
  switch (port) {
  case 0x87:
    return &ctx->vdma.Ch[0].Page;
  case 0x83:
    return &ctx->vdma.Ch[1].Page;
  case 0x81:
    return &ctx->vdma.Ch[2].Page;
  case 0x82:
    return &ctx->vdma.Ch[3].Page;
  case 0x8B:
    return &ctx->vdma.Ch[5].Page;
  case 0x89:
    return &ctx->vdma.Ch[6].Page;
  case 0x8A:
    return &ctx->vdma.Ch[7].Page;
  default:
    return NULL;
  }
}

void vdma8237_reset(IT8_CONTEXT *ctx) {
  int i;
  if (!ctx)
    return;
  memset(&ctx->vdma, 0, sizeof(ctx->vdma));
  for (i = 0; i < 8; ++i)
    ctx->vdma.Ch[i].Masked = 1;
  ctx->vdma.Mask0 = 0x0F;
  ctx->vdma.Mask1 = 0x0E;
}

IT8_STATUS vdma8237_out(IT8_CONTEXT *ctx, u16 port, u8 value) {
  VDMA8237_STATE *s;
  u8 *pg;
  if (!ctx)
    return IT8_BAD_PARAM;
  s = &ctx->vdma;
  pg = page_reg(ctx, port);
  if (pg) {
    *pg = value;
    return IT8_OK;
  }

  if (port <= 0x0F) {
    u8 ch = (u8)(port / 2);
    if (port <= 0x07) {
      if (port & 1) {
        if (!s->FlipFlop0)
          s->Ch[ch].BaseCount = (s->Ch[ch].BaseCount & 0xFF00u) | value;
        else {
          s->Ch[ch].BaseCount =
              (s->Ch[ch].BaseCount & 0x00FFu) | ((u16)value << 8);
          s->Ch[ch].CurCount = s->Ch[ch].BaseCount;
        }
        s->FlipFlop0 ^= 1;
      } else {
        if (!s->FlipFlop0)
          s->Ch[ch].BaseAddr = (s->Ch[ch].BaseAddr & 0xFF00u) | value;
        else {
          s->Ch[ch].BaseAddr =
              (s->Ch[ch].BaseAddr & 0x00FFu) | ((u16)value << 8);
          s->Ch[ch].CurAddr = s->Ch[ch].BaseAddr;
        }
        s->FlipFlop0 ^= 1;
      }
    } else
      switch (port) {
      case 0x0A: {
        u8 c = channel_from_mask_port(value);
        s->Ch[c].Masked = mask_bit(value);
        if (s->Ch[c].Masked)
          s->Mask0 |= (1u << c);
        else
          s->Mask0 &= ~(1u << c);
        break;
      }
      case 0x0B: {
        u8 c = value & 3u;
        s->Ch[c].Mode = value;
        break;
      }
      case 0x0C:
        s->FlipFlop0 = 0;
        break;
      case 0x0D:
        vdma8237_reset(ctx);
        break;
      case 0x0E: {
        int i;
        for (i = 0; i < 4; ++i)
          s->Ch[i].Masked = 0;
        s->Mask0 = 0;
        break;
      }
      case 0x0F: {
        int i;
        for (i = 0; i < 4; ++i)
          s->Ch[i].Masked = (value >> i) & 1u;
        s->Mask0 = value & 0x0F;
        break;
      }
      }
    return IT8_OK;
  }

  if (port >= 0xC0 && port <= 0xDF) {
    u8 ch = (u8)(4 + ((port - 0xC0) / 4));
    u16 rel = (u16)(port - 0xC0);
    if (ch > 7)
      return IT8_BAD_PARAM;
    if ((rel % 8) == 0) {
      if (!s->FlipFlop1)
        s->Ch[ch].BaseAddr = (s->Ch[ch].BaseAddr & 0xFF00u) | value;
      else {
        s->Ch[ch].BaseAddr = (s->Ch[ch].BaseAddr & 0x00FFu) | ((u16)value << 8);
        s->Ch[ch].CurAddr = s->Ch[ch].BaseAddr;
      }
      s->FlipFlop1 ^= 1;
    } else if ((rel % 8) == 2) {
      if (!s->FlipFlop1)
        s->Ch[ch].BaseCount = (s->Ch[ch].BaseCount & 0xFF00u) | value;
      else {
        s->Ch[ch].BaseCount =
            (s->Ch[ch].BaseCount & 0x00FFu) | ((u16)value << 8);
        s->Ch[ch].CurCount = s->Ch[ch].BaseCount;
      }
      s->FlipFlop1 ^= 1;
    } else
      switch (port) {
      case 0xD4: {
        u8 c = (u8)(4 + channel_from_mask_port(value));
        s->Ch[c].Masked = mask_bit(value);
        if (s->Ch[c].Masked)
          s->Mask1 |= (1u << (c - 4));
        else
          s->Mask1 &= ~(1u << (c - 4));
        break;
      }
      case 0xD6: {
        u8 c = (u8)(4 + (value & 3u));
        if (c < 8)
          s->Ch[c].Mode = value;
        break;
      }
      case 0xD8:
        s->FlipFlop1 = 0;
        break;
      case 0xDA:
        vdma8237_reset(ctx);
        break;
      case 0xDC: {
        int i;
        for (i = 5; i < 8; ++i)
          s->Ch[i].Masked = 0;
        s->Mask1 &= ~0x0Eu;
        break;
      }
      case 0xDE: {
        int i;
        for (i = 5; i < 8; ++i)
          s->Ch[i].Masked = (value >> (i - 4)) & 1u;
        s->Mask1 = value & 0x0F;
        break;
      }
      }
    return IT8_OK;
  }
  return IT8_BAD_PARAM;
}

IT8_STATUS vdma8237_in(IT8_CONTEXT *ctx, u16 port, u8 *value) {
  u8 *pg;
  if (!ctx || !value)
    return IT8_BAD_PARAM;
  pg = page_reg(ctx, port);
  if (pg) {
    *value = *pg;
    return IT8_OK;
  }
  *value = 0xFFu;
  if (port == 0x08) {
    *value = ctx->vdma.Status0;
    ctx->vdma.Status0 = 0;
  } else if (port == 0xD0) {
    *value = ctx->vdma.Status1;
    ctx->vdma.Status1 = 0;
  } else if (port == 0x0F)
    *value = ctx->vdma.Mask0;
  else if (port == 0xDE)
    *value = ctx->vdma.Mask1;
  return IT8_OK;
}
