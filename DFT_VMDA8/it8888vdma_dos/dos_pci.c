#include "dos_it8888.h"
#include <stdio.h>
#include <string.h>

static u32 cfg_addr(u8 bus, u8 dev, u8 fn, u8 off) {
  return 0x80000000ul | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)fn << 8) |
         ((u32)off & 0xFCu);
}

u32 it8_pci_read(u8 bus, u8 dev, u8 fn, u8 off, u8 width) {
  u32 d;
  u8 shift = (u8)((off & 3u) * 8u);
  it8_out32(0xCF8, cfg_addr(bus, dev, fn, off));
  d = it8_in32(0xCFC);
  if (width == 1)
    return (d >> shift) & 0xFFu;
  if (width == 2)
    return (d >> shift) & 0xFFFFu;
  return d;
}

void it8_pci_write(u8 bus, u8 dev, u8 fn, u8 off, u8 width, u32 val) {
  u32 d, mask;
  u8 shift = (u8)((off & 3u) * 8u);
  it8_out32(0xCF8, cfg_addr(bus, dev, fn, off));
  d = it8_in32(0xCFC);
  if (width == 1) {
    mask = 0xFFul << shift;
    d = (d & ~mask) | ((val & 0xFFul) << shift);
  } else if (width == 2) {
    mask = 0xFFFFul << shift;
    d = (d & ~mask) | ((val & 0xFFFFul) << shift);
  } else {
    d = val;
  }
  it8_out32(0xCF8, cfg_addr(bus, dev, fn, off));
  it8_out32(0xCFC, d);
}

int it8_pci_find(u16 vendor, u16 device, IT8_PCI_DEV *out) {
  u16 v, d;
  unsigned int bus, dev, fn;
  for (bus = 0; bus < 256u; ++bus) {
    for (dev = 0; dev < 32u; ++dev) {
      for (fn = 0; fn < 8u; ++fn) {
        v = (u16)it8_pci_read((u8)bus, (u8)dev, (u8)fn, 0x00, 2);
        if (v == 0xFFFFu || v == 0x0000u) {
          if (fn == 0)
            break;
          continue;
        }
        d = (u16)it8_pci_read((u8)bus, (u8)dev, (u8)fn, 0x02, 2);
        if (v == vendor && d == device) {
          if (out) {
            out->bus = (u8)bus;
            out->dev = (u8)dev;
            out->fn = (u8)fn;
            out->vendor = v;
            out->device = d;
            out->revision = (u8)it8_pci_read(bus, dev, fn, 0x08, 1);
          }
          return 1;
        }
        if (fn == 0) {
          u8 hdr = (u8)it8_pci_read((u8)bus, (u8)dev, (u8)fn, 0x0E, 1);
          if ((hdr & 0x80u) == 0)
            break;
        }
      }
    }
  }
  return 0;
}

IT8_STATUS it8_enable_command_bits(IT8_CONTEXT *ctx) {
  u16 cmd;
  if (!ctx)
    return IT8_BAD_PARAM;
  cmd = (u16)it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, 0x04, 2);
  cmd |=
      PCI_COMMAND_IO_SPACE | PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
  it8_pci_write(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, 0x04, 2, cmd);
  return IT8_OK;
}

void it8_refresh_ddma_bases(IT8_CONTEXT *ctx) {
  u32 v40, v44, v48, v4c;
  if (!ctx)
    return;
  v40 =
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH01, 4);
  v44 =
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH23, 4);
  v48 =
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH45, 4);
  v4c =
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH67, 4);
  ctx->DdmaBase[0] = (u16)(v40 & 0xFFF0u);
  ctx->DdmaBase[1] = (u16)((v40 >> 16) & 0xFFF0u);
  ctx->DdmaBase[2] = (u16)(v44 & 0xFFF0u);
  ctx->DdmaBase[3] = (u16)((v44 >> 16) & 0xFFF0u);
  ctx->DdmaBase[4] = (u16)(v48 & 0xFFF0u);
  ctx->DdmaBase[5] = (u16)((v48 >> 16) & 0xFFF0u);
  ctx->DdmaBase[6] = (u16)(v4c & 0xFFF0u);
  ctx->DdmaBase[7] = (u16)((v4c >> 16) & 0xFFF0u);
}

IT8_STATUS it8_apply_default_init(IT8_CONTEXT *ctx) {
  if (!ctx)
    return IT8_BAD_PARAM;
  it8_enable_command_bits(ctx);

  /* Same values as the working Win10 path. Low nybble contains decode/control
   * bits. */
  it8_pci_write(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH01, 4,
                0x83918381ul);
  it8_pci_write(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH23, 4,
                0x83B183A1ul);
  it8_pci_write(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH45, 4,
                0x83D30000ul);
  it8_pci_write(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH67, 4,
                0x83F383E3ul);
  it8_pci_write(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_50, 4,
                0x01FFF023ul);
  it8_pci_write(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_54, 4,
                0x8C003F3Ful);
  it8_refresh_ddma_bases(ctx);
  return IT8_OK;
}

IT8_STATUS it8_bridge_set_iowin(u8 bus, u8 dev, u8 fn, u32 base, u32 limit) {
  u16 oldcmd;
  u8 newbase, newlimit;
  u16 newbaseupper, newlimitupper;
  if ((base & 0xFFFul) != 0)
    return IT8_BAD_PARAM;
  if ((limit & 0xFFFul) != 0xFFFul)
    return IT8_BAD_PARAM;
  if (base > limit || base > 0xFFFFul || limit > 0xFFFFul)
    return IT8_BAD_PARAM;

  oldcmd = (u16)it8_pci_read(bus, dev, fn, 0x04, 2);
  newbase = (u8)((base >> 8) & 0xF0u);
  newlimit = (u8)((limit >> 8) & 0xF0u);
  newbaseupper = (u16)((base >> 16) & 0xFFFFu);
  newlimitupper = (u16)((limit >> 16) & 0xFFFFu);

  it8_pci_write(bus, dev, fn, 0x04, 2, oldcmd & ~0x0001u);
  it8_pci_write(bus, dev, fn, 0x30, 2, newbaseupper);
  it8_pci_write(bus, dev, fn, 0x32, 2, newlimitupper);
  it8_pci_write(bus, dev, fn, 0x1C, 1, newbase);
  it8_pci_write(bus, dev, fn, 0x1D, 1, newlimit);
  it8_pci_write(bus, dev, fn, 0x04, 2, oldcmd | 0x0001u);
  return IT8_OK;
}

void it8_dump_cfg(const IT8_PCI_DEV *d) {
  int i;
  if (!d)
    return;
  printf("PCI %02X:%02X.%u vendor=%04X device=%04X rev=%02X\n", d->bus, d->dev,
         d->fn, d->vendor, d->device, d->revision);
  for (i = 0; i < 256; i += 16) {
    int j;
    printf("%02X: ", i);
    for (j = 0; j < 16; j += 4) {
      printf("%08lX ", it8_pci_read(d->bus, d->dev, d->fn, (u8)(i + j), 4));
    }
    printf("\n");
  }
}

void it8_zero_context(IT8_CONTEXT *ctx) {
  if (ctx)
    memset(ctx, 0, sizeof(*ctx));
}
