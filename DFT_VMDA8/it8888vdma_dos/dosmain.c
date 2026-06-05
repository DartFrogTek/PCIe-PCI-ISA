#include "dos_it8888.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
  puts("it8dos - IT8888 DOS bring-up utility");
  puts("usage:");
  puts("  it8dos info");
  puts("  it8dos init");
  puts("  it8dos dumpcfg");
  puts("  it8dos bridge <bus> <dev> <fn> <basehex> <limithex>   ; e.g. 00 1c "
       "00 8000 8fff");
  puts("  it8dos in <porthex> [1|2|4]");
  puts("  it8dos out <porthex> <valuehex> [1|2|4]");
  puts("  it8dos ddmaprobe <ch>");
  puts("  it8dos ddmafill <size> <valuehex>");
  puts("  it8dos ddmaarm <ch> <dir> <count> [flagshex]           ; dir 1 "
       "ISA->RAM, 2 RAM->ISA");
  puts("  it8dos ddmastart <ch> <dir> <count> [flagshex]");
  puts("notes:");
  puts("  DOS buffer is conventional memory; physical address is "
       "segment*16+offset.");
  puts("  Default DDMA init writes cfg40/44/48/4c/50/54 from the working Win10 "
       "path.");
}

static u32 hx(const char *s) { return strtoul(s, NULL, 16); }
static u32 num(const char *s) { return strtoul(s, NULL, 0); }

static int find_ctx(IT8_CONTEXT *ctx) {
  it8_zero_context(ctx);
  if (!it8_pci_find(IT8888_VENDOR_ID_DEFAULT, IT8888_DEVICE_ID_DEFAULT,
                    &ctx->pci)) {
    puts("IT8888 not found by vendor/device 1283:8888");
    return 0;
  }
  it8_refresh_ddma_bases(ctx);
  return 1;
}

static void print_info(IT8_CONTEXT *ctx) {
  int i;
  u32 cmd = it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, 0x04, 4);
  printf("IT8888 at %02X:%02X.%u rev %02X cmd/status=%08lX\n", ctx->pci.bus,
         ctx->pci.dev, ctx->pci.fn, ctx->pci.revision, cmd);
  printf(
      "cfg40=%08lX cfg44=%08lX cfg48=%08lX cfg4c=%08lX cfg50=%08lX "
      "cfg54=%08lX\n",
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH01, 4),
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH23, 4),
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH45, 4),
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_CH67, 4),
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_50, 4),
      it8_pci_read(ctx->pci.bus, ctx->pci.dev, ctx->pci.fn, IT8888_CFG_54, 4));
  for (i = 0; i < 8; ++i)
    printf("ch%d ddma base=%04X%s\n", i, ctx->DdmaBase[i],
           i == 4 ? " cascade/reserved" : "");
}

int main(int argc, char **argv) {
  IT8_CONTEXT ctx;
  if (argc < 2) {
    usage();
    return 1;
  }

  if (!strcmp(argv[1], "bridge")) {
    if (argc < 7) {
      usage();
      return 1;
    }
    if (it8_bridge_set_iowin((u8)hx(argv[2]), (u8)hx(argv[3]), (u8)hx(argv[4]),
                             hx(argv[5]), hx(argv[6])) != IT8_OK) {
      puts("bridge window failed");
      return 2;
    }
    puts("bridge I/O window programmed");
    return 0;
  }

  if (!find_ctx(&ctx))
    return 2;
  vdma8237_reset(&ctx);

  if (!strcmp(argv[1], "info")) {
    print_info(&ctx);
  } else if (!strcmp(argv[1], "init")) {
    if (it8_apply_default_init(&ctx) != IT8_OK) {
      puts("init failed");
      return 2;
    }
    print_info(&ctx);
  } else if (!strcmp(argv[1], "dumpcfg")) {
    it8_dump_cfg(&ctx.pci);
  } else if (!strcmp(argv[1], "in")) {
    u16 p;
    u8 w;
    if (argc < 3) {
      usage();
      return 1;
    }
    p = (u16)hx(argv[2]);
    w = (argc >= 4) ? (u8)num(argv[3]) : 1;
    if (w == 1)
      printf("%02X\n", it8_in8(p));
    else if (w == 2)
      printf("%04X\n", it8_in16(p));
    else if (w == 4)
      printf("%08lX\n", it8_in32(p));
    else {
      puts("bad width");
      return 1;
    }
  } else if (!strcmp(argv[1], "out")) {
    u16 p;
    u32 v;
    u8 w;
    if (argc < 4) {
      usage();
      return 1;
    }
    p = (u16)hx(argv[2]);
    v = hx(argv[3]);
    w = (argc >= 5) ? (u8)num(argv[4]) : 1;
    if (w == 1)
      it8_out8(p, (u8)v);
    else if (w == 2)
      it8_out16(p, (u16)v);
    else if (w == 4)
      it8_out32(p, v);
    else {
      puts("bad width");
      return 1;
    }
  } else if (!strcmp(argv[1], "ddmaprobe")) {
    u8 ch, i, v;
    u16 port;
    if (argc < 3) {
      usage();
      return 1;
    }
    it8_apply_default_init(&ctx);
    ch = (u8)num(argv[2]);
    for (i = 0; i < 16; ++i) {
      if (it8_ddma_r8(&ctx, ch, i, &v, &port) != IT8_OK) {
        puts("probe failed");
        return 2;
      }
      printf("%04X+%X = %02X\n", ctx.DdmaBase[ch], i, v);
    }
  } else if (!strcmp(argv[1], "ddmafill")) {
    u32 size;
    u8 val;
    if (argc < 4) {
      usage();
      return 1;
    }
    size = num(argv[2]);
    val = (u8)hx(argv[3]);
    if (it8_dma_alloc(&ctx, size) != IT8_OK) {
      puts("dma alloc failed");
      return 2;
    }
    it8_dma_fill(&ctx, 0, size, val);
    printf("DMA buffer phys=%08lX size=%lu filled=%02X\n", ctx.dma.phys,
           ctx.dma.size, val);
    it8_dma_dump(&ctx, 0, size > 128 ? 128 : size);
    it8_dma_free(&ctx);
  } else if (!strcmp(argv[1], "ddmaarm") || !strcmp(argv[1], "ddmastart")) {
    IT8_DDMA_REQUEST r;
    IT8_DDMA_STATUS st;
    u32 size;
    if (argc < 5) {
      usage();
      return 1;
    }
    memset(&r, 0, sizeof(r));
    r.Channel = (u8)num(argv[2]);
    r.Direction = (u8)num(argv[3]);
    r.Count = num(argv[4]);
    r.Flags = (argc >= 6)
                  ? hx(argv[5])
                  : (IT8888_DDMA_FLAG_MASTER_CLEAR | IT8888_DDMA_FLAG_UNMASK);
    r.BufferOffset = 0;
    size = r.Count;
    if (it8_dma_alloc(&ctx, size) != IT8_OK) {
      puts("dma alloc failed");
      return 2;
    }
    it8_dma_fill(&ctx, 0, size, 0x00);
    if (it8_ddma_arm(&ctx, &r, &st) != IT8_OK) {
      puts("ddma arm failed");
      it8_dma_free(&ctx);
      return 2;
    }
    it8_print_ddma_status(&st);
    if (!strcmp(argv[1], "ddmastart")) {
      if (it8_ddma_start(&ctx, &st) != IT8_OK) {
        puts("ddma start failed");
        it8_dma_free(&ctx);
        return 2;
      }
      it8_print_ddma_status(&st);
      it8_ddma_poll(&ctx, &st);
      it8_print_ddma_status(&st);
    }
    puts("Leaving DOS buffer allocated only for command lifetime; for real "
         "device transfer, program card before exit.");
    it8_dma_free(&ctx);
  } else {
    usage();
    return 1;
  }
  return 0;
}
