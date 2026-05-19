#define WIN32_LEAN_AND_MEAN
#include "public.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <math.h>

static uint32_t u32(const char *s) { return (uint32_t)strtoul(s, NULL, 0); }
static HANDLE open_dev(void) {
  HANDLE h = CreateFileA(IT8888_DEVICE_DOS_NAME, GENERIC_READ | GENERIC_WRITE,
                         0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    fprintf(stderr, "open %s failed: %lu\n", IT8888_DEVICE_DOS_NAME,
            GetLastError());
  return h;
}
static BOOL ioctl(HANDLE h, DWORD code, void *in, DWORD inb, void *out,
                  DWORD outb, DWORD *ret) {
  DWORD r = 0;
  BOOL ok = DeviceIoControl(h, code, in, inb, out, outb, &r, NULL);
  if (ret)
    *ret = r;
  if (!ok)
    fprintf(stderr, "ioctl 0x%08lx failed: %lu\n", code, GetLastError());
  return ok;
}
static void print_info(IT8888_INFO *i) {
  printf("IT8888 info\n vendor:device %04x:%04x rev %02x started %u\n",
         i->VendorId, i->DeviceId, i->RevisionId, i->Started);
  printf(" cmd/status 0x%08x pci_status 0x%04x\n", i->PciCommandStatus,
         i->LastPciStatus);
  printf(" cfg40=%08x cfg44=%08x cfg48=%08x cfg4c=%08x cfg50=%08x cfg54=%08x\n",
         i->Cfg40, i->Cfg44, i->Cfg48, i->Cfg4C, i->Cfg50, i->Cfg54);
  printf(" ddma bases: ");
  for (int n = 0; n < 8; n++)
    printf("ch%d=%04x ", n, i->DdmaBase[n]);
  printf("\n");
  printf(" dma logical=0x%llx size=%u irq_count=%u\n",
         (unsigned long long)i->DmaLogicalAddress, i->DmaSize, i->IrqCount);
}
static int cmd_info(HANDLE h) {
  IT8888_INFO i;
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_GET_INFO, NULL, 0, &i, sizeof(i), &r))
    return 1;
  print_info(&i);
  return 0;
}
static int cmd_dumpcfg(HANDLE h) {
  for (int off = 0; off < 256; off += 16) {
    printf("%02x: ", off);
    for (int j = 0; j < 16; j += 4) {
      IT8888_CFG_ACCESS a = {(uint16_t)(off + j), 4, 0, 0};
      DWORD r;
      if (!ioctl(h, IOCTL_IT8888_CFG_READ, &a, sizeof(a), &a, sizeof(a), &r))
        return 1;
      printf("%08x ", a.Value);
    }
    printf("\n");
  }
  return 0;
}
static int cmd_cfgread(HANDLE h, int ac, char **av) {
  if (ac < 4) {
    puts("cfgread <off> <width>");
    return 2;
  }
  IT8888_CFG_ACCESS a = {(uint16_t)u32(av[2]), (uint8_t)u32(av[3]), 0, 0};
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_CFG_READ, &a, sizeof(a), &a, sizeof(a), &r))
    return 1;
  printf("cfg[%x/%u]=0x%x\n", a.Offset, a.Width, a.Value);
  return 0;
}
static int cmd_cfgwrite(HANDLE h, int ac, char **av) {
  if (ac < 5) {
    puts("cfgwrite <off> <width> <value>");
    return 2;
  }
  IT8888_CFG_ACCESS a = {(uint16_t)u32(av[2]), (uint8_t)u32(av[3]), 0,
                         u32(av[4])};
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_CFG_WRITE, &a, sizeof(a), NULL, 0, &r))
    return 1;
  return 0;
}
static int cmd_in(HANDLE h, int ac, char **av) {
  if (ac < 4) {
    puts("in <port> <width>");
    return 2;
  }
  IT8888_PORT_ACCESS a = {(uint16_t)u32(av[2]), (uint8_t)u32(av[3]), 0, 0};
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_PORT_READ, &a, sizeof(a), &a, sizeof(a), &r))
    return 1;
  printf("in[%x/%u]=0x%x\n", a.Port, a.Width, a.Value);
  return 0;
}
static int cmd_out(HANDLE h, int ac, char **av) {
  if (ac < 5) {
    puts("out <port> <width> <value>");
    return 2;
  }
  IT8888_PORT_ACCESS a = {(uint16_t)u32(av[2]), (uint8_t)u32(av[3]), 0,
                          u32(av[4])};
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_PORT_WRITE, &a, sizeof(a), NULL, 0, &r))
    return 1;
  return 0;
}
static int cmd_init(HANDLE h) {
  DWORD r;
  return ioctl(h, IOCTL_IT8888_INIT_DEFAULT, NULL, 0, NULL, 0, &r) ? 0 : 1;
}
static int cmd_dma_alloc(HANDLE h, int ac, char **av) {
  if (ac < 3) {
    puts("dma-alloc <bytes>");
    return 2;
  }
  IT8888_DMA_ALLOC_REQUEST in = {u32(av[2]), 0};
  IT8888_DMA_INFO out;
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_DMA_ALLOC, &in, sizeof(in), &out, sizeof(out), &r))
    return 1;
  printf("buf %u size %u logical 0x%llx kva 0x%llx\n", out.BufferId, out.Size,
         (unsigned long long)out.LogicalAddress,
         (unsigned long long)out.KernelVaForDebug);
  return 0;
}
static int cmd_dma_info(HANDLE h) {
  IT8888_DMA_INFO out;
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_DMA_INFO, NULL, 0, &out, sizeof(out), &r))
    return 1;
  printf("buf %u size %u logical 0x%llx kva 0x%llx\n", out.BufferId, out.Size,
         (unsigned long long)out.LogicalAddress,
         (unsigned long long)out.KernelVaForDebug);
  return 0;
}

static int cmd_dma_fill(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("dma-fill <offset> <count> <byte>");
        return 2;
    }

    IT8888_DMA_MEMOP op;
    memset(&op, 0, sizeof(op));
    op.Offset = u32(av[2]);
    op.Count = u32(av[3]);
    op.Value = (uint8_t)u32(av[4]);

    DWORD r;
    if (!ioctl(h, IOCTL_IT8888_DMA_FILL, &op, sizeof(op), NULL, 0, &r))
        return 1;

    printf("filled offset=0x%x count=%u value=0x%02x\n",
           op.Offset, op.Count, op.Value);
    return 0;
}

static void print_hex_ascii(uint32_t base, const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i += 16) {
        printf("%08x: ", base + i);

        for (uint32_t j = 0; j < 16; ++j) {
            if (i + j < n)
                printf("%02x ", p[i + j]);
            else
                printf("   ");
        }

        printf(" |");

        for (uint32_t j = 0; j < 16 && i + j < n; ++j) {
            uint8_t c = p[i + j];
            putchar((c >= 32 && c <= 126) ? c : '.');
        }

        printf("|\n");
    }
}

static int cmd_dma_dump(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("dma-dump <offset> <count>");
        return 2;
    }

    IT8888_DMA_DUMP d;
    memset(&d, 0, sizeof(d));
    d.Offset = u32(av[2]);
    d.Count = u32(av[3]);

    if (d.Count > IT8888_DMA_DUMP_MAX) {
        fprintf(stderr, "count too large, max %u\n", IT8888_DMA_DUMP_MAX);
        return 2;
    }

    DWORD r;
    if (!ioctl(h, IOCTL_IT8888_DMA_DUMP, &d, sizeof(d), &d, sizeof(d), &r))
        return 1;

    printf("dump offset=0x%x count=%u\n", d.Offset, d.Count);
    print_hex_ascii(d.Offset, d.Data, d.Count);
    return 0;
}

static int cmd_dma_check(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("dma-check <offset> <count> <byte>");
        return 2;
    }

    IT8888_DMA_CHECK c;
    memset(&c, 0, sizeof(c));
    c.Offset = u32(av[2]);
    c.Count = u32(av[3]);
    c.Expected = (uint8_t)u32(av[4]);

    DWORD r;
    if (!ioctl(h, IOCTL_IT8888_DMA_CHECK, &c, sizeof(c), &c, sizeof(c), &r))
        return 1;

    printf("check offset=0x%x count=%u expected=0x%02x mismatches=%u",
           c.Offset, c.Count, c.Expected, c.MismatchCount);

    if (c.MismatchCount)
        printf(" first_mismatch=0x%x actual=0x%02x",
               c.FirstMismatchOffset, c.FirstActual);

    printf("\n");
    return c.MismatchCount ? 3 : 0;
}

static int cmd_ddma_r8(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("ddma-r8 <channel> <offset>");
        return 2;
    }

    IT8888_DDMA_REG8 op;
    memset(&op, 0, sizeof(op));
    op.Channel = (uint8_t)u32(av[2]);
    op.Offset = (uint8_t)u32(av[3]);

    DWORD r;
    if (!ioctl(h, IOCTL_IT8888_DDMA_R8, &op, sizeof(op), &op, sizeof(op), &r))
        return 1;

    printf("ddma-r8 ch=%u off=0x%02x base=0x%04x port=0x%04x value=0x%02x\n",
           op.Channel, op.Offset, op.Base, op.Port, op.Value);
    return 0;
}

static int cmd_ddma_w8(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("ddma-w8 <channel> <offset> <value>");
        return 2;
    }

    IT8888_DDMA_REG8 op;
    memset(&op, 0, sizeof(op));
    op.Channel = (uint8_t)u32(av[2]);
    op.Offset = (uint8_t)u32(av[3]);
    op.Value = (uint8_t)u32(av[4]);

    DWORD r;
    if (!ioctl(h, IOCTL_IT8888_DDMA_W8, &op, sizeof(op), &op, sizeof(op), &r))
        return 1;

    printf("ddma-w8 ch=%u off=0x%02x base=0x%04x port=0x%04x value=0x%02x\n",
           op.Channel, op.Offset, op.Base, op.Port, op.Value);
    return 0;
}

enum {
    IT8888CTL_DDMA_REG_ADDR0     = 0x00,
    IT8888CTL_DDMA_REG_ADDR1     = 0x01,
    IT8888CTL_DDMA_REG_ADDR2     = 0x02,
    IT8888CTL_DDMA_REG_ADDR3     = 0x03,
    IT8888CTL_DDMA_REG_COUNT0    = 0x04,
    IT8888CTL_DDMA_REG_COUNT1    = 0x05,
    IT8888CTL_DDMA_REG_COMMAND   = 0x08,
    IT8888CTL_DDMA_REG_REQUEST   = 0x09,
    IT8888CTL_DDMA_REG_MODE      = 0x0B,
    IT8888CTL_DDMA_REG_MASTERCLR = 0x0D,
    IT8888CTL_DDMA_REG_MASK      = 0x0F
};
static int cmd_ddma_probe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("ddma-probe <channel>");
        return 2;
    }

    IT8888_DDMA_PROBE p;
    memset(&p, 0, sizeof(p));
    p.Channel = (uint8_t)u32(av[2]);
    p.Count = 16;

    DWORD r;
    if (!ioctl(h, IOCTL_IT8888_DDMA_PROBE, &p, sizeof(p), &p, sizeof(p), &r))
        return 1;

    printf("ddma-probe ch=%u base=0x%04x count=%u\n", p.Channel, p.Base, p.Count);
    for (uint32_t i = 0; i < p.Count; ++i) {
        printf("  +0x%02x port=0x%04x value=0x%02x", i, p.Base + i, p.Values[i]);

        if (i == IT8888CTL_DDMA_REG_ADDR0)      printf("  ADDR0");
        else if (i == IT8888CTL_DDMA_REG_ADDR1) printf("  ADDR1");
        else if (i == IT8888CTL_DDMA_REG_ADDR2) printf("  ADDR2");
        else if (i == IT8888CTL_DDMA_REG_ADDR3) printf("  ADDR3");
        else if (i == IT8888CTL_DDMA_REG_COUNT0) printf("  COUNT0");
        else if (i == IT8888CTL_DDMA_REG_COUNT1) printf("  COUNT1");
        else if (i == IT8888CTL_DDMA_REG_COMMAND) printf("  COMMAND/STATUS?");
        else if (i == IT8888CTL_DDMA_REG_REQUEST) printf("  REQUEST");
        else if (i == IT8888CTL_DDMA_REG_MODE) printf("  MODE");
        else if (i == IT8888CTL_DDMA_REG_MASTERCLR) printf("  MASTERCLR");
        else if (i == IT8888CTL_DDMA_REG_MASK) printf("  MASK");

        printf("\n");
    }

    return 0;
}

static uint16_t rd16u(const uint8_t *p, uint32_t off)
{
    return (uint16_t)(p[off] | ((uint16_t)p[off + 1] << 8));
}

static uint32_t rd32u(const uint8_t *p, uint32_t off)
{
    return (uint32_t)p[off] |
           ((uint32_t)p[off + 1] << 8) |
           ((uint32_t)p[off + 2] << 16) |
           ((uint32_t)p[off + 3] << 24);
}

static void print_pci_cfg_hex(const uint8_t *p)
{
    for (uint32_t off = 0; off < 256; off += 16) {
        printf("%02x:", off);
        for (uint32_t i = 0; i < 16; i += 4) {
            printf(" %08x", rd32u(p, off + i));
        }
        printf("\n");
    }
}

static void print_bridge_decode(const uint8_t *p)
{
    uint8_t baseClass = p[0x0B];
    uint8_t subClass  = p[0x0A];
    uint8_t progIf    = p[0x09];

    printf("\nclass=%02x subclass=%02x progif=%02x\n", baseClass, subClass, progIf);

    if (baseClass != 0x06 || subClass != 0x04) {
        printf("not a PCI-to-PCI bridge Type-1 decode target\n");
        return;
    }

    uint8_t pri = p[0x18];
    uint8_t sec = p[0x19];
    uint8_t sub = p[0x1A];
    uint8_t secLat = p[0x1B];

    uint8_t ioBaseRaw = p[0x1C];
    uint8_t ioLimitRaw = p[0x1D];
    uint16_t secStatus = rd16u(p, 0x1E);

    uint16_t memBaseRaw = rd16u(p, 0x20);
    uint16_t memLimitRaw = rd16u(p, 0x22);
    uint16_t preBaseRaw = rd16u(p, 0x24);
    uint16_t preLimitRaw = rd16u(p, 0x26);
    uint32_t preBaseUpper = rd32u(p, 0x28);
    uint32_t preLimitUpper = rd32u(p, 0x2C);

    uint16_t ioBaseUpper = rd16u(p, 0x30);
    uint16_t ioLimitUpper = rd16u(p, 0x32);
    uint16_t bridgeCtl = rd16u(p, 0x3E);

    printf("bridge buses: primary=%u secondary=%u subordinate=%u sec_latency=%u\n",
           pri, sec, sub, secLat);

    printf("secondary_status=0x%04x bridge_control=0x%04x\n", secStatus, bridgeCtl);

    printf("raw io_base=0x%02x io_limit=0x%02x io_base_upper=0x%04x io_limit_upper=0x%04x\n",
           ioBaseRaw, ioLimitRaw, ioBaseUpper, ioLimitUpper);

    if ((ioBaseRaw & 0x0F) == 0x01 || (ioLimitRaw & 0x0F) == 0x01) {
        uint32_t ioBase = ((uint32_t)ioBaseUpper << 16) | ((uint32_t)(ioBaseRaw & 0xF0) << 8);
        uint32_t ioLimit = ((uint32_t)ioLimitUpper << 16) | ((uint32_t)(ioLimitRaw & 0xF0) << 8) | 0x0FFFu;

        if ((ioBaseRaw & 0xF0) > (ioLimitRaw & 0xF0) && ioBaseUpper == ioLimitUpper) {
            printf("I/O window appears disabled or inverted: base=0x%08x limit=0x%08x\n", ioBase, ioLimit);
        } else {
            printf("I/O window decoded: 0x%08x-0x%08x\n", ioBase, ioLimit);
            printf("contains 0x0220: %s\n", (ioBase <= 0x220 && 0x220 <= ioLimit) ? "YES" : "NO");
            printf("contains 0x0330: %s\n", (ioBase <= 0x330 && 0x330 <= ioLimit) ? "YES" : "NO");
            printf("contains 0x0388: %s\n", (ioBase <= 0x388 && 0x388 <= ioLimit) ? "YES" : "NO");
            printf("contains 0x8390: %s\n", (ioBase <= 0x8390 && 0x8390 <= ioLimit) ? "YES" : "NO");
        }
    } else {
        uint32_t ioBase = ((uint32_t)(ioBaseRaw & 0xF0) << 8);
        uint32_t ioLimit = ((uint32_t)(ioLimitRaw & 0xF0) << 8) | 0x0FFFu;

        if ((ioBaseRaw & 0xF0) > (ioLimitRaw & 0xF0)) {
            printf("I/O window appears disabled or inverted: base=0x%08x limit=0x%08x\n", ioBase, ioLimit);
        } else {
            printf("I/O window decoded: 0x%08x-0x%08x\n", ioBase, ioLimit);
            printf("contains 0x0220: %s\n", (ioBase <= 0x220 && 0x220 <= ioLimit) ? "YES" : "NO");
            printf("contains 0x0330: %s\n", (ioBase <= 0x330 && 0x330 <= ioLimit) ? "YES" : "NO");
            printf("contains 0x0388: %s\n", (ioBase <= 0x388 && 0x388 <= ioLimit) ? "YES" : "NO");
            printf("contains 0x8390: %s\n", (ioBase <= 0x8390 && 0x8390 <= ioLimit) ? "YES" : "NO");
        }
    }

    uint32_t memBase = ((uint32_t)(memBaseRaw & 0xFFF0) << 16);
    uint32_t memLimit = ((uint32_t)(memLimitRaw & 0xFFF0) << 16) | 0x000FFFFFu;
    printf("memory window decoded: 0x%08x-0x%08x\n", memBase, memLimit);

    printf("prefetch raw base=0x%04x limit=0x%04x upper_base=0x%08x upper_limit=0x%08x\n",
           preBaseRaw, preLimitRaw, preBaseUpper, preLimitUpper);
}

static int cmd_pci_dumpcfg(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("pci-dumpcfg <bus> <device> <function>");
        return 2;
    }

    IT8888_PCI_CFG_DUMP d;
    memset(&d, 0, sizeof(d));
    d.Bus = (uint8_t)u32(av[2]);
    d.Device = (uint8_t)u32(av[3]);
    d.Function = (uint8_t)u32(av[4]);

    DWORD r;
    if (!ioctl(h, IOCTL_IT8888_PCI_DUMPCFG, &d, sizeof(d), &d, sizeof(d), &r))
        return 1;

    printf("pci-dumpcfg bus=%u dev=%u func=%u bytes=%u status=%u\n",
           d.Bus, d.Device, d.Function, d.BytesRead, d.Status);

    if (d.BytesRead < 64) {
        printf("short PCI config read; device may not exist or HalGetBusDataByOffset could not access it\n");
        return 1;
    }

    printf("vendor:device %04x:%04x command/status %08x class %02x%02x%02x rev %02x\n",
           rd16u(d.Data, 0x00),
           rd16u(d.Data, 0x02),
           rd32u(d.Data, 0x04),
           d.Data[0x0B], d.Data[0x0A], d.Data[0x09], d.Data[0x08]);

    print_pci_cfg_hex(d.Data);
    print_bridge_decode(d.Data);

    return 0;
}

static int cmd_pci_cfgread(HANDLE h, int ac, char **av)
{
    if (ac < 7) { puts("pci-cfgread <bus> <device> <function> <offset> <width>"); return 2; }
    IT8888_PCI_CFG_RW rw; memset(&rw,0,sizeof(rw));
    rw.Bus=(uint8_t)u32(av[2]); rw.Device=(uint8_t)u32(av[3]); rw.Function=(uint8_t)u32(av[4]); rw.Offset=(uint8_t)u32(av[5]); rw.Width=(uint8_t)u32(av[6]);
    DWORD r; if(!ioctl(h,IOCTL_IT8888_PCI_CFGRW,&rw,sizeof(rw),&rw,sizeof(rw),&r)) return 1;
    printf("pci-cfgread %u:%u.%u off=0x%02x width=%u value=0x%0*x bytes=%u status=%u\n", rw.Bus,rw.Device,rw.Function,rw.Offset,rw.Width,rw.Width*2,rw.Value,rw.Bytes,rw.Status);
    return 0;
}

static int cmd_pci_cfgwrite(HANDLE h, int ac, char **av)
{
    if (ac < 8) { puts("pci-cfgwrite <bus> <device> <function> <offset> <width> <value>"); return 2; }
    IT8888_PCI_CFG_RW rw; memset(&rw,0,sizeof(rw));
    rw.Bus=(uint8_t)u32(av[2]); rw.Device=(uint8_t)u32(av[3]); rw.Function=(uint8_t)u32(av[4]); rw.Offset=(uint8_t)u32(av[5]); rw.Width=(uint8_t)u32(av[6]); rw.Value=u32(av[7]); rw.Write=1;
    DWORD r; if(!ioctl(h,IOCTL_IT8888_PCI_CFGRW,&rw,sizeof(rw),&rw,sizeof(rw),&r)) return 1;
    printf("pci-cfgwrite %u:%u.%u off=0x%02x width=%u value=0x%x bytes=%u status=%u\n", rw.Bus,rw.Device,rw.Function,rw.Offset,rw.Width,rw.Value,rw.Bytes,rw.Status);
    return 0;
}

static int cmd_bridge_iowin(HANDLE h, int ac, char **av)
{
    if (ac < 7) { puts("bridge-iowin <bus> <device> <function> <base> <limit>"); return 2; }
    IT8888_BRIDGE_IOWIN w; memset(&w,0,sizeof(w));
    w.Bus=(uint8_t)u32(av[2]); w.Device=(uint8_t)u32(av[3]); w.Function=(uint8_t)u32(av[4]); w.Base=u32(av[5]); w.Limit=u32(av[6]);
    DWORD r; if(!ioctl(h,IOCTL_IT8888_BRIDGE_IOWIN,&w,sizeof(w),&w,sizeof(w),&r)) return 1;
    printf("bridge-iowin %u:%u.%u 0x%04x-0x%04x\n", w.Bus,w.Device,w.Function,w.Base,w.Limit);
    printf("  command old=0x%04x new=0x%04x\n", w.OldCommand,w.NewCommand);
    printf("  io base  old=0x%02x upper=0x%04x  new=0x%02x upper=0x%04x\n", w.OldIoBase,w.OldIoBaseUpper,w.NewIoBase,w.NewIoBaseUpper);
    printf("  io limit old=0x%02x upper=0x%04x  new=0x%02x upper=0x%04x\n", w.OldIoLimit,w.OldIoLimitUpper,w.NewIoLimit,w.NewIoLimitUpper);
    printf("  status=%u\n", w.Status);
    return 0;
}
/* ------------------------------------------------------------------------- */
/* PicoGUS helpers                                                            */
/* ------------------------------------------------------------------------- */

#define PGUS_DEFAULT_CTRL_ALIAS 0x81D0u

#define PGUS_CMD_MAGIC    0x00u
#define PGUS_CMD_PROTOCOL 0x01u
#define PGUS_CMD_FWSTRING 0x02u
#define PGUS_CMD_BOOTMODE 0x03u
#define PGUS_CMD_GUSPORT  0x04u
#define PGUS_CMD_GUSBUF   0x10u
#define PGUS_CMD_GUSDMA   0x11u
#define PGUS_CMD_GUS44K   0x12u
#define PGUS_CMD_MAINVOL  0x70u
#define PGUS_CMD_GUSVOL   0x74u

static uint16_t pgus_ctrl_alias(int ac, char **av, int idx)
{
    return (uint16_t)((ac > idx) ? u32(av[idx]) : PGUS_DEFAULT_CTRL_ALIAS);
}

static int port_out8(HANDLE h, uint16_t port, uint8_t value)
{
    IT8888_PORT_ACCESS a;
    DWORD r;
    memset(&a, 0, sizeof(a));
    a.Port = port;
    a.Width = 1;
    a.Value = value;
    return ioctl(h, IOCTL_IT8888_PORT_WRITE, &a, sizeof(a), NULL, 0, &r) ? 0 : 1;
}

static int port_in8(HANDLE h, uint16_t port, uint8_t *value)
{
    IT8888_PORT_ACCESS a;
    DWORD r;
    memset(&a, 0, sizeof(a));
    a.Port = port;
    a.Width = 1;
    if (!ioctl(h, IOCTL_IT8888_PORT_READ, &a, sizeof(a), &a, sizeof(a), &r))
        return 1;
    *value = (uint8_t)(a.Value & 0xFFu);
    return 0;
}

static int port_out16_bytes(HANDLE h, uint16_t port, uint16_t value)
{
    if (port_out8(h, port, (uint8_t)(value & 0xFFu))) return 1;
    if (port_out8(h, (uint16_t)(port + 1), (uint8_t)((value >> 8) & 0xFFu))) return 1;
    return 0;
}

static int port_in16_bytes(HANDLE h, uint16_t port, uint16_t *value)
{
    uint8_t lo = 0, hi = 0;
    if (port_in8(h, port, &lo)) return 1;
    if (port_in8(h, (uint16_t)(port + 1), &hi)) return 1;
    *value = (uint16_t)(lo | ((uint16_t)hi << 8));
    return 0;
}

static int pgus_select(HANDLE h, uint16_t ctrl, uint8_t cmd)
{
    if (port_out8(h, ctrl, 0xCCu)) return 1;
    if (port_out8(h, ctrl, cmd)) return 1;
    return 0;
}

static int pgus_read8_value(HANDLE h, uint16_t ctrl, uint8_t cmd, uint8_t *value)
{
    if (pgus_select(h, ctrl, cmd)) return 1;
    return port_in8(h, (uint16_t)(ctrl + 2), value);
}

static int pgus_write8_value(HANDLE h, uint16_t ctrl, uint8_t cmd, uint8_t value)
{
    if (pgus_select(h, ctrl, cmd)) return 1;
    return port_out8(h, (uint16_t)(ctrl + 2), value);
}

static int pgus_read16_value(HANDLE h, uint16_t ctrl, uint8_t cmd, uint16_t *value)
{
    if (pgus_select(h, ctrl, cmd)) return 1;
    return port_in16_bytes(h, (uint16_t)(ctrl + 1), value);
}

static int pgus_write16_value(HANDLE h, uint16_t ctrl, uint8_t cmd, uint16_t value)
{
    if (pgus_select(h, ctrl, cmd)) return 1;
    return port_out16_bytes(h, (uint16_t)(ctrl + 1), value);
}

static int pgus_print_string(HANDLE h, uint16_t ctrl, uint8_t cmd, uint32_t count)
{
    char s[260];
    uint32_t n = count;
    if (n > 255) n = 255;
    memset(s, 0, sizeof(s));

    if (pgus_select(h, ctrl, cmd)) return 1;

    printf("pgus string cmd=0x%02x ctrl=0x%04x count=%u\n", cmd, ctrl, n);
    printf("hex:");
    for (uint32_t i = 0; i < n; i++) {
        uint8_t b = 0;
        if (port_in8(h, (uint16_t)(ctrl + 2), &b)) return 1;
        printf(" %02x", b);
        s[i] = (b >= 32 && b < 127) ? (char)b : '.';
        if (b == 0) break;
    }
    printf("\nascii: %s\n", s);
    return 0;
}

static int cmd_pgus_protocol(HANDLE h, int ac, char **av)
{
    uint16_t ctrl = pgus_ctrl_alias(ac, av, 2);
    uint8_t v = 0;
    if (pgus_read8_value(h, ctrl, PGUS_CMD_PROTOCOL, &v)) return 1;
    printf("pgus protocol ctrl=0x%04x value=0x%02x (%u)\n", ctrl, v, v);
    return 0;
}

static int cmd_pgus_fwstring(HANDLE h, int ac, char **av)
{
    uint32_t count = (ac > 2) ? u32(av[2]) : 64;
    uint16_t ctrl = pgus_ctrl_alias(ac, av, 3);
    return pgus_print_string(h, ctrl, PGUS_CMD_FWSTRING, count);
}

static int cmd_pgus_magic(HANDLE h, int ac, char **av)
{
    uint32_t count = (ac > 2) ? u32(av[2]) : 16;
    uint16_t ctrl = pgus_ctrl_alias(ac, av, 3);
    return pgus_print_string(h, ctrl, PGUS_CMD_MAGIC, count);
}

static int cmd_pgus_read8(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("pgus-read8 <cmd> [count] [ctrl_alias]");
        return 2;
    }

    uint8_t cmd = (uint8_t)u32(av[2]);
    uint32_t count = (ac > 3) ? u32(av[3]) : 1;
    uint16_t ctrl = pgus_ctrl_alias(ac, av, 4);

    if (pgus_select(h, ctrl, cmd)) return 1;

    printf("pgus-read8 cmd=0x%02x ctrl=0x%04x count=%u\n", cmd, ctrl, count);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t b = 0;
        if (port_in8(h, (uint16_t)(ctrl + 2), &b)) return 1;
        printf("[%u]=0x%02x", i, b);
        if (b >= 32 && b < 127) printf(" '%c'", b);
        printf("\n");
    }

    return 0;
}

static int cmd_pgus_write8(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("pgus-write8 <cmd> <value> [ctrl_alias]");
        return 2;
    }

    uint8_t cmd = (uint8_t)u32(av[2]);
    uint8_t value = (uint8_t)u32(av[3]);
    uint16_t ctrl = pgus_ctrl_alias(ac, av, 4);

    if (pgus_write8_value(h, ctrl, cmd, value)) return 1;
    printf("pgus-write8 cmd=0x%02x value=0x%02x ctrl=0x%04x\n", cmd, value, ctrl);
    return 0;
}

static int cmd_pgus_write16(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("pgus-write16 <cmd> <value> [ctrl_alias]");
        return 2;
    }

    uint8_t cmd = (uint8_t)u32(av[2]);
    uint16_t value = (uint16_t)u32(av[3]);
    uint16_t ctrl = pgus_ctrl_alias(ac, av, 4);

    if (pgus_write16_value(h, ctrl, cmd, value)) return 1;
    printf("pgus-write16 cmd=0x%02x value=0x%04x ctrl=0x%04x (byte writes)\n", cmd, value, ctrl);
    return 0;
}

static int cmd_pgus_gus_get(HANDLE h, int ac, char **av)
{
    uint16_t ctrl = pgus_ctrl_alias(ac, av, 2);
    uint16_t gusport = 0;
    uint8_t gusbuf = 0, gusdma = 0, gus44k = 0, gusvol = 0, mainvol = 0;

    if (pgus_read16_value(h, ctrl, PGUS_CMD_GUSPORT, &gusport)) return 1;
    if (pgus_read8_value(h, ctrl, PGUS_CMD_GUSBUF, &gusbuf)) return 1;
    if (pgus_read8_value(h, ctrl, PGUS_CMD_GUSDMA, &gusdma)) return 1;
    if (pgus_read8_value(h, ctrl, PGUS_CMD_GUS44K, &gus44k)) return 1;
    if (pgus_read8_value(h, ctrl, PGUS_CMD_GUSVOL, &gusvol)) return 1;
    if (pgus_read8_value(h, ctrl, PGUS_CMD_MAINVOL, &mainvol)) return 1;

    printf("PicoGUS GUS settings ctrl=0x%04x\n", ctrl);
    printf(" gusport=0x%04x\n", gusport);
    printf(" gusbuf=%u\n", gusbuf);
    printf(" gusdma_interval_us=%u\n", gusdma);
    printf(" gus44k=%u\n", gus44k);
    printf(" gusvol=%u\n", gusvol);
    printf(" mainvol=%u\n", mainvol);
    puts(" note: pgusinit only sets GUS base/buffer/dma-interval/44k/vol here; GUS IRQ/DMA channel use ULTRASND/jumpers.");
    return 0;
}

static int cmd_pgus_gus_set(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("pgus-gus-set <gusport> [gusbuf] [gusdma_interval_us] [force44k] [gusvol] [ctrl_alias]");
        return 2;
    }

    uint16_t ctrl = (uint16_t)((ac > 7) ? u32(av[7]) : PGUS_DEFAULT_CTRL_ALIAS);
    uint16_t gusport = (uint16_t)u32(av[2]);

    if (pgus_write16_value(h, ctrl, PGUS_CMD_GUSPORT, gusport)) return 1;
    printf("set gusport=0x%04x\n", gusport);

    if (ac > 3) {
        uint8_t v = (uint8_t)u32(av[3]);
        if (pgus_write8_value(h, ctrl, PGUS_CMD_GUSBUF, v)) return 1;
        printf("set gusbuf=%u\n", v);
    }
    if (ac > 4) {
        uint8_t v = (uint8_t)u32(av[4]);
        if (pgus_write8_value(h, ctrl, PGUS_CMD_GUSDMA, v)) return 1;
        printf("set gusdma_interval_us=%u\n", v);
    }
    if (ac > 5) {
        uint8_t v = (uint8_t)u32(av[5]);
        if (pgus_write8_value(h, ctrl, PGUS_CMD_GUS44K, v)) return 1;
        printf("set gus44k=%u\n", v);
    }
    if (ac > 6) {
        uint8_t v = (uint8_t)u32(av[6]);
        if (pgus_write8_value(h, ctrl, PGUS_CMD_GUSVOL, v)) return 1;
        printf("set gusvol=%u\n", v);
    }

    puts("set done; run pgus-gus-get to verify.");
    return 0;
}

static int gus_probe_core(HANDLE h, uint16_t base, int do_reset)
{
    uint8_t v = 0;

    if (port_out8(h, (uint16_t)(base + 0x103), 0x43)) return 1;
    if (port_out16_bytes(h, (uint16_t)(base + 0x104), 0x0000)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x103), 0x44)) return 1;
    if (port_out16_bytes(h, (uint16_t)(base + 0x104), 0x0000)) return 1;

    if (port_out8(h, (uint16_t)(base + 0x107), 0xDD)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x107), &v)) return 1;

    printf("gus-probe base=0x%04x test-read=0x%02x %s\n",
           base, v, (v == 0xDD) ? "OK" : "FAIL");

    if (do_reset) {
        puts("gus-reset: enabling IRQ latch/master reset run bit like pgusinit");
        if (port_out8(h, base, 0x08)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x103), 0x4C)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x105), 0x01)) return 1;
    }

    return (v == 0xDD) ? 0 : 1;
}

static int cmd_gus_probe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-probe <gus_alias_base>");
        return 2;
    }
    return gus_probe_core(h, (uint16_t)u32(av[2]), 0);
}

static int cmd_gus_reset(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-reset <gus_alias_base>");
        return 2;
    }
    return gus_probe_core(h, (uint16_t)u32(av[2]), 1);
}

/* ------------------------------------------------------------------------- */
/* Safer GUS register helpers                                                 */
/* ------------------------------------------------------------------------- */

static int gus_reg_select(HANDLE h, uint16_t base, uint8_t reg)
{
    return port_out8(h, (uint16_t)(base + 0x103), reg);
}

static int gus_reg_read8(HANDLE h, uint16_t base, uint8_t reg, uint8_t *value)
{
    if (gus_reg_select(h, base, reg)) return 1;
    return port_in8(h, (uint16_t)(base + 0x105), value);
}

static int gus_reg_write8(HANDLE h, uint16_t base, uint8_t reg, uint8_t value)
{
    if (gus_reg_select(h, base, reg)) return 1;
    return port_out8(h, (uint16_t)(base + 0x105), value);
}

static int cmd_gus_reg_read(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-reg-read <gus_alias_base> <reg>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t reg = (uint8_t)u32(av[3]);
    uint8_t value = 0;

    if (gus_reg_read8(h, base, reg, &value)) return 1;

    printf("gus-reg-read base=0x%04x reg=0x%02x value=0x%02x\n", base, reg, value);
    return 0;
}

static int cmd_gus_reg_write(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-reg-write <gus_alias_base> <reg> <value>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t reg = (uint8_t)u32(av[3]);
    uint8_t value = (uint8_t)u32(av[4]);

    if (gus_reg_write8(h, base, reg, value)) return 1;

    printf("gus-reg-write base=0x%04x reg=0x%02x value=0x%02x\n", base, reg, value);
    return 0;
}

static int cmd_gus_reg_probe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-reg-probe <gus_alias_base>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t v = 0;

    /*
        Scratch-style test known to work from pgusinit:
          select 0x43, clear address low
          select 0x44, clear address high
          write/read 0xDD at base+0x107
    */
    if (port_out8(h, (uint16_t)(base + 0x103), 0x43)) return 1;
    if (port_out16_bytes(h, (uint16_t)(base + 0x104), 0x0000)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x103), 0x44)) return 1;
    if (port_out16_bytes(h, (uint16_t)(base + 0x104), 0x0000)) return 1;

    if (port_out8(h, (uint16_t)(base + 0x107), 0xDD)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x107), &v)) return 1;

    printf("gus-reg-probe base=0x%04x scratch=0x%02x %s\n",
           base, v, (v == 0xDD) ? "OK" : "FAIL");

    return (v == 0xDD) ? 0 : 1;
}

static int cmd_gus_reg_reset_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-reg-reset-safe <gus_alias_base>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t v = 0;

    /*
        This exactly matches the manual sequence that succeeded:
          out base, 0x08
          select 0x4C
          write 0x01 to selected register's high/data byte at base+0x105
          read it back
    */
    if (port_out8(h, base, 0x08)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x103), 0x4C)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x105), 0x01)) return 1;
    Sleep(20);
    if (port_out8(h, (uint16_t)(base + 0x103), 0x4C)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &v)) return 1;

    printf("gus-reg-reset-safe base=0x%04x master-reset-readback=0x%02x %s\n",
           base, v, (v == 0x01) ? "OK" : "CHECK");

    return 0;
}

static int cmd_gus_map_test(HANDLE h, int ac, char **av)
{
    (void)h; (void)ac; (void)av;
    puts("Known-good GUS mapping for this IT8888 setup:");
    puts("  bridge-iowin 0 28 3 0x8000 0x8fff");
    puts("  bridge-iowin 3 0 0 0x8000 0x8fff");
    puts("  cfgwrite 0x5C 4 0xe3008340   # GUS +0x100 regs");
    puts("  cfgwrite 0x60 4 0xe2008240   # GUS base");
    puts("  gus-reg-probe 0x8240");
    puts("  gus-reg-reset-safe 0x8240");
    puts("Restore PicoGUS control:");
    puts("  cfgwrite 0x60 4 0xe20081d0");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* GUS DMA-oriented selected register helpers                                 */
/* ------------------------------------------------------------------------- */

static int gus_selected_reg_write16_value(HANDLE h, uint16_t base, uint8_t reg, uint16_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_out16_bytes(h, (uint16_t)(base + 0x104), value);
}

static int gus_selected_reg_read16_value(HANDLE h, uint16_t base, uint8_t reg, uint16_t *value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_in16_bytes(h, (uint16_t)(base + 0x104), value);
}

static int gus_selected_reg_write8_value(HANDLE h, uint16_t base, uint8_t reg, uint8_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_out8(h, (uint16_t)(base + 0x105), value);
}

static int gus_selected_reg_read8_value(HANDLE h, uint16_t base, uint8_t reg, uint8_t *value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_in8(h, (uint16_t)(base + 0x105), value);
}

static int cmd_gus_reg_read16(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-reg-read16 <gus_alias_base> <reg>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t reg = (uint8_t)u32(av[3]);
    uint16_t value = 0;

    if (gus_selected_reg_read16_value(h, base, reg, &value)) return 1;

    printf("gus-reg-read16 base=0x%04x reg=0x%02x value=0x%04x\n",
           base, reg, value);
    return 0;
}

static int cmd_gus_reg_write16(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-reg-write16 <gus_alias_base> <reg> <value>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t reg = (uint8_t)u32(av[3]);
    uint16_t value = (uint16_t)u32(av[4]);

    if (gus_selected_reg_write16_value(h, base, reg, value)) return 1;

    printf("gus-reg-write16 base=0x%04x reg=0x%02x value=0x%04x\n",
           base, reg, value);
    return 0;
}

static int gus_dma_kick_once(HANDLE h, uint16_t base, uint16_t dram_addr, uint8_t ctrl, uint32_t settle_ms)
{
    uint16_t addr_back_before = 0;
    uint16_t addr_back_after = 0;
    uint8_t ctrl_before = 0;
    uint8_t ctrl_after = 0;

    /*
        Minimal GUS DMA kick:
          reg 0x42 = DRAM DMA address, 16-bit selected register
          reg 0x41 = DMA control/status, 8-bit selected register

        This does NOT touch IT8888/DDMA directly. Arm DDMA first using ddma-arm.
    */
    if (gus_selected_reg_write16_value(h, base, 0x42, dram_addr)) return 1;
    if (gus_selected_reg_read16_value(h, base, 0x42, &addr_back_before)) return 1;
    if (gus_selected_reg_read8_value(h, base, 0x41, &ctrl_before)) return 1;

    if (gus_selected_reg_write8_value(h, base, 0x41, ctrl)) return 1;

    if (settle_ms)
        Sleep(settle_ms);

    if (gus_selected_reg_read8_value(h, base, 0x41, &ctrl_after)) return 1;
    if (gus_selected_reg_read16_value(h, base, 0x42, &addr_back_after)) return 1;

    printf("gus-dma-kick base=0x%04x dram=0x%04x ctrl=0x%02x settle=%ums\n",
           base, dram_addr, ctrl, settle_ms);
    printf("  addr_before=0x%04x ctrl_before=0x%02x\n",
           addr_back_before, ctrl_before);
    printf("  addr_after =0x%04x ctrl_after =0x%02x\n",
           addr_back_after, ctrl_after);

    return 0;
}

static int cmd_gus_dma_kick(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-dma-kick <gus_alias_base> <dram_addr16> <ctrl> [settle_ms]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint16_t dram_addr = (uint16_t)u32(av[3]);
    uint8_t ctrl = (uint8_t)u32(av[4]);
    uint32_t settle_ms = (ac > 5) ? u32(av[5]) : 100;

    return gus_dma_kick_once(h, base, dram_addr, ctrl, settle_ms);
}

static int cmd_gus_dma_sweep(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-dma-sweep <gus_alias_base> <dram_addr16> <ctrl0> [ctrl1 ...]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint16_t dram_addr = (uint16_t)u32(av[3]);

    for (int i = 4; i < ac; i++) {
        uint8_t ctrl = (uint8_t)u32(av[i]);
        printf("---- sweep ctrl=0x%02x ----\n", ctrl);
        if (gus_dma_kick_once(h, base, dram_addr, ctrl, 100))
            return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* DDMA/GUS DMA observability helpers                                         */
/* ------------------------------------------------------------------------- */

static uint16_t ddma_guess_base_for_channel(unsigned ch)
{
    /*
        Current IT8888 init layout observed repeatedly:
          ch0=0x8380 ch1=0x8390 ch2=0x83a0 ch3=0x83b0
          ch4 unused/8237 cascade
          ch5=0x83d0 ch6=0x83e0 ch7=0x83f0
    */
    if (ch <= 3) return (uint16_t)(0x8380u + ch * 0x10u);
    if (ch >= 5 && ch <= 7) return (uint16_t)(0x8380u + ch * 0x10u);
    return 0;
}

static const char *ddma_off_name(unsigned off)
{
    switch (off) {
    case 0x00: return "ADDR0";
    case 0x01: return "ADDR1";
    case 0x02: return "ADDR2";
    case 0x03: return "ADDR3";
    case 0x04: return "COUNT0";
    case 0x05: return "COUNT1";
    case 0x08: return "CMD/STATUS";
    case 0x09: return "REQUEST";
    case 0x0b: return "MODE";
    case 0x0d: return "MASTERCLR";
    case 0x0f: return "MASK";
    default: return "";
    }
}

static int ddma_probe_one_channel(HANDLE h, unsigned ch)
{
    uint16_t base = ddma_guess_base_for_channel(ch);
    if (!base) {
        printf("ch%u base=0000 skipped\n", ch);
        return 0;
    }

    printf("ch%u base=0x%04x:", ch, base);

    for (unsigned off = 0; off < 0x10; off++) {
        uint8_t v = 0;
        if (port_in8(h, (uint16_t)(base + off), &v)) {
            printf(" +%02x=ERR\n", off);
            return 1;
        }
        printf(" %02x", v);
    }

    printf("  labels: +08=%s +09=%s +0b=%s +0f=%s\n",
           ddma_off_name(0x08), ddma_off_name(0x09),
           ddma_off_name(0x0b), ddma_off_name(0x0f));
    return 0;
}

static int ddma_probe_all_once(HANDLE h)
{
    for (unsigned ch = 0; ch <= 7; ch++) {
        if (ch == 4) continue;
        if (ddma_probe_one_channel(h, ch)) return 1;
    }
    return 0;
}

static int cmd_ddma_probe_all(HANDLE h, int ac, char **av)
{
    /* DISABLED_BY_SAFE_CH_ONLY_PATCH */
    (void)h; (void)ac; (void)av;
    puts("ddma-probe-all disabled: unsafe broad DDMA reads can hang this machine. Use ddma-probe-ch-safe 1.");
    return 2;

    uint32_t loops = (ac > 2) ? u32(av[2]) : 1;
    uint32_t delay_ms = (ac > 3) ? u32(av[3]) : 0;

    for (uint32_t i = 0; i < loops; i++) {
        if (loops > 1) printf("---- ddma-probe-all loop %u/%u ----\n", i + 1, loops);
        if (ddma_probe_all_once(h)) return 1;
        if (delay_ms && i + 1 < loops) Sleep(delay_ms);
    }

    return 0;
}

static int cmd_gus_dma_status_loop(HANDLE h, int ac, char **av)
{
    /* DISABLED_BY_SAFE_CH_ONLY_PATCH */
    (void)h; (void)ac; (void)av;
    puts("gus-dma-status-loop disabled: unsafe all-channel loop. Use explicit gus-reg-read plus ddma-probe-ch-safe 1.");
    return 2;

    if (ac < 3) {
        puts("gus-dma-status-loop <gus_alias_base> [loops] [delay_ms]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t loops = (ac > 3) ? u32(av[3]) : 10;
    uint32_t delay_ms = (ac > 4) ? u32(av[4]) : 100;

    for (uint32_t i = 0; i < loops; i++) {
        uint8_t ctrl = 0;
        uint16_t addr = 0;

        if (gus_selected_reg_read8_value(h, base, 0x41, &ctrl)) return 1;
        if (gus_selected_reg_read16_value(h, base, 0x42, &addr)) return 1;

        printf("---- loop %u/%u: gus reg41=0x%02x reg42=0x%04x ----\n",
               i + 1, loops, ctrl, addr);

        if (ddma_probe_all_once(h)) return 1;

        if (delay_ms && i + 1 < loops) Sleep(delay_ms);
    }

    return 0;
}

static int cmd_gus_ddma_sweep_watch(HANDLE h, int ac, char **av)
{
    /* DISABLED_BY_SAFE_CH_ONLY_PATCH */
    (void)h; (void)ac; (void)av;
    puts("gus-ddma-sweep-watch disabled: unsafe all-channel probe. Use gus-ddma-sweep-watch-ch 1 ...");
    return 2;

    if (ac < 5) {
        puts("gus-ddma-sweep-watch <gus_alias_base> <dram_addr16> <ctrl0> [ctrl1 ...]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint16_t dram_addr = (uint16_t)u32(av[3]);

    for (int i = 4; i < ac; i++) {
        uint8_t ctrl = (uint8_t)u32(av[i]);

        printf("========== ctrl=0x%02x: before ==========\n", ctrl);
        if (ddma_probe_all_once(h)) return 1;

        printf("========== ctrl=0x%02x: kick ==========\n", ctrl);
        if (gus_dma_kick_once(h, base, dram_addr, ctrl, 100)) return 1;

        printf("========== ctrl=0x%02x: after ==========\n", ctrl);
        if (ddma_probe_all_once(h)) return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Safe channel-only DDMA observability                                       */
/* ------------------------------------------------------------------------- */

static uint16_t ddma_safe_base_for_channel(unsigned ch)
{
    /*
        Current IT8888 init layout:
          ch0=0x8380 ch1=0x8390 ch2=0x83a0 ch3=0x83b0
          ch4 cascade/unused here
          ch5=0x83d0 ch6=0x83e0 ch7=0x83f0

        Do NOT use this to broad-scan all channels. Use only the known
        hardware jumper channel. For current PicoGUS: DMA 1 => ch1.
    */
    if (ch <= 3) return (uint16_t)(0x8380u + ch * 0x10u);
    if (ch >= 5 && ch <= 7) return (uint16_t)(0x8380u + ch * 0x10u);
    return 0;
}

static int ddma_probe_ch_safe_once(HANDLE h, unsigned ch)
{
    uint16_t base = ddma_safe_base_for_channel(ch);
    uint8_t v08 = 0, v09 = 0, v0b = 0, v0f = 0;

    if (!base) {
        printf("ddma-probe-ch-safe ch%u invalid/unsupported\n", ch);
        return 2;
    }

    /*
        Only these offsets are used. The earlier all-channel/full-window probe
        hung the machine after GUS DMA ctrl=0x01 when it touched ch0.
    */
    if (port_in8(h, (uint16_t)(base + 0x08), &v08)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x09), &v09)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x0b), &v0b)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x0f), &v0f)) return 1;

    printf("ddma-ch%u base=0x%04x cmd/status[08]=0x%02x request[09]=0x%02x mode[0b]=0x%02x mask[0f]=0x%02x\n",
           ch, base, v08, v09, v0b, v0f);
    return 0;
}

static int cmd_ddma_probe_ch_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("ddma-probe-ch-safe <ch> [loops] [delay_ms]");
        return 2;
    }

    unsigned ch = (unsigned)u32(av[2]);
    uint32_t loops = (ac > 3) ? u32(av[3]) : 1;
    uint32_t delay_ms = (ac > 4) ? u32(av[4]) : 0;

    for (uint32_t i = 0; i < loops; i++) {
        if (loops > 1) printf("---- ddma-probe-ch-safe ch%u loop %u/%u ----\n", ch, i + 1, loops);
        int rc = ddma_probe_ch_safe_once(h, ch);
        if (rc) return rc;
        if (delay_ms && i + 1 < loops) Sleep(delay_ms);
    }

    return 0;
}

static int cmd_gus_ddma_sweep_watch_ch(HANDLE h, int ac, char **av)
{
    if (ac < 6) {
        puts("gus-ddma-sweep-watch-ch <ch> <gus_alias_base> <dram_addr16> <ctrl0> [ctrl1 ...]");
        return 2;
    }

    unsigned ch = (unsigned)u32(av[2]);
    uint16_t base = (uint16_t)u32(av[3]);
    uint16_t dram_addr = (uint16_t)u32(av[4]);

    if (!ddma_safe_base_for_channel(ch)) {
        printf("gus-ddma-sweep-watch-ch: invalid ch%u\n", ch);
        return 2;
    }

    for (int i = 5; i < ac; i++) {
        uint8_t ctrl = (uint8_t)u32(av[i]);

        printf("========== ch%u ctrl=0x%02x: before ==========\n", ch, ctrl);
        if (ddma_probe_ch_safe_once(h, ch)) return 1;

        printf("========== ch%u ctrl=0x%02x: kick ==========\n", ch, ctrl);
        if (gus_dma_kick_once(h, base, dram_addr, ctrl, 100)) return 1;

        printf("========== ch%u ctrl=0x%02x: after ==========\n", ch, ctrl);
        if (ddma_probe_ch_safe_once(h, ch)) return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* GUS DRAM and simple voice test helpers                                     */
/* ------------------------------------------------------------------------- */

static int gus_dram_set_addr2(HANDLE h, uint16_t base, uint32_t addr)
{
    uint16_t lo = (uint16_t)(addr & 0xFFFFu);
    uint16_t hi = (uint16_t)((addr >> 16) & 0xFFFFu);
    if (port_out8(h, (uint16_t)(base + 0x103), 0x43)) return 1;
    if (port_out16_bytes(h, (uint16_t)(base + 0x104), lo)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x103), 0x44)) return 1;
    if (port_out16_bytes(h, (uint16_t)(base + 0x104), hi)) return 1;
    return 0;
}

static int gus_dram_write8_2(HANDLE h, uint16_t base, uint32_t addr, uint8_t value)
{
    if (gus_dram_set_addr2(h, base, addr)) return 1;
    return port_out8(h, (uint16_t)(base + 0x107), value);
}

static int gus_dram_read8_2(HANDLE h, uint16_t base, uint32_t addr, uint8_t *value)
{
    if (gus_dram_set_addr2(h, base, addr)) return 1;
    return port_in8(h, (uint16_t)(base + 0x107), value);
}

static int gus_global_write16_2(HANDLE h, uint16_t base, uint8_t reg, uint16_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_out16_bytes(h, (uint16_t)(base + 0x104), value);
}

static int gus_global_write8_high_2(HANDLE h, uint16_t base, uint8_t reg, uint8_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_out8(h, (uint16_t)(base + 0x105), value);
}

static int gus_select_voice_2(HANDLE h, uint16_t base, uint8_t voice)
{
    return port_out8(h, (uint16_t)(base + 0x102), voice);
}

static uint16_t gus_lsw_addr_2(uint32_t addr) { return (uint16_t)(addr & 0xFFE0u); }
static uint16_t gus_msw_addr_2(uint32_t addr) { return (uint16_t)((addr >> 16) & 0x1FFFu); }

static int cmd_gus_dram_poke(HANDLE h, int ac, char **av)
{
    if (ac < 5) { puts("gus-dram-poke <gus_alias_base> <addr> <value>"); return 2; }
    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint8_t value = (uint8_t)u32(av[4]);
    if (gus_dram_write8_2(h, base, addr, value)) return 1;
    printf("gus-dram-poke base=0x%04x addr=0x%06x value=0x%02x\n", base, addr, value);
    return 0;
}

static int cmd_gus_dram_peek(HANDLE h, int ac, char **av)
{
    if (ac < 4) { puts("gus-dram-peek <gus_alias_base> <addr>"); return 2; }
    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint8_t value = 0;
    if (gus_dram_read8_2(h, base, addr, &value)) return 1;
    printf("gus-dram-peek base=0x%04x addr=0x%06x value=0x%02x\n", base, addr, value);
    return 0;
}

static int cmd_gus_dram_dump(HANDLE h, int ac, char **av)
{
    if (ac < 5) { puts("gus-dram-dump <gus_alias_base> <addr> <count>"); return 2; }
    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint32_t count = u32(av[4]);
    if (count > 4096) count = 4096;
    printf("gus-dram-dump base=0x%04x addr=0x%06x count=%u\n", base, addr, count);
    for (uint32_t row = 0; row < count; row += 16) {
        printf("%06x:", addr + row);
        char ascii[17]; memset(ascii, 0, sizeof(ascii));
        for (uint32_t i = 0; i < 16 && row + i < count; i++) {
            uint8_t v = 0;
            if (gus_dram_read8_2(h, base, addr + row + i, &v)) return 1;
            printf(" %02x", v);
            ascii[i] = (v >= 32 && v < 127) ? (char)v : '.';
        }
        printf("  |%s|\n", ascii);
    }
    return 0;
}

static int cmd_gus_dram_fill(HANDLE h, int ac, char **av)
{
    if (ac < 6) { puts("gus-dram-fill <gus_alias_base> <addr> <count> <value>"); return 2; }
    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint32_t count = u32(av[4]);
    uint8_t value = (uint8_t)u32(av[5]);
    if (count > 65536) count = 65536;
    if (gus_dram_set_addr2(h, base, addr)) return 1;
    for (uint32_t i = 0; i < count; i++) if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    printf("gus-dram-fill base=0x%04x addr=0x%06x count=%u value=0x%02x\n", base, addr, count, value);
    return 0;
}

static int cmd_gus_dram_ramp(HANDLE h, int ac, char **av)
{
    if (ac < 5) { puts("gus-dram-ramp <gus_alias_base> <addr> <count>"); return 2; }
    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint32_t count = u32(av[4]);
    if (count > 65536) count = 65536;
    if (gus_dram_set_addr2(h, base, addr)) return 1;
    for (uint32_t i = 0; i < count; i++) if (port_out8(h, (uint16_t)(base + 0x107), (uint8_t)(i & 0xFFu))) return 1;
    printf("gus-dram-ramp base=0x%04x addr=0x%06x count=%u\n", base, addr, count);
    return 0;
}

static int gus_program_voice_2(HANDLE h, uint16_t base, uint32_t addr, uint32_t count)
{
    uint32_t start = addr;
    uint32_t end = addr + ((count > 1) ? (count - 1) : 1);
    if (gus_select_voice_2(h, base, 0)) return 1;
    if (gus_global_write8_high_2(h, base, 0x00, 0x03)) return 1; /* stop voice */
    if (gus_global_write8_high_2(h, base, 0x0D, 0x03)) return 1; /* stop ramp */
    if (gus_global_write8_high_2(h, base, 0x0C, 0x08)) return 1; /* center pan */
    if (gus_global_write16_2(h, base, 0x09, 0xE000)) return 1;  /* volume */
    if (gus_global_write16_2(h, base, 0x02, gus_msw_addr_2(start))) return 1;
    if (gus_global_write16_2(h, base, 0x03, gus_lsw_addr_2(start))) return 1;
    if (gus_global_write16_2(h, base, 0x04, gus_msw_addr_2(end))) return 1;
    if (gus_global_write16_2(h, base, 0x05, gus_lsw_addr_2(end))) return 1;
    if (gus_global_write16_2(h, base, 0x0A, gus_msw_addr_2(start))) return 1;
    if (gus_global_write16_2(h, base, 0x0B, gus_lsw_addr_2(start))) return 1;
    if (gus_global_write16_2(h, base, 0x01, 0x0400)) return 1;  /* frequency trial */
    if (gus_global_write8_high_2(h, base, 0x00, 0x00)) return 1; /* run */
    return 0;
}

static int cmd_gus_voice_test(HANDLE h, int ac, char **av)
{
    if (ac < 3) { puts("gus-voice-test <gus_alias_base> [addr] [count] [mode]"); return 2; }
    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x0000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 256;
    uint32_t mode = (ac > 5) ? u32(av[5]) : 1; /* 0=flat 0x80, 1=ramp */
    if (count < 32) count = 32;
    if (count > 65536) count = 65536;
    if (gus_dram_set_addr2(h, base, addr)) return 1;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t v = mode ? (uint8_t)(i & 0xFFu) : 0x80u;
        if (port_out8(h, (uint16_t)(base + 0x107), v)) return 1;
    }
    if (gus_program_voice_2(h, base, addr, count)) return 1;
    printf("gus-voice-test base=0x%04x addr=0x%06x count=%u mode=%u\n", base, addr, count, mode);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Stronger GUS voice loop / audible square-wave tests                        */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* VOICE_MISSING_HELPERS_FIX: helper definitions required by voice loop test   */
/* ------------------------------------------------------------------------- */

/* GUS_DRAM_20BIT_ADDR_PATCH */
/* GUS_DRAM_HIGH_ADDR_MODE3_CONFIRMED */
static int gus_dram_set_addr(HANDLE h, uint16_t base, uint32_t addr)
{
    /*
        Confirmed by gus-dram-hiaddr-mode-sweep-safe:

          MODE 3 works:
            0x43 low16, then 0x44 with high address byte in HIGH byte.

        Readback after mode 3:
            000000 -> 11
            008000 -> 22
            010000 -> 33
            018000 -> 44
            020000 -> 55

        So:
          reg 0x43 = addr[15:0]
          reg 0x44 high byte = addr[23:16]
          reg 0x44 low byte  = 0
    */
    uint16_t lo16 = (uint16_t)(addr & 0xFFFFu);
    uint8_t hi8 = (uint8_t)((addr >> 16) & 0xFFu);

    if (port_out8(h, (uint16_t)(base + 0x103), 0x43)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x104), (uint8_t)(lo16 & 0xFFu))) return 1;
    if (port_out8(h, (uint16_t)(base + 0x105), (uint8_t)((lo16 >> 8) & 0xFFu))) return 1;

    if (port_out8(h, (uint16_t)(base + 0x103), 0x44)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x104), 0x00)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x105), hi8)) return 1;

    return 0;
}

static int gus_global_write16(HANDLE h, uint16_t base, uint8_t reg, uint16_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_out16_bytes(h, (uint16_t)(base + 0x104), value);
}

static int gus_global_write8_high(HANDLE h, uint16_t base, uint8_t reg, uint8_t value)
{
    /*
        Many byte-like GF1 global regs are consumed from the high byte of the
        selected register data path, i.e. base+0x105.
    */
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    return port_out8(h, (uint16_t)(base + 0x105), value);
}

static int gus_select_voice(HANDLE h, uint16_t base, uint8_t voice)
{
    /*
        Current voice select is base+0x102.
        For alias base 0x8240 -> 0x8342.
    */
    return port_out8(h, (uint16_t)(base + 0x102), voice);
}

static uint16_t gus_lsw_addr(uint32_t addr)
{
    /*
        GF1 wave address low register ignores low 5 fractional bits in the
        model we are matching, so keep bits 15:5.
    */
    return (uint16_t)(addr & 0xFFE0u);
}

static uint16_t gus_msw_addr(uint32_t addr)
{
    return (uint16_t)((addr >> 16) & 0x1FFFu);
}
static int gus_global_read16_diag(HANDLE h, uint16_t base, uint8_t reg, uint16_t *value)
{
    uint8_t lo = 0, hi = 0;
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi)) return 1;
    *value = (uint16_t)(lo | ((uint16_t)hi << 8));
    return 0;
}

static int gus_enable_dac_sequence(HANDLE h, uint16_t base)
{
    uint8_t v = 0;

    if (gus_global_write8_high(h, base, 0x4C, 0x00)) return 1;
    Sleep(20);

    if (gus_global_write8_high(h, base, 0x0E, 0x0D)) return 1;

    if (gus_global_write8_high(h, base, 0x4C, 0x01)) return 1;
    Sleep(20);
    if (gus_global_write8_high(h, base, 0x4C, 0x07)) return 1;

    if (port_out8(h, (uint16_t)(base + 0x103), 0x4C)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &v)) return 1;

    printf("gus-enable-dac reset-readback=0x%02x\n", v);
    return 0;
}

static int cmd_gus_dram_square(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-dram-square <base> <addr> <count> [low] [high] [period]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint32_t count = u32(av[4]);
    uint8_t low = (ac > 5) ? (uint8_t)u32(av[5]) : 0x00;
    uint8_t high = (ac > 6) ? (uint8_t)u32(av[6]) : 0xFF;
    uint32_t period = (ac > 7) ? u32(av[7]) : 32;

    if (period < 2) period = 2;
    if (count > 262144) count = 262144;

    if (gus_dram_set_addr(h, base, addr)) return 1;

    for (uint32_t i = 0; i < count; i++) {
        uint8_t v = ((i / period) & 1) ? high : low;
        if (port_out8(h, (uint16_t)(base + 0x107), v)) return 1;
    }

    printf("gus-dram-square base=0x%04x addr=0x%06x count=%u low=0x%02x high=0x%02x period=%u\n",
           base, addr, count, low, high, period);
    return 0;
}

static int gus_voice_dump_one(HANDLE h, uint16_t base, uint8_t voice)
{
    static const uint8_t regs[] = {
        0x80,0x81,0x82,0x83,0x84,0x85,0x89,0x8A,0x8B,0x8C,0x8D
    };
    static const char *names[] = {
        "ctrl","freq","start_msw","start_lsw","end_msw","end_lsw",
        "vol","cur_msw","cur_lsw","pan","volctrl"
    };

    if (gus_select_voice(h, base, voice)) return 1;

    printf("gus-voice-dump base=0x%04x voice=%u\n", base, voice);
    for (unsigned i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) {
        uint16_t v = 0;
        if (gus_global_read16_diag(h, base, regs[i], &v)) return 1;
        printf("  reg 0x%02x %-9s = 0x%04x\n", regs[i], names[i], v);
    }
    return 0;
}

static int cmd_gus_voice_dump(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-voice-dump <base> [voice]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t voice = (ac > 3) ? (uint8_t)u32(av[3]) : 0;

    return gus_voice_dump_one(h, base, voice);
}

static int cmd_gus_voice_stop(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-voice-stop <base> [voice]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint8_t voice = (ac > 3) ? (uint8_t)u32(av[3]) : 0;

    if (gus_select_voice(h, base, voice)) return 1;
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    printf("gus-voice-stop base=0x%04x voice=%u\n", base, voice);
    return 0;
}

static int cmd_gus_voice_loop_test(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-voice-loop-test <base> [addr] [count] [freq] [pattern]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x4000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 32768;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0800;
    const char *pattern = (ac > 6) ? av[6] : "square";

    if (count < 64) count = 64;
    if (count > 262144) count = 262144;

    if (gus_enable_dac_sequence(h, base)) return 1;

    if (gus_dram_set_addr(h, base, addr)) return 1;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t v;
        if (!_stricmp(pattern, "ramp")) {
            v = (uint8_t)(i & 0xFFu);
        } else if (!_stricmp(pattern, "alt")) {
            v = (i & 1) ? 0xFF : 0x00;
        } else {
            v = ((i / 32u) & 1u) ? 0xFF : 0x00;
        }
        if (port_out8(h, (uint16_t)(base + 0x107), v)) return 1;
    }

    uint32_t start = addr;
    uint32_t end = addr + count - 1;

    if (gus_select_voice(h, base, 0)) return 1;

    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    if (gus_global_write8_high(h, base, 0x0C, 0x08)) return 1;
    if (gus_global_write16(h, base, 0x09, 0xF000)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x00)) return 1;

    if (gus_global_write16(h, base, 0x02, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x03, gus_lsw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x04, gus_msw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x05, gus_lsw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x0A, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x0B, gus_lsw_addr(start))) return 1;

    if (gus_global_write16(h, base, 0x01, freq)) return 1;

    if (gus_global_write8_high(h, base, 0x00, 0x08)) return 1;

    printf("gus-voice-loop-test base=0x%04x addr=0x%06x count=%u freq=0x%04x pattern=%s\n",
           base, addr, count, freq, pattern);

    if (gus_voice_dump_one(h, base, 0)) return 1;
    Sleep(200);
    puts("after 200ms:");
    if (gus_voice_dump_one(h, base, 0)) return 1;

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Known-good audible PicoGUS voice test                                      */
/* ------------------------------------------------------------------------- */

static int cmd_gus_audible_test(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-audible-test <base> [addr] [count] [freq]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x4000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 32768;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0800;

    if (count < 256) count = 256;
    if (count > 262144) count = 262144;

    /*
        This follows the manual sequence that produced sound:
          - DAC reset sequence to 0x07
          - load a loud-ish alternating/ramp-ish pattern
          - stop wave/ramp
          - set current address
          - set current volume
          - keep volume ramp stopped
          - start looped voice
    */
    if (gus_enable_dac_sequence(h, base)) return 1;

    if (gus_dram_set_addr(h, base, addr)) return 1;
    for (uint32_t i = 0; i < count; i++) {
        /*
            Loud periodic pattern. Period 32 avoids ultrasonic-only output.
            Use 0x00/0xff around unsigned midpoint.
        */
        uint8_t v = ((i / 32u) & 1u) ? 0xFF : 0x00;
        if (port_out8(h, (uint16_t)(base + 0x107), v)) return 1;
    }

    uint32_t start = addr;
    uint32_t end = addr + count - 1;

    if (gus_select_voice(h, base, 0)) return 1;

    /* Stop voice and volume ramp before programming. */
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Address window. */
    if (gus_global_write16(h, base, 0x02, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x03, gus_lsw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x04, gus_msw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x05, gus_lsw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x0A, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x0B, gus_lsw_addr(start))) return 1;

    /* Center pan, frequency, loud current volume. */
    if (gus_global_write8_high(h, base, 0x0C, 0x08)) return 1;
    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write16(h, base, 0x09, 0xF000)) return 1;

    /*
        Critical fix: leave volume ramp stopped AFTER writing volume.
        Manual proof: 0x89 stayed 0xF000, 0x8D stayed 0x0300, and sound was heard.
    */
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Start voice with loop bit set, stop bit clear. */
    if (gus_global_write8_high(h, base, 0x00, 0x08)) return 1;

    printf("gus-audible-test base=0x%04x addr=0x%06x count=%u freq=0x%04x\n",
           base, addr, count, freq);

    if (gus_voice_dump_one(h, base, 0)) return 1;
    Sleep(200);
    puts("after 200ms:");
    if (gus_voice_dump_one(h, base, 0)) return 1;

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Tiny WAV loader/player for PicoGUS                                         */
/* ------------------------------------------------------------------------- */

typedef struct GUS_WAV_INFO_ {
    uint16_t format_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} GUS_WAV_INFO;

static uint16_t rd_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int wav_read_info(const char *path, GUS_WAV_INFO *wi)
{
    FILE *f = NULL;
    uint8_t hdr[12];

    memset(wi, 0, sizeof(*wi));

    f = fopen(path, "rb");
    if (!f) {
        printf("open wav failed: %s\n", path);
        return 1;
    }

    if (fread(hdr, 1, 12, f) != 12) {
        fclose(f);
        puts("wav: short RIFF header");
        return 1;
    }

    if (memcmp(hdr + 0, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        fclose(f);
        puts("wav: not RIFF/WAVE");
        return 1;
    }

    int have_fmt = 0;
    int have_data = 0;

    for (;;) {
        uint8_t chdr[8];
        uint32_t csz;
        long payload_pos;

        if (fread(chdr, 1, 8, f) != 8) break;

        csz = rd_le32(chdr + 4);
        payload_pos = ftell(f);

        if (!memcmp(chdr, "fmt ", 4)) {
            uint8_t fmt[40];
            uint32_t take = csz < sizeof(fmt) ? csz : (uint32_t)sizeof(fmt);
            memset(fmt, 0, sizeof(fmt));
            if (fread(fmt, 1, take, f) != take) {
                fclose(f);
                puts("wav: short fmt chunk");
                return 1;
            }

            wi->format_tag = rd_le16(fmt + 0);
            wi->channels = rd_le16(fmt + 2);
            wi->sample_rate = rd_le32(fmt + 4);
            wi->bits_per_sample = rd_le16(fmt + 14);
            have_fmt = 1;
        } else if (!memcmp(chdr, "data", 4)) {
            wi->data_offset = (uint32_t)payload_pos;
            wi->data_size = csz;
            have_data = 1;
        }

        if (fseek(f, payload_pos + (long)csz + (long)(csz & 1u), SEEK_SET) != 0) {
            fclose(f);
            puts("wav: seek failed");
            return 1;
        }

        if (have_fmt && have_data) break;
    }

    fclose(f);

    if (!have_fmt || !have_data) {
        puts("wav: missing fmt or data chunk");
        return 1;
    }

    if (wi->format_tag != 1) {
        printf("wav: unsupported format tag %u; only PCM=1 supported\n", wi->format_tag);
        return 1;
    }

    if (!(wi->channels == 1 || wi->channels == 2)) {
        printf("wav: unsupported channel count %u; only mono/stereo supported\n", wi->channels);
        return 1;
    }

    if (!(wi->bits_per_sample == 8 || wi->bits_per_sample == 16)) {
        printf("wav: unsupported bits/sample %u; only 8/16 supported\n", wi->bits_per_sample);
        return 1;
    }

    return 0;
}

static uint32_t wav_estimate_out_samples(const GUS_WAV_INFO *wi)
{
    uint32_t bytes_per_frame = (uint32_t)wi->channels * ((uint32_t)wi->bits_per_sample / 8u);
    if (!bytes_per_frame) return 0;
    return wi->data_size / bytes_per_frame;
}

static int wav_convert_to_u8(const char *path, const GUS_WAV_INFO *wi, uint8_t **out_buf, uint32_t *out_count, uint32_t max_samples)
{
    FILE *f = NULL;
    uint32_t frames = wav_estimate_out_samples(wi);
    uint32_t n;
    uint8_t *dst = NULL;

    if (frames == 0) {
        puts("wav: zero frames");
        return 1;
    }

    n = frames;
    if (max_samples && n > max_samples) n = max_samples;
    if (n > 262144) n = 262144;

    dst = (uint8_t *)malloc(n);
    if (!dst) {
        puts("malloc failed");
        return 1;
    }

    f = fopen(path, "rb");
    if (!f) {
        free(dst);
        printf("open wav failed: %s\n", path);
        return 1;
    }

    if (fseek(f, (long)wi->data_offset, SEEK_SET) != 0) {
        fclose(f);
        free(dst);
        puts("wav: seek data failed");
        return 1;
    }

    for (uint32_t i = 0; i < n; i++) {
        if (wi->bits_per_sample == 8) {
            int a = fgetc(f);
            int b = a;

            if (a < 0) {
                n = i;
                break;
            }

            if (wi->channels == 2) {
                b = fgetc(f);
                if (b < 0) b = a;
                dst[i] = (uint8_t)(((unsigned)a + (unsigned)b) >> 1);
            } else {
                dst[i] = (uint8_t)a;
            }
        } else {
            uint8_t s[4];
            int32_t a, b;

            if (wi->channels == 1) {
                if (fread(s, 1, 2, f) != 2) {
                    n = i;
                    break;
                }
                a = (int16_t)rd_le16(s);
                dst[i] = (uint8_t)(((a >> 8) + 128) & 0xFF);
            } else {
                if (fread(s, 1, 4, f) != 4) {
                    n = i;
                    break;
                }
                a = (int16_t)rd_le16(s + 0);
                b = (int16_t)rd_le16(s + 2);
                a = (a + b) / 2;
                dst[i] = (uint8_t)(((a >> 8) + 128) & 0xFF);
            }
        }
    }

    fclose(f);

    *out_buf = dst;
    *out_count = n;
    return 0;
}

static int gus_upload_u8_buffer(HANDLE h, uint16_t base, uint32_t addr, const uint8_t *buf, uint32_t count)
{
    if (gus_dram_set_addr(h, base, addr)) return 1;

    for (uint32_t i = 0; i < count; i++) {
        if (port_out8(h, (uint16_t)(base + 0x107), buf[i])) return 1;
    }

    return 0;
}

static int gus_start_voice_known_good(HANDLE h, uint16_t base, uint32_t addr, uint32_t count, uint16_t freq)
{
    uint32_t start = addr;
    uint32_t end = addr + count - 1;

    if (gus_enable_dac_sequence(h, base)) return 1;

    if (gus_select_voice(h, base, 0)) return 1;

    /* Stop voice and volume ramp before programming. */
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Address window and current position. */
    if (gus_global_write16(h, base, 0x02, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x03, gus_lsw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x04, gus_msw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x05, gus_lsw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x0A, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x0B, gus_lsw_addr(start))) return 1;

    /* Center pan, playback freq, loud volume. */
    if (gus_global_write8_high(h, base, 0x0C, 0x08)) return 1;
    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write16(h, base, 0x09, 0xF000)) return 1;

    /* Critical: keep volume ramp stopped AFTER writing volume. */
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Start looped voice. */
    if (gus_global_write8_high(h, base, 0x00, 0x08)) return 1;

    return 0;
}

static uint16_t wav_guess_gus_freq(uint32_t sample_rate)
{
    /*
        Rough starting points. We already know 0x0800 is audible and safe.
        This is not exact GF1 math; it is just a convenience so lower-rate
        files do not all scream at the same pitch.
    */
    if (sample_rate <= 11025) return 0x0400;
    if (sample_rate <= 22050) return 0x0800;
    if (sample_rate <= 32000) return 0x0C00;
    return 0x1000;
}

static int cmd_gus_wav_info(HANDLE h, int ac, char **av)
{
    (void)h;

    if (ac < 3) {
        puts("gus-wav-info <wavfile>");
        return 2;
    }

    GUS_WAV_INFO wi;
    if (wav_read_info(av[2], &wi)) return 1;

    printf("gus-wav-info file=%s\n", av[2]);
    printf("  format=%u channels=%u sample_rate=%u bits=%u data_offset=%u data_size=%u frames=%u suggested_freq=0x%04x\n",
           wi.format_tag, wi.channels, wi.sample_rate, wi.bits_per_sample,
           wi.data_offset, wi.data_size, wav_estimate_out_samples(&wi),
           wav_guess_gus_freq(wi.sample_rate));
    return 0;
}

static int cmd_gus_wav_load(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-wav-load <base> <wavfile> <addr> [max_bytes]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t addr = u32(av[4]);
    uint32_t max_bytes = (ac > 5) ? u32(av[5]) : 65536;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_to_u8(path, &wi, &buf, &n, max_bytes)) return 1;

    if (gus_upload_u8_buffer(h, base, addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    printf("gus-wav-load base=0x%04x file=%s addr=0x%06x bytes=%u src_rate=%u src_bits=%u src_ch=%u\n",
           base, path, addr, n, wi.sample_rate, wi.bits_per_sample, wi.channels);
    return 0;
}


/* ------------------------------------------------------------------------- */
/* WAV playback fix: start voice without resetting GF1 after upload            */
/* ------------------------------------------------------------------------- */

static int gus_start_voice_no_reset_known_good(HANDLE h, uint16_t base, uint32_t addr, uint32_t count, uint16_t freq)
{
    uint32_t start = addr;
    uint32_t end = addr + count - 1;

    if (gus_select_voice(h, base, 0)) return 1;

    /* Stop voice and volume ramp before programming. */
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Address window and current position. */
    if (gus_global_write16(h, base, 0x02, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x03, gus_lsw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x04, gus_msw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x05, gus_lsw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x0A, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x0B, gus_lsw_addr(start))) return 1;

    /* Center pan, playback freq, loud current volume. */
    if (gus_global_write8_high(h, base, 0x0C, 0x08)) return 1;
    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write16(h, base, 0x09, 0xF000)) return 1;

    /* Critical: keep volume ramp stopped AFTER writing volume. */
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Start looped voice. */
    if (gus_global_write8_high(h, base, 0x00, 0x08)) return 1;

    return 0;
}

static void gus_print_u8_preview(const char *label, const uint8_t *buf, uint32_t n)
{
    uint32_t m = n < 16 ? n : 16;
    printf("%s", label);
    for (uint32_t i = 0; i < m; i++) {
        printf(" %02x", buf[i]);
    }
    printf("\n");
}
static int cmd_gus_wav_play(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play <base> <wavfile> [addr] [max_bytes] [freq]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t addr = (ac > 4) ? u32(av[4]) : 0x8000;
    uint32_t max_bytes = (ac > 5) ? u32(av[5]) : 65536;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint16_t freq;

    if (wav_read_info(path, &wi)) return 1;
    freq = (ac > 6) ? (uint16_t)u32(av[6]) : wav_guess_gus_freq(wi.sample_rate);

    if (wav_convert_to_u8(path, &wi, &buf, &n, max_bytes)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    /*
        Important order:
          enable/reset DAC BEFORE upload, then do not reset GF1 again after
          DRAM upload. The old version uploaded first, then reset in
          gus_start_voice_known_good(), which could leave us hearing old data.
    */
    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    gus_print_u8_preview("converted first bytes:", buf, n);

    if (gus_upload_u8_buffer(h, base, addr, buf, n)) {
        free(buf);
        return 1;
    }

    /*
        Write one guard byte after the sample so the end does not read stale
        data on exact loop boundary.
    */
    {
        uint8_t mid = 0x80;
        (void)gus_upload_u8_buffer(h, base, addr + n, &mid, 1);
    }

    free(buf);

    if (gus_start_voice_no_reset_known_good(h, base, addr, n, freq)) return 1;

    printf("gus-wav-play base=0x%04x file=%s addr=0x%06x bytes=%u src_rate=%u src_bits=%u src_ch=%u freq=0x%04x no-post-upload-reset=1\n",
           base, path, addr, n, wi.sample_rate, wi.bits_per_sample, wi.channels, freq);

    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Safe per-byte GUS DRAM upload helpers                                      */
/* ------------------------------------------------------------------------- */

static int gus_upload_u8_buffer_safe(HANDLE h, uint16_t base, uint32_t addr, const uint8_t *buf, uint32_t count)
{
    /*
        Slow but reliable path:
          set GUS DRAM address for every byte, then write the data port.

        The streaming path (set address once, write base+0x107 repeatedly)
        produced address-like/ramp-like contents over this bridge:
          80 01 02 03 ...
        while single-byte poke/peek worked. So use the proven single-byte path.
    */
    for (uint32_t i = 0; i < count; i++) {
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), buf[i])) return 1;
    }

    return 0;
}

static int cmd_gus_dram_fill_safe(HANDLE h, int ac, char **av)
{
    if (ac < 6) {
        puts("gus-dram-fill-safe <base> <addr> <count> <value>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint32_t count = u32(av[4]);
    uint8_t value = (uint8_t)u32(av[5]);

    if (count > 262144) count = 262144;

    for (uint32_t i = 0; i < count; i++) {
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }

    printf("gus-dram-fill-safe base=0x%04x addr=0x%06x count=%u value=0x%02x\n",
           base, addr, count, value);
    return 0;
}

static int cmd_gus_dram_square_safe(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-dram-square-safe <base> <addr> <count> [low] [high] [period]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint32_t count = u32(av[4]);
    uint8_t low = (ac > 5) ? (uint8_t)u32(av[5]) : 0x00;
    uint8_t high = (ac > 6) ? (uint8_t)u32(av[6]) : 0xFF;
    uint32_t period = (ac > 7) ? u32(av[7]) : 16;

    if (period < 2) period = 2;
    if (count > 262144) count = 262144;

    for (uint32_t i = 0; i < count; i++) {
        uint8_t v = ((i / period) & 1u) ? high : low;
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), v)) return 1;
    }

    printf("gus-dram-square-safe base=0x%04x addr=0x%06x count=%u low=0x%02x high=0x%02x period=%u\n",
           base, addr, count, low, high, period);
    return 0;
}

static int cmd_gus_wav_load_safe(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-wav-load-safe <base> <wavfile> <addr> [max_bytes]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t addr = u32(av[4]);
    uint32_t max_bytes = (ac > 5) ? u32(av[5]) : 65536;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_to_u8(path, &wi, &buf, &n, max_bytes)) return 1;

    gus_print_u8_preview("converted first bytes:", buf, n);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    printf("gus-wav-load-safe base=0x%04x file=%s addr=0x%06x bytes=%u src_rate=%u src_bits=%u src_ch=%u\n",
           base, path, addr, n, wi.sample_rate, wi.bits_per_sample, wi.channels);
    return 0;
}

static int cmd_gus_wav_play_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-safe <base> <wavfile> [addr] [max_bytes] [freq]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t addr = (ac > 4) ? u32(av[4]) : 0x8000;
    uint32_t max_bytes = (ac > 5) ? u32(av[5]) : 65536;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint16_t freq;

    if (wav_read_info(path, &wi)) return 1;
    freq = (ac > 6) ? (uint16_t)u32(av[6]) : wav_guess_gus_freq(wi.sample_rate);

    if (wav_convert_to_u8(path, &wi, &buf, &n, max_bytes)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    gus_print_u8_preview("converted first bytes:", buf, n);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_known_good(h, base, addr, n, freq)) return 1;

    printf("gus-wav-play-safe base=0x%04x file=%s addr=0x%06x bytes=%u src_rate=%u src_bits=%u src_ch=%u freq=0x%04x safe-upload=1\n",
           base, path, addr, n, wi.sample_rate, wi.bits_per_sample, wi.channels, freq);

    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Explicit safe WAV one-shot / loop playback modes                           */
/* ------------------------------------------------------------------------- */

static int gus_start_voice_no_reset_with_ctrl(HANDLE h, uint16_t base, uint32_t addr, uint32_t count, uint16_t freq, uint8_t voice_ctrl)
{
    uint32_t start = addr;
    uint32_t end = addr + count - 1;

    if (gus_select_voice(h, base, 0)) return 1;

    /* Stop voice and volume ramp before programming. */
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Address window and current position. */
    if (gus_global_write16(h, base, 0x02, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x03, gus_lsw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x04, gus_msw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x05, gus_lsw_addr(end))) return 1;
    if (gus_global_write16(h, base, 0x0A, gus_msw_addr(start))) return 1;
    if (gus_global_write16(h, base, 0x0B, gus_lsw_addr(start))) return 1;

    /* Center pan, playback freq, loud current volume. */
    if (gus_global_write8_high(h, base, 0x0C, 0x08)) return 1;
    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write16(h, base, 0x09, 0xF000)) return 1;

    /* Critical: keep volume ramp stopped AFTER writing volume. */
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /*
        voice_ctrl:
          0x00 = run once, no loop
          0x08 = loop, stop bit clear
    */
    if (gus_global_write8_high(h, base, 0x00, voice_ctrl)) return 1;

    return 0;
}

static int gus_wav_play_safe_mode(HANDLE h, int ac, char **av, int loop)
{
    if (ac < 4) {
        puts(loop ?
             "gus-wav-play-loop-safe <base> <wavfile> [addr] [max_bytes] [freq]" :
             "gus-wav-play-once-safe <base> <wavfile> [addr] [max_bytes] [freq]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t addr = (ac > 4) ? u32(av[4]) : 0x8000;
    uint32_t max_bytes = (ac > 5) ? u32(av[5]) : 65536;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint16_t freq;

    if (wav_read_info(path, &wi)) return 1;
    freq = (ac > 6) ? (uint16_t)u32(av[6]) : wav_guess_gus_freq(wi.sample_rate);

    if (wav_convert_to_u8(path, &wi, &buf, &n, max_bytes)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    gus_print_u8_preview("converted first bytes:", buf, n);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, addr, n, freq, loop ? 0x08 : 0x00)) return 1;

    printf("%s base=0x%04x file=%s addr=0x%06x bytes=%u src_rate=%u src_bits=%u src_ch=%u freq=0x%04x ctrl=0x%02x\n",
           loop ? "gus-wav-play-loop-safe" : "gus-wav-play-once-safe",
           base, path, addr, n, wi.sample_rate, wi.bits_per_sample, wi.channels,
           freq, loop ? 0x08 : 0x00);

    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

static int cmd_gus_wav_play_once_safe(HANDLE h, int ac, char **av)
{
    return gus_wav_play_safe_mode(h, ac, av, 0);
}

static int cmd_gus_wav_play_loop_safe(HANDLE h, int ac, char **av)
{
    return gus_wav_play_safe_mode(h, ac, av, 1);
}

/* ------------------------------------------------------------------------- */
/* WAV source-window + gain conversion/playback                               */
/* ------------------------------------------------------------------------- */

static uint8_t wav_apply_gain_u8(uint8_t x, uint32_t gain_x100)
{
    int v = (int)x - 128;
    v = (v * (int)gain_x100) / 100;
    v += 128;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

static int wav_convert_window_to_u8_gain(
    const char *path,
    const GUS_WAV_INFO *wi,
    uint32_t sample_offset,
    uint32_t max_samples,
    uint32_t gain_x100,
    uint8_t **out_buf,
    uint32_t *out_count)
{
    FILE *f = NULL;
    uint32_t total_frames = wav_estimate_out_samples(wi);
    uint32_t bytes_per_frame = (uint32_t)wi->channels * ((uint32_t)wi->bits_per_sample / 8u);
    uint32_t n;
    uint8_t *dst = NULL;

    if (gain_x100 == 0) gain_x100 = 100;
    if (bytes_per_frame == 0 || total_frames == 0) {
        puts("wav: no frames");
        return 1;
    }

    if (sample_offset >= total_frames) {
        printf("wav: sample_offset %u beyond total frames %u\n", sample_offset, total_frames);
        return 1;
    }

    n = total_frames - sample_offset;
    if (max_samples && n > max_samples) n = max_samples;
    if (n > 262144) n = 262144;

    dst = (uint8_t *)malloc(n);
    if (!dst) {
        puts("malloc failed");
        return 1;
    }

    f = fopen(path, "rb");
    if (!f) {
        free(dst);
        printf("open wav failed: %s\n", path);
        return 1;
    }

    if (fseek(f, (long)wi->data_offset + (long)((uint64_t)sample_offset * bytes_per_frame), SEEK_SET) != 0) {
        fclose(f);
        free(dst);
        puts("wav: seek data window failed");
        return 1;
    }

    for (uint32_t i = 0; i < n; i++) {
        uint8_t outv = 0x80;

        if (wi->bits_per_sample == 8) {
            int a = fgetc(f);
            int b = a;

            if (a < 0) {
                n = i;
                break;
            }

            if (wi->channels == 2) {
                b = fgetc(f);
                if (b < 0) b = a;
                outv = (uint8_t)(((unsigned)a + (unsigned)b) >> 1);
            } else {
                outv = (uint8_t)a;
            }
        } else {
            uint8_t s[4];
            int32_t a, b;

            if (wi->channels == 1) {
                if (fread(s, 1, 2, f) != 2) {
                    n = i;
                    break;
                }
                a = (int16_t)rd_le16(s);
                outv = (uint8_t)(((a >> 8) + 128) & 0xFF);
            } else {
                if (fread(s, 1, 4, f) != 4) {
                    n = i;
                    break;
                }
                a = (int16_t)rd_le16(s + 0);
                b = (int16_t)rd_le16(s + 2);
                a = (a + b) / 2;
                outv = (uint8_t)(((a >> 8) + 128) & 0xFF);
            }
        }

        dst[i] = wav_apply_gain_u8(outv, gain_x100);
    }

    fclose(f);

    *out_buf = dst;
    *out_count = n;
    return 0;
}

static void wav_analyze_u8_buffer(const uint8_t *buf, uint32_t n)
{
    uint8_t mn = 255, mx = 0;
    uint64_t sum = 0;
    uint64_t abs_sum = 0;
    uint32_t zc = 0;
    int prev = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint8_t v = buf[i];
        int centered = (int)v - 128;

        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
        abs_sum += (uint64_t)(centered < 0 ? -centered : centered);

        if (i > 0) {
            if ((prev < 0 && centered >= 0) || (prev >= 0 && centered < 0)) zc++;
        }
        prev = centered;
    }

    printf("analysis: samples=%u min=0x%02x max=0x%02x avg=%u avg_abs_from_0x80=%u zero_cross=%u\n",
           n, mn, mx,
           n ? (unsigned)(sum / n) : 0,
           n ? (unsigned)(abs_sum / n) : 0,
           zc);
}

static int cmd_gus_wav_analyze_window(HANDLE h, int ac, char **av)
{
    (void)h;

    if (ac < 3) {
        puts("gus-wav-analyze-window <wavfile> [sample_offset] [max_samples] [gain_x100]");
        return 2;
    }

    const char *path = av[2];
    uint32_t sample_offset = (ac > 3) ? u32(av[3]) : 0;
    uint32_t max_samples = (ac > 4) ? u32(av[4]) : 65536;
    uint32_t gain_x100 = (ac > 5) ? u32(av[5]) : 100;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, max_samples, gain_x100, &buf, &n)) return 1;

    printf("gus-wav-analyze-window file=%s offset=%u max=%u gain=%u src_rate=%u bits=%u ch=%u\n",
           path, sample_offset, max_samples, gain_x100,
           wi.sample_rate, wi.bits_per_sample, wi.channels);

    gus_print_u8_preview("converted first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    free(buf);
    return 0;
}

static int cmd_gus_wav_play_window_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-window-safe <base> <wavfile> [dram_addr] [sample_offset] [max_samples] [freq] [gain_x100] [loop]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t dram_addr = (ac > 4) ? u32(av[4]) : 0x8000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 0;
    uint32_t max_samples = (ac > 6) ? u32(av[6]) : 65536;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint16_t freq;
    uint32_t gain_x100;
    int loop;

    if (wav_read_info(path, &wi)) return 1;

    freq = (ac > 7) ? (uint16_t)u32(av[7]) : wav_guess_gus_freq(wi.sample_rate);
    gain_x100 = (ac > 8) ? u32(av[8]) : 100;
    loop = (ac > 9) ? (int)u32(av[9]) : 1;

    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, max_samples, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play-window-safe file=%s src_offset=%u dram=0x%06x samples=%u freq=0x%04x gain=%u loop=%d\n",
           path, sample_offset, dram_addr, n, freq, gain_x100, loop);

    gus_print_u8_preview("converted first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, dram_addr, n, freq, loop ? 0x08 : 0x00)) return 1;

    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Signed 8-bit GUS sample-mode experiments                                   */
/* ------------------------------------------------------------------------- */

static void wav_xor_unsigned_to_signed_inplace(uint8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        buf[i] ^= 0x80;
    }
}


/* ------------------------------------------------------------------------- */
/* SIGNED_MISSING_DRAM_RW_FIX: safe single-byte GUS DRAM read/write wrappers   */
/* ------------------------------------------------------------------------- */

static int gus_dram_read8(HANDLE h, uint16_t base, uint32_t addr, uint8_t *value)
{
    if (gus_dram_set_addr(h, base, addr)) return 1;
    return port_in8(h, (uint16_t)(base + 0x107), value);
}

static int gus_dram_write8(HANDLE h, uint16_t base, uint32_t addr, uint8_t value)
{
    if (gus_dram_set_addr(h, base, addr)) return 1;
    return port_out8(h, (uint16_t)(base + 0x107), value);
}
static int cmd_gus_wav_analyze_window_signed(HANDLE h, int ac, char **av)
{
    (void)h;

    if (ac < 3) {
        puts("gus-wav-analyze-window-signed <wavfile> [sample_offset] [max_samples] [gain_x100]");
        return 2;
    }

    const char *path = av[2];
    uint32_t sample_offset = (ac > 3) ? u32(av[3]) : 0;
    uint32_t max_samples = (ac > 4) ? u32(av[4]) : 65536;
    uint32_t gain_x100 = (ac > 5) ? u32(av[5]) : 100;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, max_samples, gain_x100, &buf, &n)) return 1;

    printf("unsigned/u8 preview before xor:\n");
    gus_print_u8_preview("  unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    wav_xor_unsigned_to_signed_inplace(buf, n);

    printf("signed/GUS preview after xor 0x80:\n");
    gus_print_u8_preview("  signed first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    printf("gus-wav-analyze-window-signed file=%s offset=%u max=%u gain=%u src_rate=%u bits=%u ch=%u\n",
           path, sample_offset, max_samples, gain_x100,
           wi.sample_rate, wi.bits_per_sample, wi.channels);

    free(buf);
    return 0;
}

static int cmd_gus_wav_play_window_signed_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-window-signed-safe <base> <wavfile> [dram_addr] [sample_offset] [max_samples] [freq] [gain_x100] [loop]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t dram_addr = (ac > 4) ? u32(av[4]) : 0x8000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 0;
    uint32_t max_samples = (ac > 6) ? u32(av[6]) : 65536;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint16_t freq;
    uint32_t gain_x100;
    int loop;

    if (wav_read_info(path, &wi)) return 1;

    freq = (ac > 7) ? (uint16_t)u32(av[7]) : wav_guess_gus_freq(wi.sample_rate);
    gain_x100 = (ac > 8) ? u32(av[8]) : 100;
    loop = (ac > 9) ? (int)u32(av[9]) : 1;

    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, max_samples, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    printf("unsigned/u8 converted before xor:\n");
    gus_print_u8_preview("  unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    wav_xor_unsigned_to_signed_inplace(buf, n);

    printf("signed/GUS converted after xor 0x80:\n");
    gus_print_u8_preview("  signed first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, dram_addr, n, freq, loop ? 0x08 : 0x00)) return 1;

    printf("gus-wav-play-window-signed-safe file=%s src_offset=%u dram=0x%06x samples=%u freq=0x%04x gain=%u loop=%d signed_xor=1\n",
           path, sample_offset, dram_addr, n, freq, gain_x100, loop);

    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

static int cmd_gus_dram_unsigned_to_signed(HANDLE h, int ac, char **av)
{
    if (ac < 5) {
        puts("gus-dram-unsigned-to-signed <base> <addr> <count>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = u32(av[3]);
    uint32_t count = u32(av[4]);

    if (count > 262144) count = 262144;

    for (uint32_t i = 0; i < count; i++) {
        uint8_t v = 0;
        if (gus_dram_read8(h, base, addr + i, &v)) return 1;
        v ^= 0x80;
        if (gus_dram_write8(h, base, addr + i, v)) return 1;
    }

    printf("gus-dram-unsigned-to-signed base=0x%04x addr=0x%06x count=%u xor=0x80\n",
           base, addr, count);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Generated waveform tests for PicoGUS voice path                            */
/* ------------------------------------------------------------------------- */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint8_t gus_encode_wave_sample(double x, int signed_mode)
{
    /*
        x is -1.0 .. +1.0.
        unsigned: silence at 0x80, full range roughly 0x01..0xff.
        signed:   silence at 0x00, positive 0x7f, negative 0x81-ish.
    */
    if (x > 1.0) x = 1.0;
    if (x < -1.0) x = -1.0;

    if (signed_mode) {
        int s = (int)(x * 127.0);
        if (s < -128) s = -128;
        if (s > 127) s = 127;
        return (uint8_t)(int8_t)s;
    } else {
        int u = 128 + (int)(x * 120.0);
        if (u < 0) u = 0;
        if (u > 255) u = 255;
        return (uint8_t)u;
    }
}

static double gus_wave_value(const char *wave, uint32_t i, uint32_t period)
{
    double phase;

    if (period < 2) period = 2;
    phase = (double)(i % period) / (double)period;

    if (!_stricmp(wave, "square")) {
        return phase < 0.5 ? -1.0 : 1.0;
    }

    if (!_stricmp(wave, "saw")) {
        return phase * 2.0 - 1.0;
    }

    if (!_stricmp(wave, "triangle")) {
        if (phase < 0.25) return phase * 4.0;
        if (phase < 0.75) return 2.0 - phase * 4.0;
        return phase * 4.0 - 4.0;
    }

    /* Default: sine. */
    return sin(phase * 2.0 * M_PI);
}

static int cmd_gus_gen_wave_test_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-gen-wave-test-safe gus-const-test-safe gus-gen-wave-period-test-safe gus-gen-wave-addrmode-test-safe gus-addrmode-sweep-safe <base> [addr] [count] [freq] [wave] [encoding] [loop]");
        puts("  wave: sine | triangle | square | saw");
        puts("  encoding: unsigned | signed");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x8000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 4096;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0200;
    const char *wave = (ac > 6) ? av[6] : "sine";
    const char *encoding = (ac > 7) ? av[7] : "unsigned";
    int loop = (ac > 8) ? (int)u32(av[8]) : 1;
    int signed_mode = !_stricmp(encoding, "signed");
    uint32_t period = 256;
    uint8_t *buf = NULL;

    if (count < 64) count = 64;
    if (count > 262144) count = 262144;

    /*
        Keep waveform period reasonably smaller than sample loop length.
        Larger period makes sine/triangle easier to hear as a tone at low freqreg.
    */
    if (count < 1024) period = count / 4;
    if (period < 16) period = 16;

    buf = (uint8_t *)malloc(count);
    if (!buf) {
        puts("malloc failed");
        return 1;
    }

    for (uint32_t i = 0; i < count; i++) {
        double x = gus_wave_value(wave, i, period);
        buf[i] = gus_encode_wave_sample(x, signed_mode);
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-gen-wave-test-safe gus-const-test-safe gus-gen-wave-period-test-safe base=0x%04x addr=0x%06x count=%u freq=0x%04x wave=%s encoding=%s loop=%d period=%u\n",
           base, addr, count, freq, wave, signed_mode ? "signed" : "unsigned", loop, period);

    gus_print_u8_preview("generated first bytes:", buf, count);
    wav_analyze_u8_buffer(buf, count);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, count)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, addr, count, freq, loop ? 0x08 : 0x00)) return 1;

    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* GF1 voice address-packing experiments                                      */
/* ------------------------------------------------------------------------- */

static void gus_pack_addr_mode(uint32_t addr, unsigned mode, uint16_t *msw, uint16_t *lsw)
{
    uint32_t p;

    switch (mode) {
    default:
    case 0:
        *msw = (uint16_t)((addr >> 16) & 0x1FFFu);
        *lsw = (uint16_t)(addr & 0xFFE0u);
        break;

    case 1:
        *msw = (uint16_t)((addr >> 16) & 0x1FFFu);
        *lsw = (uint16_t)(addr & 0xFFFFu);
        break;

    case 2:
        p = addr << 5;
        *msw = (uint16_t)((p >> 16) & 0x1FFFu);
        *lsw = (uint16_t)(p & 0xFFE0u);
        break;

    case 3:
        p = addr >> 1;
        *msw = (uint16_t)((p >> 16) & 0x1FFFu);
        *lsw = (uint16_t)(p & 0xFFE0u);
        break;

    case 4:
        p = addr >> 4;
        *msw = (uint16_t)((p >> 16) & 0x1FFFu);
        *lsw = (uint16_t)(p & 0xFFE0u);
        break;

    case 5:
        *msw = (uint16_t)((addr >> 20) & 0x1FFFu);
        *lsw = (uint16_t)((addr >> 4) & 0xFFE0u);
        break;
    }
}

static int gus_start_voice_addrmode_no_reset(
    HANDLE h,
    uint16_t base,
    uint32_t addr,
    uint32_t count,
    uint16_t freq,
    uint8_t voice_ctrl,
    unsigned mode)
{
    uint32_t start = addr;
    uint32_t end = addr + count - 1;
    uint16_t smsw, slsw, emsw, elsw;

    gus_pack_addr_mode(start, mode, &smsw, &slsw);
    gus_pack_addr_mode(end,   mode, &emsw, &elsw);

    if (gus_select_voice(h, base, 0)) return 1;

    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    if (gus_global_write16(h, base, 0x02, smsw)) return 1;
    if (gus_global_write16(h, base, 0x03, slsw)) return 1;
    if (gus_global_write16(h, base, 0x04, emsw)) return 1;
    if (gus_global_write16(h, base, 0x05, elsw)) return 1;
    if (gus_global_write16(h, base, 0x0A, smsw)) return 1;
    if (gus_global_write16(h, base, 0x0B, slsw)) return 1;

    if (gus_global_write8_high(h, base, 0x0C, 0x08)) return 1;
    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write16(h, base, 0x09, 0xF000)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    if (gus_global_write8_high(h, base, 0x00, voice_ctrl)) return 1;

    printf("addrmode=%u start_addr=0x%06x end_addr=0x%06x packed_start=%04x:%04x packed_end=%04x:%04x ctrl=0x%02x\n",
           mode, start, end, smsw, slsw, emsw, elsw, voice_ctrl);

    return 0;
}

static int gus_make_generated_wave_buffer(uint8_t **out_buf, uint32_t count, const char *wave, const char *encoding)
{
    int signed_mode = !_stricmp(encoding, "signed");
    uint32_t period = 256;
    uint8_t *buf = NULL;

    if (count < 64) count = 64;
    if (count > 262144) count = 262144;

    if (count < 1024) period = count / 4;
    if (period < 16) period = 16;

    buf = (uint8_t *)malloc(count);
    if (!buf) {
        puts("malloc failed");
        return 1;
    }

    for (uint32_t i = 0; i < count; i++) {
        double x = gus_wave_value(wave, i, period);
        buf[i] = gus_encode_wave_sample(x, signed_mode);
    }

    printf("generated wave=%s encoding=%s count=%u period=%u\n",
           wave, signed_mode ? "signed" : "unsigned", count, period);

    gus_print_u8_preview("generated first bytes:", buf, count);
    wav_analyze_u8_buffer(buf, count);

    *out_buf = buf;
    return 0;
}

static int cmd_gus_gen_wave_addrmode_test_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-gen-wave-addrmode-test-safe <base> [addr] [count] [freq] [wave] [encoding] [loop] [mode]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x8000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 4096;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0200;
    const char *wave = (ac > 6) ? av[6] : "sine";
    const char *encoding = (ac > 7) ? av[7] : "unsigned";
    int loop = (ac > 8) ? (int)u32(av[8]) : 1;
    unsigned mode = (ac > 9) ? (unsigned)u32(av[9]) : 0;
    uint8_t *buf = NULL;

    if (count < 64) count = 64;
    if (count > 262144) count = 262144;

    if (gus_make_generated_wave_buffer(&buf, count, wave, encoding)) return 1;

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, count)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_addrmode_no_reset(h, base, addr, count, freq, loop ? 0x08 : 0x00, mode)) return 1;

    printf("gus-gen-wave-addrmode-test-safe base=0x%04x addr=0x%06x count=%u freq=0x%04x wave=%s encoding=%s loop=%d mode=%u\n",
           base, addr, count, freq, wave, encoding, loop, mode);

    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

static int cmd_gus_addrmode_sweep_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-addrmode-sweep-safe <base> [addr] [count] [freq] [wave] [encoding]");
        puts("Manual test: listen to each mode, press Enter to continue.");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x8000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 4096;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0200;
    const char *wave = (ac > 6) ? av[6] : "sine";
    const char *encoding = (ac > 7) ? av[7] : "unsigned";
    uint8_t *buf = NULL;

    if (count < 64) count = 64;
    if (count > 262144) count = 262144;

    if (gus_make_generated_wave_buffer(&buf, count, wave, encoding)) return 1;

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, count)) {
        free(buf);
        return 1;
    }

    free(buf);

    for (unsigned mode = 0; mode <= 5; mode++) {
        printf("\n===== ADDRESS MODE %u =====\n", mode);
        if (gus_start_voice_addrmode_no_reset(h, base, addr, count, freq, 0x08, mode)) return 1;
        if (gus_voice_dump_one(h, base, 0)) return 1;
        puts("Listening for 1200 ms...");
        Sleep(1200);
        if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
        Sleep(100);
    }

    puts("addrmode sweep done.");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Constant and explicit-period generated waveform tests                      */
/* ------------------------------------------------------------------------- */

static int cmd_gus_const_test_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-const-test-safe <base> [addr] [count] [freq] [value] [loop]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x8000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 4096;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0020;
    uint8_t value = (ac > 6) ? (uint8_t)u32(av[6]) : 0x80;
    int loop = (ac > 7) ? (int)u32(av[7]) : 1;

    uint8_t *buf = NULL;

    if (count < 64) count = 64;
    if (count > 262144) count = 262144;

    buf = (uint8_t *)malloc(count);
    if (!buf) {
        puts("malloc failed");
        return 1;
    }

    memset(buf, value, count);

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-const-test-safe base=0x%04x addr=0x%06x count=%u freq=0x%04x value=0x%02x loop=%d\n",
           base, addr, count, freq, value, loop);

    gus_print_u8_preview("generated first bytes:", buf, count);
    wav_analyze_u8_buffer(buf, count);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, count)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, addr, count, freq, loop ? 0x08 : 0x00)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

static int cmd_gus_gen_wave_period_test_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-gen-wave-period-test-safe <base> [addr] [count] [freq] [wave] [encoding] [period] [loop]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x8000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 16384;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0020;
    const char *wave = (ac > 6) ? av[6] : "sine";
    const char *encoding = (ac > 7) ? av[7] : "unsigned";
    uint32_t period = (ac > 8) ? u32(av[8]) : 4096;
    int loop = (ac > 9) ? (int)u32(av[9]) : 1;

    int signed_mode = !_stricmp(encoding, "signed");
    uint8_t *buf = NULL;

    if (count < 64) count = 64;
    if (count > 262144) count = 262144;
    if (period < 2) period = 2;
    if (period > count) period = count;

    buf = (uint8_t *)malloc(count);
    if (!buf) {
        puts("malloc failed");
        return 1;
    }

    for (uint32_t i = 0; i < count; i++) {
        double x = gus_wave_value(wave, i, period);
        buf[i] = gus_encode_wave_sample(x, signed_mode);
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-gen-wave-period-test-safe base=0x%04x addr=0x%06x count=%u freq=0x%04x wave=%s encoding=%s period=%u loop=%d\n",
           base, addr, count, freq, wave, signed_mode ? "signed" : "unsigned", period, loop);

    gus_print_u8_preview("generated first bytes:", buf, count);
    wav_analyze_u8_buffer(buf, count);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, count)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, addr, count, freq, loop ? 0x08 : 0x00)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Guarded one-shot WAV playback with current-address polling                 */
/* ------------------------------------------------------------------------- */

static uint32_t gus_cur_addr_from_regs(uint16_t msw, uint16_t lsw)
{
    return ((uint32_t)msw << 16) | (uint32_t)lsw;
}

static int gus_read_current_addr_pollsafe(HANDLE h, uint16_t base, uint32_t *addr_out, uint16_t *msw_out, uint16_t *lsw_out)
{
    uint16_t msw = 0, lsw = 0;
    uint8_t lo = 0, hi = 0;

    /* read global register 0x8A: current address MSW */
    if (port_out8(h, (uint16_t)(base + 0x103), 0x8A)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi)) return 1;
    msw = (uint16_t)(lo | ((uint16_t)hi << 8));

    /* read global register 0x8B: current address LSW */
    if (port_out8(h, (uint16_t)(base + 0x103), 0x8B)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi)) return 1;
    lsw = (uint16_t)(lo | ((uint16_t)hi << 8));

    if (msw_out) *msw_out = msw;
    if (lsw_out) *lsw_out = lsw;
    if (addr_out) *addr_out = gus_cur_addr_from_regs(msw, lsw);
    return 0;
}

static int gus_fill_guard_region_80_pollsafe(HANDLE h, uint16_t base)
{
    const uint8_t v = 0x80;

    for (uint32_t a = 0; a < 0x80000u; a++) {
        if (gus_dram_set_addr(h, base, a)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), v)) return 1;
    }

    return 0;
}

static int cmd_gus_wav_play_guarded_poll_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-guarded-poll-safe gus-wav-play-calibrated-safe <base> <wavfile> [dram_addr] [sample_offset] [sample_count] [freq] [gain_x100] [poll_ms] [poll_count]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t dram_addr = (ac > 4) ? u32(av[4]) : 0x10000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 0;
    uint32_t sample_count = (ac > 6) ? u32(av[6]) : 262144;
    uint16_t freq = (ac > 7) ? (uint16_t)u32(av[7]) : 0x0002;
    uint32_t gain_x100 = (ac > 8) ? u32(av[8]) : 100;
    uint32_t poll_ms = (ac > 9) ? u32(av[9]) : 100;
    uint32_t poll_count = (ac > 10) ? u32(av[10]) : 30;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    LARGE_INTEGER qpf, q0, qprev;
    uint32_t prev_addr = 0;
    uint64_t total_delta = 0;
    double total_dt = 0.0;

    if (poll_ms < 10) poll_ms = 10;
    if (poll_ms > 2000) poll_ms = 2000;
    if (poll_count < 1) poll_count = 1;
    if (poll_count > 200) poll_count = 200;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    printf("guard-fill: 0x00000..0x7ffff with 0x80; this can take a while...\n");
    if (gus_fill_guard_region_80_pollsafe(h, base)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play-guarded-poll-safe gus-wav-play-calibrated-safe file=%s src_offset=%u dram=0x%06x samples=%u freq=0x%04x gain=%u poll_ms=%u poll_count=%u src_rate=%u\n",
           path, sample_offset, dram_addr, n, freq, gain_x100, poll_ms, poll_count, wi.sample_rate);

    gus_print_u8_preview("converted first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, dram_addr, n, freq, 0x00)) return 1;

    if (gus_voice_dump_one(h, base, 0)) return 1;

    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&q0);
    qprev = q0;

    if (gus_read_current_addr_pollsafe(h, base, &prev_addr, NULL, NULL)) return 1;

    printf("poll index elapsed_ms cur_msw cur_lsw cur_addr delta bytes_per_sec\n");

    for (uint32_t i = 0; i < poll_count; i++) {
        LARGE_INTEGER qnow;
        uint16_t msw = 0, lsw = 0;
        uint32_t cur = 0;
        int32_t delta;
        double dt;
        double bps;

        Sleep(poll_ms);
        QueryPerformanceCounter(&qnow);

        if (gus_read_current_addr_pollsafe(h, base, &cur, &msw, &lsw)) return 1;

        dt = (double)(qnow.QuadPart - qprev.QuadPart) / (double)qpf.QuadPart;
        delta = (int32_t)(cur - prev_addr);
        if (delta < 0) delta = 0;
        bps = dt > 0.0 ? (double)delta / dt : 0.0;

        printf("poll %3u %9.3f %04x %04x 0x%06x %+8d %10.1f\n",
               i + 1,
               1000.0 * (double)(qnow.QuadPart - q0.QuadPart) / (double)qpf.QuadPart,
               msw, lsw, cur, delta, bps);

        total_delta += (uint32_t)delta;
        total_dt += dt;

        prev_addr = cur;
        qprev = qnow;

        if (cur >= dram_addr + n - 32) {
            puts("poll: near/end reached");
            break;
        }
    }

    if (total_dt > 0.0 && total_delta > 0) {
        double avg_bps = (double)total_delta / total_dt;
        double freq_for_src = (double)freq * ((double)wi.sample_rate / avg_bps);
        unsigned approx = (unsigned)((freq_for_src < 0.0) ? 0.0 : (freq_for_src + 0.5));

        printf("calibration: avg_bytes_per_sec=%.1f current_freq=0x%04x source_rate=%u estimated_freq_for_source_rate=%.3f approx_hex=0x%04x\n",
               avg_bps, freq, wi.sample_rate, freq_for_src, approx & 0xFFFFu);
    } else {
        puts("calibration: insufficient movement measured");
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Calibrated guarded one-shot WAV playback                                   */
/* ------------------------------------------------------------------------- */

#ifndef gus_fill_guard_region_80_calibrated_DEFINED
#define gus_fill_guard_region_80_calibrated_DEFINED
static int gus_fill_guard_region_80_calibrated(HANDLE h, uint16_t base)
{
    const uint8_t v = 0x80;

#ifdef gus_fill_guard_region_80_pollsafe
    return gus_fill_guard_region_80_pollsafe(h, base);
#else
    for (uint32_t a = 0; a < 0x80000u; a++) {
        if (gus_dram_set_addr(h, base, a)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), v)) return 1;
    }
    return 0;
#endif
}
#endif

static uint32_t gus_cur_addr_from_regs_calibrated(uint16_t msw, uint16_t lsw)
{
    return ((uint32_t)msw << 16) | (uint32_t)lsw;
}

static int gus_read_current_addr_calibrated(HANDLE h, uint16_t base, uint32_t *addr_out, uint16_t *msw_out, uint16_t *lsw_out)
{
#ifdef gus_read_current_addr_pollsafe
    return gus_read_current_addr_pollsafe(h, base, addr_out, msw_out, lsw_out);
#else
    uint16_t msw = 0, lsw = 0;
    uint8_t lo = 0, hi = 0;

    if (port_out8(h, (uint16_t)(base + 0x103), 0x8A)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi)) return 1;
    msw = (uint16_t)(lo | ((uint16_t)hi << 8));

    if (port_out8(h, (uint16_t)(base + 0x103), 0x8B)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi)) return 1;
    lsw = (uint16_t)(lo | ((uint16_t)hi << 8));

    if (msw_out) *msw_out = msw;
    if (lsw_out) *lsw_out = lsw;
    if (addr_out) *addr_out = gus_cur_addr_from_regs_calibrated(msw, lsw);
    return 0;
#endif
}

static uint16_t gus_calibrated_freq_for_rate(uint32_t sample_rate)
{
    /*
        Empirical result from this setup:
          freq=0x0002 => ~44102.5 bytes/sec on 44100 Hz 8-bit mono.
          freq=0x0001 => no movement.
          freq=0x0003 => also ~44102 bytes/sec in later run, likely quantized.
    */
    if (sample_rate < 8000) return 0x0002;
    return 0x0002;
}

static void gus_calibrated_xor_u8_inplace(uint8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) buf[i] ^= 0x80;
}

static int cmd_gus_wav_play_calibrated_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-calibrated-safe <base> <wavfile> [dram_addr] [sample_offset] [sample_count] [gain_x100] [signed_mode] [poll]");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t dram_addr = (ac > 4) ? u32(av[4]) : 0x10000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 0;
    uint32_t sample_count = (ac > 6) ? u32(av[6]) : 262144;
    uint32_t gain_x100 = (ac > 7) ? u32(av[7]) : 100;
    int signed_mode = (ac > 8) ? (int)u32(av[8]) : 0;
    int poll = (ac > 9) ? (int)u32(av[9]) : 1;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint16_t freq;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    freq = gus_calibrated_freq_for_rate(wi.sample_rate);

    if (signed_mode) {
        gus_calibrated_xor_u8_inplace(buf, n);
    }

    printf("calibrated guard-fill: 0x00000..0x7ffff with 0x80; this can take a while...\n");
    if (gus_fill_guard_region_80_calibrated(h, base)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play-calibrated-safe file=%s src_offset=%u dram=0x%06x samples=%u src_rate=%u freq=0x%04x gain=%u signed=%d loop=0\n",
           path, sample_offset, dram_addr, n, wi.sample_rate, freq, gain_x100, signed_mode);

    gus_print_u8_preview(signed_mode ? "converted signed/XOR first bytes:" : "converted unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_no_reset_with_ctrl(h, base, dram_addr, n, freq, 0x00)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;

    if (poll) {
        puts("polling current address for ~1500 ms...");
        for (int i = 0; i < 15; i++) {
            uint16_t msw = 0, lsw = 0;
            uint32_t cur = 0;
            Sleep(100);
            if (gus_read_current_addr_calibrated(h, base, &cur, &msw, &lsw)) return 1;
            printf("  poll %02d cur=%04x:%04x 0x%06x\n", i + 1, msw, lsw, cur);
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Output/mixer/voice-control sweep                                           */
/* ------------------------------------------------------------------------- */

static int gus_guard_fill_value_sweep(HANDLE h, uint16_t base, uint8_t value)
{
    for (uint32_t a = 0; a < 0x80000u; a++) {
        if (gus_dram_set_addr(h, base, a)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static int gus_start_voice_sweep_case(
    HANDLE h,
    uint16_t base,
    uint32_t addr,
    uint32_t count,
    uint16_t freq,
    uint8_t active,
    uint16_t vol,
    uint8_t volctrl,
    uint8_t pan,
    uint8_t ctrl)
{
    uint32_t end = addr + count - 1;
    uint16_t smsw = (uint16_t)((addr >> 16) & 0x1FFFu);
    uint16_t slsw = (uint16_t)(addr & 0xFFE0u);
    uint16_t emsw = (uint16_t)((end >> 16) & 0x1FFFu);
    uint16_t elsw = (uint16_t)(end & 0xFFE0u);

    if (gus_select_voice(h, base, 0)) return 1;

    /* Stop voice/ramp before reprogramming. */
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /*
        Active voices register affects mixer/sample-rate behavior on GF1.
        This may need to be set before starting the voice.
    */
    if (gus_global_write8_high(h, base, 0x0E, active)) return 1;

    if (gus_global_write16(h, base, 0x02, smsw)) return 1;
    if (gus_global_write16(h, base, 0x03, slsw)) return 1;
    if (gus_global_write16(h, base, 0x04, emsw)) return 1;
    if (gus_global_write16(h, base, 0x05, elsw)) return 1;
    if (gus_global_write16(h, base, 0x0A, smsw)) return 1;
    if (gus_global_write16(h, base, 0x0B, slsw)) return 1;

    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write8_high(h, base, 0x0C, pan)) return 1;

    /*
        Try ramp endpoints too. If these registers are meaningful for the
        current mode, keeping them at/near the current volume avoids a ramp to 0.
    */
    if (gus_global_write16(h, base, 0x07, vol)) return 1;
    if (gus_global_write16(h, base, 0x08, vol)) return 1;
    if (gus_global_write16(h, base, 0x09, vol)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, volctrl)) return 1;

    if (gus_global_write8_high(h, base, 0x00, ctrl)) return 1;
    return 0;
}

static int cmd_gus_output_sweep_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-output-sweep-safe <base> [addr] [count] [freq] [wave] [encoding] [period]");
        puts("default: addr=0x10000 count=262144 freq=0x0002 wave=sine encoding=unsigned period=100");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x10000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 262144;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0002;
    const char *wave = (ac > 6) ? av[6] : "sine";
    const char *encoding = (ac > 7) ? av[7] : "unsigned";
    uint32_t period = (ac > 8) ? u32(av[8]) : 100;
    int signed_mode = !_stricmp(encoding, "signed");
    uint8_t guard = signed_mode ? 0x00 : 0x80;

    uint8_t *buf = NULL;

    static const uint8_t active_vals[] = { 0x0D, 0x0E, 0x0F, 0x1F };
    static const uint16_t vol_vals[] = { 0xE000, 0xF000, 0xFFFF };
    static const uint8_t volctrl_vals[] = { 0x00, 0x01, 0x03 };
    static const uint8_t pan_vals[] = { 0x08, 0x00, 0x0F };
    static const uint8_t ctrl_vals[] = { 0x00, 0x04, 0x10, 0x14 };

    if (count < 4096) count = 4096;
    if (count > 262144) count = 262144;
    if (period < 2) period = 2;
    if (period > count) period = count;

    buf = (uint8_t *)malloc(count);
    if (!buf) {
        puts("malloc failed");
        return 1;
    }

    for (uint32_t i = 0; i < count; i++) {
        double x = gus_wave_value(wave, i, period);
        buf[i] = gus_encode_wave_sample(x, signed_mode);
    }

    printf("guard-fill: 0x00000..0x7ffff value=0x%02x; this can take a while...\n", guard);
    if (gus_guard_fill_value_sweep(h, base, guard)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-output-sweep-safe base=0x%04x addr=0x%06x count=%u freq=0x%04x wave=%s encoding=%s period=%u\n",
           base, addr, count, freq, wave, signed_mode ? "signed" : "unsigned", period);

    gus_print_u8_preview("generated first bytes:", buf, count);
    wav_analyze_u8_buffer(buf, count);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, count)) {
        free(buf);
        return 1;
    }

    free(buf);

    int case_no = 0;

    /*
        First: compact sweep over most likely cases.
    */
    for (unsigned ai = 0; ai < sizeof(active_vals)/sizeof(active_vals[0]); ai++) {
        for (unsigned vi = 0; vi < sizeof(vol_vals)/sizeof(vol_vals[0]); vi++) {
            for (unsigned ri = 0; ri < sizeof(volctrl_vals)/sizeof(volctrl_vals[0]); ri++) {
                case_no++;
                printf("\nCASE %d: active=0x%02x vol=0x%04x volctrl=0x%02x pan=0x08 ctrl=0x00\n",
                       case_no, active_vals[ai], vol_vals[vi], volctrl_vals[ri]);

                if (gus_start_voice_sweep_case(h, base, addr, count, freq,
                                               active_vals[ai], vol_vals[vi],
                                               volctrl_vals[ri], 0x08, 0x00)) return 1;
                if (gus_voice_dump_one(h, base, 0)) return 1;
                Sleep(900);
                if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
                Sleep(120);
            }
        }
    }

    /*
        Second: voice ctrl / pan sweep using the best-guess active/vol/ramp.
    */
    for (unsigned ci = 0; ci < sizeof(ctrl_vals)/sizeof(ctrl_vals[0]); ci++) {
        for (unsigned pi = 0; pi < sizeof(pan_vals)/sizeof(pan_vals[0]); pi++) {
            case_no++;
            printf("\nCASE %d: active=0x0d vol=0xffff volctrl=0x03 pan=0x%02x ctrl=0x%02x\n",
                   case_no, pan_vals[pi], ctrl_vals[ci]);

            if (gus_start_voice_sweep_case(h, base, addr, count, freq,
                                           0x0D, 0xFFFF, 0x03,
                                           pan_vals[pi], ctrl_vals[ci])) return 1;
            if (gus_voice_dump_one(h, base, 0)) return 1;
            Sleep(900);
            if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
            Sleep(120);
        }
    }

    puts("\noutput sweep done.");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Loud-case calibrated guarded one-shot WAV playback                         */
/* ------------------------------------------------------------------------- */

static int gus_guard_fill_loud_safe(HANDLE h, uint16_t base, uint8_t value)
{
    for (uint32_t a = 0; a < 0x80000u; a++) {
        if (gus_dram_set_addr(h, base, a)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static uint32_t gus_cur_addr_from_regs_loud(uint16_t msw, uint16_t lsw)
{
    return ((uint32_t)msw << 16) | (uint32_t)lsw;
}

static int gus_read_current_addr_loud(HANDLE h, uint16_t base, uint32_t *addr_out, uint16_t *msw_out, uint16_t *lsw_out)
{
    uint16_t msw = 0, lsw = 0;
    uint8_t lo = 0, hi = 0;

    if (port_out8(h, (uint16_t)(base + 0x103), 0x8A)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi)) return 1;
    msw = (uint16_t)(lo | ((uint16_t)hi << 8));

    if (port_out8(h, (uint16_t)(base + 0x103), 0x8B)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi)) return 1;
    lsw = (uint16_t)(lo | ((uint16_t)hi << 8));

    if (msw_out) *msw_out = msw;
    if (lsw_out) *lsw_out = lsw;
    if (addr_out) *addr_out = gus_cur_addr_from_regs_loud(msw, lsw);
    return 0;
}

static uint16_t gus_loud_calibrated_freq_for_rate(uint32_t sample_rate)
{
    (void)sample_rate;
    return 0x0002; /* measured ~44102.5 bytes/sec on this setup */
}

static int gus_start_voice_loud_case(
    HANDLE h,
    uint16_t base,
    uint32_t addr,
    uint32_t count,
    uint16_t freq,
    uint8_t active,
    uint16_t vol,
    uint8_t volctrl,
    uint8_t pan,
    uint8_t ctrl)
{
    uint32_t end = addr + count - 1;
    uint16_t smsw = (uint16_t)((addr >> 16) & 0x1FFFu);
    uint16_t slsw = (uint16_t)(addr & 0xFFE0u);
    uint16_t emsw = (uint16_t)((end >> 16) & 0x1FFFu);
    uint16_t elsw = (uint16_t)(end & 0xFFE0u);

    if (gus_select_voice(h, base, 0)) return 1;

    /* Stop voice/ramp before programming. */
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1;

    /* Sweep showed active=0x1f family was much louder. */
    if (gus_global_write8_high(h, base, 0x0E, active)) return 1;

    if (gus_global_write16(h, base, 0x02, smsw)) return 1;
    if (gus_global_write16(h, base, 0x03, slsw)) return 1;
    if (gus_global_write16(h, base, 0x04, emsw)) return 1;
    if (gus_global_write16(h, base, 0x05, elsw)) return 1;
    if (gus_global_write16(h, base, 0x0A, smsw)) return 1;
    if (gus_global_write16(h, base, 0x0B, slsw)) return 1;

    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write8_high(h, base, 0x0C, pan)) return 1;

    /*
        Keep ramp endpoints and current volume together. Sweep showed case 28/30
        at active=0x1f were strong even with vol=0xe000.
    */
    if (gus_global_write16(h, base, 0x07, vol)) return 1;
    if (gus_global_write16(h, base, 0x08, vol)) return 1;
    if (gus_global_write16(h, base, 0x09, vol)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, volctrl)) return 1;

    if (gus_global_write8_high(h, base, 0x00, ctrl)) return 1;
    return 0;
}

/* CASE42_DEFAULT_CONFIRMED_BY_LISTENING: active=0x0d vol=0xffff volctrl=0x03 pan=0x0f ctrl=0x04 */
static int cmd_gus_wav_play_loud_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-loud-safe <base> <wavfile> [dram_addr] [sample_offset] [sample_count] [gain_x100] [active] [vol] [volctrl] [pan] [ctrl] [poll]");
        puts("default case 42: dram=0x10000 offset=262144 count=214997 gain=400 active=0x0d vol=0xffff volctrl=0x03 pan=0x0f ctrl=0x04 poll=1");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t dram_addr = (ac > 4) ? u32(av[4]) : 0x10000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 262144;
    uint32_t sample_count = (ac > 6) ? u32(av[6]) : 214997;
    uint32_t gain_x100 = (ac > 7) ? u32(av[7]) : 400;
    uint8_t active = (ac > 8) ? (uint8_t)u32(av[8]) : 0x0d;
    uint16_t vol = (ac > 9) ? (uint16_t)u32(av[9]) : 0xffff;
    uint8_t volctrl = (ac > 10) ? (uint8_t)u32(av[10]) : 0x03;
    uint8_t pan = (ac > 11) ? (uint8_t)u32(av[11]) : 0x0f;
    uint8_t ctrl = (ac > 12) ? (uint8_t)u32(av[12]) : 0x04;
    int poll = (ac > 13) ? (int)u32(av[13]) : 1;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint16_t freq;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    freq = gus_loud_calibrated_freq_for_rate(wi.sample_rate);

    printf("loud-case guard-fill: 0x00000..0x7ffff with 0x80; this can take a while...\n");
    if (gus_guard_fill_loud_safe(h, base, 0x80)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play-loud-safe file=%s src_offset=%u dram=0x%06x samples=%u src_rate=%u freq=0x%04x gain=%u active=0x%02x vol=0x%04x volctrl=0x%02x pan=0x%02x ctrl=0x%02x\n",
           path, sample_offset, dram_addr, n, wi.sample_rate, freq, gain_x100,
           active, vol, volctrl, pan, ctrl);

    gus_print_u8_preview("converted unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_loud_case(h, base, dram_addr, n, freq, active, vol, volctrl, pan, ctrl)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;

    if (poll) {
        puts("polling current address for ~1500 ms...");
        for (int i = 0; i < 15; i++) {
            uint16_t msw = 0, lsw = 0;
            uint32_t cur = 0;
            Sleep(100);
            if (gus_read_current_addr_loud(h, base, &cur, &msw, &lsw)) return 1;
            printf("  poll %02d cur=%04x:%04x 0x%06x\n", i + 1, msw, lsw, cur);
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Old-loud loop-mode WAV diagnostic                                          */
/* ------------------------------------------------------------------------- */

static int gus_guard_fill_oldloud_loop(HANDLE h, uint16_t base, uint8_t value)
{
    for (uint32_t a = 0; a < 0x80000u; a++) {
        if (gus_dram_set_addr(h, base, a)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static int gus_start_voice_oldloud_loop(
    HANDLE h,
    uint16_t base,
    uint32_t addr,
    uint32_t count,
    uint16_t freq)
{
    uint32_t end = addr + count - 1;
    uint16_t smsw = (uint16_t)((addr >> 16) & 0x1FFFu);
    uint16_t slsw = (uint16_t)(addr & 0xFFE0u);
    uint16_t emsw = (uint16_t)((end >> 16) & 0x1FFFu);
    uint16_t elsw = (uint16_t)(end & 0xFFE0u);

    if (gus_select_voice(h, base, 0)) return 1;

    /* Match the original loud loop path as closely as possible. */
    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1; /* stop voice */
    if (gus_global_write8_high(h, base, 0x0D, 0x00)) return 1; /* volctrl readback was 0x0000 in loud test */

    if (gus_global_write16(h, base, 0x02, smsw)) return 1;
    if (gus_global_write16(h, base, 0x03, slsw)) return 1;
    if (gus_global_write16(h, base, 0x04, emsw)) return 1;
    if (gus_global_write16(h, base, 0x05, elsw)) return 1;

    if (gus_global_write16(h, base, 0x0A, smsw)) return 1;
    if (gus_global_write16(h, base, 0x0B, slsw)) return 1;

    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write8_high(h, base, 0x0C, 0x08)) return 1;    /* center pan */
    if (gus_global_write16(h, base, 0x09, 0xF000)) return 1;      /* same as loud test */
    if (gus_global_write8_high(h, base, 0x0D, 0x00)) return 1;    /* keep ramp running/neutral like original */

    if (gus_global_write8_high(h, base, 0x00, 0x08)) return 1;    /* loop on */
    return 0;
}

static int cmd_gus_wav_play_oldloud_loop_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-oldloud-loop-safe <base> <wavfile> [addr] [sample_offset] [sample_count] [gain_x100] [freq] [guard_fill]");
        puts("default: addr=0x4000 offset=262144 count=32768 gain=400 freq=0x0800 guard_fill=0");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t dram_addr = (ac > 4) ? u32(av[4]) : 0x4000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 262144;
    uint32_t sample_count = (ac > 6) ? u32(av[6]) : 32768;
    uint32_t gain_x100 = (ac > 7) ? u32(av[7]) : 400;
    uint16_t freq = (ac > 8) ? (uint16_t)u32(av[8]) : 0x0800;
    int guard_fill = (ac > 9) ? (int)u32(av[9]) : 0;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    if (guard_fill) {
        puts("oldloud diagnostic guard-fill enabled: 0x00000..0x7ffff with 0x80");
        if (gus_guard_fill_oldloud_loop(h, base, 0x80)) {
            free(buf);
            return 1;
        }
    } else {
        puts("oldloud diagnostic: no guard-fill, matching original loud/stale-DRAM conditions");
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play-oldloud-loop-safe file=%s src_offset=%u dram=0x%06x samples=%u src_rate=%u freq=0x%04x gain=%u loop=1 ctrl=0x08 vol=0xf000 volctrl=0x00\n",
           path, sample_offset, dram_addr, n, wi.sample_rate, freq, gain_x100);

    gus_print_u8_preview("converted unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_oldloud_loop(h, base, dram_addr, n, freq)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;

    Sleep(200);
    puts("after 200ms:");
    if (gus_voice_dump_one(h, base, 0)) return 1;

    return 0;
}

/* ------------------------------------------------------------------------- */
/* GF1-correct address-pack test                                              */
/* ------------------------------------------------------------------------- */

static uint16_t gf1_addr_hi_from_byte(uint32_t addr)
{
    return (uint16_t)((addr >> 7) & 0x1FFFu);
}

static uint16_t gf1_addr_lo_from_byte(uint32_t addr)
{
    return (uint16_t)((addr & 0x7Fu) << 9);
}

static uint32_t gf1_addr_byte_from_regs(uint16_t hi, uint16_t lo)
{
    return (((uint32_t)hi & 0x1FFFu) << 7) | (((uint32_t)lo >> 9) & 0x7Fu);
}

static int gus_guard_fill_gf1addr(HANDLE h, uint16_t base, uint8_t value)
{
    for (uint32_t a = 0; a < 0x80000u; a++) {
        if (gus_dram_set_addr(h, base, a)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static int gus_read_cur_addr_gf1addr(HANDLE h, uint16_t base, uint32_t *byte_addr_out, uint16_t *hi_out, uint16_t *lo_out)
{
    uint8_t lo8 = 0, hi8 = 0;
    uint16_t hi = 0, lo = 0;

    if (port_out8(h, (uint16_t)(base + 0x103), 0x8A)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo8)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi8)) return 1;
    hi = (uint16_t)(lo8 | ((uint16_t)hi8 << 8));

    if (port_out8(h, (uint16_t)(base + 0x103), 0x8B)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x104), &lo8)) return 1;
    if (port_in8(h, (uint16_t)(base + 0x105), &hi8)) return 1;
    lo = (uint16_t)(lo8 | ((uint16_t)hi8 << 8));

    if (hi_out) *hi_out = hi;
    if (lo_out) *lo_out = lo;
    if (byte_addr_out) *byte_addr_out = gf1_addr_byte_from_regs(hi, lo);
    return 0;
}

static int gus_start_voice_gf1addr(
    HANDLE h,
    uint16_t base,
    uint32_t addr,
    uint32_t count,
    uint16_t freq,
    uint8_t ctrl,
    uint8_t active,
    uint16_t vol,
    uint8_t volctrl,
    uint8_t pan)
{
    uint32_t end = addr + count - 1;

    uint16_t smsw = gf1_addr_hi_from_byte(addr);
    uint16_t slsw = gf1_addr_lo_from_byte(addr);
    uint16_t emsw = gf1_addr_hi_from_byte(end);
    uint16_t elsw = gf1_addr_lo_from_byte(end);

    if (gus_select_voice(h, base, 0)) return 1;

    if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1; /* stop voice */
    if (gus_global_write8_high(h, base, 0x0D, 0x03)) return 1; /* stop ramp */

    if (gus_global_write8_high(h, base, 0x0E, active)) return 1;

    if (gus_global_write16(h, base, 0x02, smsw)) return 1;
    if (gus_global_write16(h, base, 0x03, slsw)) return 1;
    if (gus_global_write16(h, base, 0x04, emsw)) return 1;
    if (gus_global_write16(h, base, 0x05, elsw)) return 1;

    if (gus_global_write16(h, base, 0x0A, smsw)) return 1;
    if (gus_global_write16(h, base, 0x0B, slsw)) return 1;

    if (gus_global_write16(h, base, 0x01, freq)) return 1;
    if (gus_global_write8_high(h, base, 0x0C, pan)) return 1;

    if (gus_global_write16(h, base, 0x07, vol)) return 1;
    if (gus_global_write16(h, base, 0x08, vol)) return 1;
    if (gus_global_write16(h, base, 0x09, vol)) return 1;
    if (gus_global_write8_high(h, base, 0x0D, volctrl)) return 1;

    if (gus_global_write8_high(h, base, 0x00, ctrl)) return 1;

    printf("GF1 addr pack: start byte=0x%06x -> %04x:%04x, end byte=0x%06x -> %04x:%04x\n",
           addr, smsw, slsw, end, emsw, elsw);

    return 0;
}

static int cmd_gus_gf1addr_square_test_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-gf1addr-square-test-safe <base> [addr] [count] [freq] [period] [ctrl] [active] [vol] [volctrl] [pan]");
        puts("default: addr=0x10000 count=262144 freq=0x0800 period=100 ctrl=0x08 active=0x0d vol=0xf000 volctrl=0x00 pan=0x08");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    uint32_t addr = (ac > 3) ? u32(av[3]) : 0x10000;
    uint32_t count = (ac > 4) ? u32(av[4]) : 262144;
    uint16_t freq = (ac > 5) ? (uint16_t)u32(av[5]) : 0x0800;
    uint32_t period = (ac > 6) ? u32(av[6]) : 100;
    uint8_t ctrl = (ac > 7) ? (uint8_t)u32(av[7]) : 0x08;
    uint8_t active = (ac > 8) ? (uint8_t)u32(av[8]) : 0x0d;
    uint16_t vol = (ac > 9) ? (uint16_t)u32(av[9]) : 0xf000;
    uint8_t volctrl = (ac > 10) ? (uint8_t)u32(av[10]) : 0x00;
    uint8_t pan = (ac > 11) ? (uint8_t)u32(av[11]) : 0x08;

    uint8_t *buf = NULL;

    if (count < 4096) count = 4096;
    if (count > 262144) count = 262144;
    if (period < 2) period = 2;

    buf = (uint8_t *)malloc(count);
    if (!buf) {
        puts("malloc failed");
        return 1;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint32_t phase = i % period;
        buf[i] = (phase < (period / 2)) ? 0x00 : 0xff;
    }

    puts("guard-fill: 0x00000..0x7ffff with 0x80");
    if (gus_guard_fill_gf1addr(h, base, 0x80)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-gf1addr-square-test-safe base=0x%04x addr=0x%06x count=%u freq=0x%04x period=%u ctrl=0x%02x active=0x%02x vol=0x%04x volctrl=0x%02x pan=0x%02x\n",
           base, addr, count, freq, period, ctrl, active, vol, volctrl, pan);

    gus_print_u8_preview("generated first bytes:", buf, count);
    wav_analyze_u8_buffer(buf, count);

    if (gus_upload_u8_buffer_safe(h, base, addr, buf, count)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_gf1addr(h, base, addr, count, freq, ctrl, active, vol, volctrl, pan)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;

    Sleep(200);
    puts("after 200ms:");
    if (gus_voice_dump_one(h, base, 0)) return 1;

    for (int i = 0; i < 10; i++) {
        uint16_t hi = 0, lo = 0;
        uint32_t cur = 0;
        Sleep(100);
        if (gus_read_cur_addr_gf1addr(h, base, &cur, &hi, &lo)) return 1;
        printf("  cur %02d raw=%04x:%04x byte???0x%06x\n", i + 1, hi, lo, cur);
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* GF1-address-correct WAV playback                                           */
/* ------------------------------------------------------------------------- */

static int cmd_gus_wav_play_gf1addr_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-gf1addr-safe <base> <wavfile> [addr] [sample_offset] [sample_count] [gain_x100] [freq] [ctrl] [active] [vol] [volctrl] [pan] [poll]");
        puts("default: addr=0x10000 offset=262144 count=214997 gain=400 freq=0x0400 ctrl=0x00 active=0x0d vol=0xf000 volctrl=0x00 pan=0x08 poll=1");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    uint32_t dram_addr     = (ac > 4)  ? u32(av[4])          : 0x10000;
    uint32_t sample_offset = (ac > 5)  ? u32(av[5])          : 262144;
    uint32_t sample_count  = (ac > 6)  ? u32(av[6])          : 214997;
    uint32_t gain_x100     = (ac > 7)  ? u32(av[7])          : 400;
    uint16_t freq          = (ac > 8)  ? (uint16_t)u32(av[8])  : 0x0400;
    uint8_t ctrl           = (ac > 9)  ? (uint8_t)u32(av[9])   : 0x00;
    uint8_t active         = (ac > 10) ? (uint8_t)u32(av[10])  : 0x0d;
    uint16_t vol           = (ac > 11) ? (uint16_t)u32(av[11]) : 0xf000;
    uint8_t volctrl        = (ac > 12) ? (uint8_t)u32(av[12])  : 0x00;
    uint8_t pan            = (ac > 13) ? (uint8_t)u32(av[13])  : 0x08;
    int poll               = (ac > 14) ? (int)u32(av[14])      : 1;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    puts("guard-fill: 0x00000..0x7ffff with 0x80");
    if (gus_guard_fill_gf1addr(h, base, 0x80)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play-gf1addr-safe file=%s src_offset=%u dram=0x%06x samples=%u src_rate=%u freq=0x%04x gain=%u ctrl=0x%02x active=0x%02x vol=0x%04x volctrl=0x%02x pan=0x%02x\n",
           path, sample_offset, dram_addr, n, wi.sample_rate, freq, gain_x100,
           ctrl, active, vol, volctrl, pan);

    gus_print_u8_preview("converted unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_gf1addr(h, base, dram_addr, n, freq, ctrl, active, vol, volctrl, pan)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;

    Sleep(200);
    puts("after 200ms:");
    if (gus_voice_dump_one(h, base, 0)) return 1;

    if (poll) {
        puts("polling current GF1 byte address for ~1500ms...");
        for (int i = 0; i < 15; i++) {
            uint16_t hi = 0, lo = 0;
            uint32_t cur = 0;
            Sleep(100);
            if (gus_read_cur_addr_gf1addr(h, base, &cur, &hi, &lo)) return 1;
            printf("  poll %02d raw=%04x:%04x byte???0x%06x\n", i + 1, hi, lo, cur);
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* DRAM high-address / mirroring test                                         */
/* ------------------------------------------------------------------------- */

static int gus_dram_fill_small_pattern(HANDLE h, uint16_t base, uint32_t addr, uint8_t value, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static int gus_dram_dump_small_inline(HANDLE h, uint16_t base, uint32_t addr, uint32_t count)
{
    printf("%06x:", addr);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t v = 0;
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_in8(h, (uint16_t)(base + 0x107), &v)) return 1;
        printf(" %02x", v);
    }
    printf("\n");
    return 0;
}

static int cmd_gus_dram_hiaddr_test_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-dram-hiaddr-test-safe <base>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);

    puts("Writing unique 32-byte patterns at 0x00000/0x08000/0x10000/0x18000/0x20000...");
    if (gus_dram_fill_small_pattern(h, base, 0x00000, 0x11, 32)) return 1;
    if (gus_dram_fill_small_pattern(h, base, 0x08000, 0x22, 32)) return 1;
    if (gus_dram_fill_small_pattern(h, base, 0x10000, 0x33, 32)) return 1;
    if (gus_dram_fill_small_pattern(h, base, 0x18000, 0x44, 32)) return 1;
    if (gus_dram_fill_small_pattern(h, base, 0x20000, 0x55, 32)) return 1;

    puts("Readback. These must be distinct; if they mirror, high address is still broken:");
    if (gus_dram_dump_small_inline(h, base, 0x00000, 16)) return 1;
    if (gus_dram_dump_small_inline(h, base, 0x08000, 16)) return 1;
    if (gus_dram_dump_small_inline(h, base, 0x10000, 16)) return 1;
    if (gus_dram_dump_small_inline(h, base, 0x18000, 16)) return 1;
    if (gus_dram_dump_small_inline(h, base, 0x20000, 16)) return 1;

    return 0;
}

/* ------------------------------------------------------------------------- */
/* 64 KiB-aperture GF1-address-correct WAV playback                           */
/* ------------------------------------------------------------------------- */

static int cmd_gus_wav_play64_gf1addr_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play64-gf1addr-safe <base> <wavfile> [sample_offset] [count] [gain_x100] [freq] [ctrl] [active] [vol] [volctrl] [pan] [poll]");
        puts("hardcoded DRAM addr=0x0000; count is clamped to <=65536 because current DRAM I/O aperture mirrors every 64 KiB");
        puts("default: offset=0 count=65536 gain=150 freq=0x0300 ctrl=0x00 active=0x0d vol=0xf000 volctrl=0x00 pan=0x08 poll=1");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    const uint32_t dram_addr = 0x0000;
    uint32_t sample_offset = (ac > 4) ? u32(av[4]) : 0;
    uint32_t sample_count  = (ac > 5) ? u32(av[5]) : 65536;
    uint32_t gain_x100     = (ac > 6) ? u32(av[6]) : 150;
    uint16_t freq          = (ac > 7) ? (uint16_t)u32(av[7]) : 0x0300;
    uint8_t ctrl           = (ac > 8) ? (uint8_t)u32(av[8]) : 0x00;
    uint8_t active         = (ac > 9) ? (uint8_t)u32(av[9]) : 0x0d;
    uint16_t vol           = (ac > 10) ? (uint16_t)u32(av[10]) : 0xf000;
    uint8_t volctrl        = (ac > 11) ? (uint8_t)u32(av[11]) : 0x00;
    uint8_t pan            = (ac > 12) ? (uint8_t)u32(av[12]) : 0x08;
    int poll               = (ac > 13) ? (int)u32(av[13]) : 1;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (sample_count == 0 || sample_count > 65536) {
        sample_count = 65536;
    }

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    if (n > 65536) {
        n = 65536;
    }

    puts("play64: guard-fill 0x00000..0x7ffff with 0x80; only 0x0000..0xffff is reliable for upload/play right now");
    if (gus_guard_fill_gf1addr(h, base, 0x80)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play64-gf1addr-safe file=%s src_offset=%u dram=0x000000 samples=%u src_rate=%u freq=0x%04x gain=%u ctrl=0x%02x active=0x%02x vol=0x%04x volctrl=0x%02x pan=0x%02x\n",
           path, sample_offset, n, wi.sample_rate, freq, gain_x100,
           ctrl, active, vol, volctrl, pan);

    gus_print_u8_preview("converted unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_gf1addr(h, base, dram_addr, n, freq, ctrl, active, vol, volctrl, pan)) return 1;
    if (gus_voice_dump_one(h, base, 0)) return 1;

    Sleep(200);
    puts("after 200ms:");
    if (gus_voice_dump_one(h, base, 0)) return 1;

    if (poll) {
        puts("polling current GF1 byte address for ~1500ms...");
        for (int i = 0; i < 15; i++) {
            uint16_t hi = 0, lo = 0;
            uint32_t cur = 0;
            Sleep(100);
            if (gus_read_cur_addr_gf1addr(h, base, &cur, &hi, &lo)) return 1;
            printf("  poll %02d raw=%04x:%04x byte???0x%06x\n", i + 1, hi, lo, cur);
        }
    }

    puts("play64 note: use source offsets 0, 65536, 131072, 196608, 262144... to manually step through the WAV.");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* 64 KiB GF1 WAV audition: no readback/polling while audio plays             */
/* ------------------------------------------------------------------------- */

static int cmd_gus_wav_play64_audition_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play64-audition-safe <base> <wavfile> [sample_offset] [count] [gain_x100] [freq] [ctrl] [active] [vol] [volctrl] [pan] [guard]");
        puts("hardcoded DRAM addr=0x0000; count clamped <=65536; no dump/poll after start");
        puts("default: offset=0 count=65536 gain=100 freq=0x0380 ctrl=0x00 active=0x0d vol=0xf000 volctrl=0x00 pan=0x08 guard=1");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    const uint32_t dram_addr = 0x0000;
    uint32_t sample_offset = (ac > 4) ? u32(av[4]) : 0;
    uint32_t sample_count  = (ac > 5) ? u32(av[5]) : 65536;
    uint32_t gain_x100     = (ac > 6) ? u32(av[6]) : 100;
    uint16_t freq          = (ac > 7) ? (uint16_t)u32(av[7]) : 0x0380;
    uint8_t ctrl           = (ac > 8) ? (uint8_t)u32(av[8]) : 0x00;
    uint8_t active         = (ac > 9) ? (uint8_t)u32(av[9]) : 0x0d;
    uint16_t vol           = (ac > 10) ? (uint16_t)u32(av[10]) : 0xf000;
    uint8_t volctrl        = (ac > 11) ? (uint8_t)u32(av[11]) : 0x00;
    uint8_t pan            = (ac > 12) ? (uint8_t)u32(av[12]) : 0x08;
    int guard              = (ac > 13) ? (int)u32(av[13]) : 1;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (sample_count == 0 || sample_count > 65536) sample_count = 65536;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }
    if (n > 65536) n = 65536;

    if (guard) {
        puts("audition: guard-fill 0x00000..0x7ffff with 0x80 before playback");
        if (gus_guard_fill_gf1addr(h, base, 0x80)) {
            free(buf);
            return 1;
        }
    } else {
        puts("audition: guard-fill skipped");
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play64-audition-safe file=%s src_offset=%u dram=0x000000 samples=%u src_rate=%u freq=0x%04x gain=%u ctrl=0x%02x active=0x%02x vol=0x%04x volctrl=0x%02x pan=0x%02x guard=%d\n",
           path, sample_offset, n, wi.sample_rate, freq, gain_x100,
           ctrl, active, vol, volctrl, pan, guard);

    gus_print_u8_preview("converted unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_gf1addr(h, base, dram_addr, n, freq, ctrl, active, vol, volctrl, pan)) return 1;

    puts("started; no further GUS register reads/polls from this command");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* 64 KiB GF1 WAV quality audition matrix                                     */
/* ------------------------------------------------------------------------- */

static int cmd_gus_wav_play64_quality_matrix_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play64-quality-matrix-safe <base> <wavfile> [sample_offset] [count] [gain_x100] [listen_ms]");
        puts("uploads once to DRAM 0x0000, then sweeps freq/control without polling while audio plays");
        puts("default: offset=0 count=65536 gain=100 listen_ms=1200");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    const uint32_t dram_addr = 0x0000;
    uint32_t sample_offset = (ac > 4) ? u32(av[4]) : 0;
    uint32_t sample_count  = (ac > 5) ? u32(av[5]) : 65536;
    uint32_t gain_x100     = (ac > 6) ? u32(av[6]) : 100;
    uint32_t listen_ms     = (ac > 7) ? u32(av[7]) : 1200;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    static const uint16_t freqs[] = {
        0x0340, 0x0360, 0x0380, 0x03A0, 0x03C0, 0x03E0, 0x0400
    };

    static const uint8_t ctrls[] = {
        0x00, 0x04
    };

    if (sample_count == 0 || sample_count > 65536) sample_count = 65536;
    if (listen_ms < 250) listen_ms = 250;
    if (listen_ms > 5000) listen_ms = 5000;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }
    if (n > 65536) n = 65536;

    puts("quality-matrix: guard-fill 0x00000..0x7ffff with 0x80");
    if (gus_guard_fill_gf1addr(h, base, 0x80)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play64-quality-matrix-safe file=%s src_offset=%u dram=0x000000 samples=%u src_rate=%u gain=%u listen_ms=%u\n",
           path, sample_offset, n, wi.sample_rate, gain_x100, listen_ms);

    gus_print_u8_preview("converted unsigned first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    int case_no = 0;
    for (unsigned ci = 0; ci < sizeof(ctrls) / sizeof(ctrls[0]); ci++) {
        for (unsigned fi = 0; fi < sizeof(freqs) / sizeof(freqs[0]); fi++) {
            case_no++;
            printf("\nCASE %02d: freq=0x%04x ctrl=0x%02x active=0x0d vol=0xf000 volctrl=0x00 pan=0x08\n",
                   case_no, freqs[fi], ctrls[ci]);

            if (gus_start_voice_gf1addr(h, base, dram_addr, n,
                                        freqs[fi],
                                        ctrls[ci],
                                        0x0d,
                                        0xf000,
                                        0x00,
                                        0x08)) {
                return 1;
            }

            Sleep(listen_ms);

            /* Stop voice between cases. Do not dump/poll while playing. */
            if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
            Sleep(180);
        }
    }

    puts("\nquality matrix done. Pick the least crunchy CASE number.");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* 64 KiB GF1 WAV encoding audition                                           */
/* ------------------------------------------------------------------------- */

static void gus_apply_u8_encoding_variant(uint8_t *buf, uint32_t n, const char *encoding)
{
    if (!encoding || !_stricmp(encoding, "unsigned") || !_stricmp(encoding, "u8")) {
        return;
    }

    if (!_stricmp(encoding, "xor80") || !_stricmp(encoding, "signed")) {
        for (uint32_t i = 0; i < n; i++) {
            buf[i] ^= 0x80;
        }
        return;
    }

    if (!_stricmp(encoding, "invert") || !_stricmp(encoding, "inv")) {
        for (uint32_t i = 0; i < n; i++) {
            buf[i] = (uint8_t)(255u - buf[i]);
        }
        return;
    }

    if (!_stricmp(encoding, "signedinv") || !_stricmp(encoding, "invxor80")) {
        for (uint32_t i = 0; i < n; i++) {
            buf[i] = (uint8_t)((255u - buf[i]) ^ 0x80u);
        }
        return;
    }
}

static uint8_t gus_guard_value_for_encoding(const char *encoding)
{
    if (!encoding) return 0x80;

    /*
        For unsigned PCM, silence is 0x80.
        For xor80/signed-style, silence is 0x00.
        Inverted unsigned also has silence around 0x7f, so 0x80 is close.
        signedinv maps unsigned 0x80 -> 0x7f, also not important outside 64 KiB
        because we stay inside the loaded aperture.
    */
    if (!_stricmp(encoding, "xor80") || !_stricmp(encoding, "signed")) {
        return 0x00;
    }
    return 0x80;
}

static int cmd_gus_wav_play64_enc_audition_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play64-enc-audition-safe <base> <wavfile> [sample_offset] [count] [gain_x100] [freq] [encoding] [ctrl] [active] [vol] [volctrl] [pan] [guard]");
        puts("hardcoded DRAM addr=0x0000; count clamped <=65536; no dump/poll after start");
        puts("encodings: unsigned xor80 invert signedinv");
        puts("default: offset=0 count=65536 gain=100 freq=0x03c0 encoding=unsigned ctrl=0x00 active=0x0d vol=0xf000 volctrl=0x00 pan=0x08 guard=1");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    const uint32_t dram_addr = 0x0000;
    uint32_t sample_offset = (ac > 4) ? u32(av[4]) : 0;
    uint32_t sample_count  = (ac > 5) ? u32(av[5]) : 65536;
    uint32_t gain_x100     = (ac > 6) ? u32(av[6]) : 100;
    uint16_t freq          = (ac > 7) ? (uint16_t)u32(av[7]) : 0x03c0;
    const char *encoding   = (ac > 8) ? av[8] : "unsigned";
    uint8_t ctrl           = (ac > 9) ? (uint8_t)u32(av[9]) : 0x00;
    uint8_t active         = (ac > 10) ? (uint8_t)u32(av[10]) : 0x0d;
    uint16_t vol           = (ac > 11) ? (uint16_t)u32(av[11]) : 0xf000;
    uint8_t volctrl        = (ac > 12) ? (uint8_t)u32(av[12]) : 0x00;
    uint8_t pan            = (ac > 13) ? (uint8_t)u32(av[13]) : 0x08;
    int guard              = (ac > 14) ? (int)u32(av[14]) : 1;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint8_t guard_value;

    if (sample_count == 0 || sample_count > 65536) sample_count = 65536;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }
    if (n > 65536) n = 65536;

    gus_apply_u8_encoding_variant(buf, n, encoding);
    guard_value = gus_guard_value_for_encoding(encoding);

    if (guard) {
        printf("enc-audition: guard-fill 0x00000..0x7ffff with 0x%02x before playback\n", guard_value);
        if (gus_guard_fill_gf1addr(h, base, guard_value)) {
            free(buf);
            return 1;
        }
    } else {
        puts("enc-audition: guard-fill skipped");
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play64-enc-audition-safe file=%s src_offset=%u dram=0x000000 samples=%u src_rate=%u freq=0x%04x gain=%u encoding=%s ctrl=0x%02x active=0x%02x vol=0x%04x volctrl=0x%02x pan=0x%02x guard=%d\n",
           path, sample_offset, n, wi.sample_rate, freq, gain_x100, encoding,
           ctrl, active, vol, volctrl, pan, guard);

    gus_print_u8_preview("encoded first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_gf1addr(h, base, dram_addr, n, freq, ctrl, active, vol, volctrl, pan)) return 1;

    puts("started; no further GUS register reads/polls from this command");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Clean 64 KiB GF1 WAV playback: xor80/signed-style default                  */
/* ------------------------------------------------------------------------- */

static void gus_xor80_buffer(uint8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        buf[i] ^= 0x80;
    }
}

static int cmd_gus_wav_play64_clean_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play64-clean-safe <base> <wavfile> [sample_offset] [count] [gain_x100] [freq] [listen_ms]");
        puts("hardcoded: DRAM=0x0000, encoding=xor80, ctrl=0x00, active=0x0d, vol=0xf000, volctrl=0x00, pan=0x08");
        puts("default: offset=0 count=65536 gain=400 freq=0x03c0 listen_ms=0/no-autostop");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    const uint32_t dram_addr = 0x0000;
    uint32_t sample_offset = (ac > 4) ? u32(av[4]) : 0;
    uint32_t sample_count  = (ac > 5) ? u32(av[5]) : 65536;
    uint32_t gain_x100     = (ac > 6) ? u32(av[6]) : 400;
    uint16_t freq          = (ac > 7) ? (uint16_t)u32(av[7]) : 0x03c0;
    uint32_t listen_ms     = (ac > 8) ? u32(av[8]) : 0;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;

    if (sample_count == 0 || sample_count > 65536) sample_count = 65536;

    if (wav_read_info(path, &wi)) return 1;
    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }
    if (n > 65536) n = 65536;

    /*
        Critical discovery:
          PicoGUS/GF1 voice playback sounded clean when unsigned WAV bytes were
          converted to signed-style bytes by XOR 0x80.
        Silence/guard for this encoding is 0x00.
    */
    gus_xor80_buffer(buf, n);

    puts("clean64: guard-fill 0x00000..0x7ffff with 0x00 for xor80/signed-style silence");
    if (gus_guard_fill_gf1addr(h, base, 0x00)) {
        free(buf);
        return 1;
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play64-clean-safe file=%s src_offset=%u dram=0x000000 samples=%u src_rate=%u freq=0x%04x gain=%u encoding=xor80 ctrl=0x00 active=0x0d vol=0xf000 volctrl=0x00 pan=0x08 listen_ms=%u\n",
           path, sample_offset, n, wi.sample_rate, freq, gain_x100, listen_ms);

    gus_print_u8_preview("encoded xor80 first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_gf1addr(h, base, dram_addr, n,
                                freq,
                                0x00,   /* ctrl: one-shot */
                                0x0d,   /* active voices */
                                0xf000, /* volume */
                                0x00,   /* volctrl */
                                0x08))  /* pan center */
    {
        return 1;
    }

    puts("started clean64; no post-start polling/dumps");

    if (listen_ms > 0) {
        if (listen_ms > 10000) listen_ms = 10000;
        Sleep(listen_ms);
        if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
        puts("auto-stopped");
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* GUS/PicoGUS DRAM high-address latch mode sweep                             */
/* ------------------------------------------------------------------------- */

static int gus_global_write16_raw_hi_sweep(HANDLE h, uint16_t base, uint8_t reg, uint16_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x104), (uint8_t)(value & 0xff))) return 1;
    if (port_out8(h, (uint16_t)(base + 0x105), (uint8_t)((value >> 8) & 0xff))) return 1;
    return 0;
}

static int gus_global_write8_low_raw_hi_sweep(HANDLE h, uint16_t base, uint8_t reg, uint8_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x104), value)) return 1;
    return 0;
}

static int gus_global_write8_high_raw_hi_sweep(HANDLE h, uint16_t base, uint8_t reg, uint8_t value)
{
    if (port_out8(h, (uint16_t)(base + 0x103), reg)) return 1;
    if (port_out8(h, (uint16_t)(base + 0x105), value)) return 1;
    return 0;
}

/*
    Mode guesses for programming DRAM address.

    Known low-address mechanism:
      reg 0x43 = low 16 address bits.

    The unknown part is how/if high address bits are latched for PicoGUS.
*/
static int gus_dram_set_addr_mode_hi_sweep(HANDLE h, uint16_t base, uint32_t addr, int mode)
{
    uint16_t lo16 = (uint16_t)(addr & 0xffffu);
    uint8_t hi8 = (uint8_t)((addr >> 16) & 0xffu);
    uint16_t hi16 = (uint16_t)hi8;

    switch (mode) {
    default:
    case 0:
        /* old/current assumption: 0x43 low16 then 0x44 low byte */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x44, hi16)) return 1;
        break;

    case 1:
        /* high first, then low */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x44, hi16)) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        break;

    case 2:
        /* low, high, reselect low */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x44, hi16)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x103), 0x43)) return 1;
        break;

    case 3:
        /* write high addr to high byte of 0x44 */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x44, (uint16_t)(hi8 << 8))) return 1;
        break;

    case 4:
        /* high first to high byte of 0x44, then low */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x44, (uint16_t)(hi8 << 8))) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        break;

    case 5:
        /* byte write low side of 0x44 only */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        if (gus_global_write8_low_raw_hi_sweep(h, base, 0x44, hi8)) return 1;
        break;

    case 6:
        /* byte write high side of 0x44 only */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        if (gus_global_write8_high_raw_hi_sweep(h, base, 0x44, hi8)) return 1;
        break;

    case 7:
        /* write high bits to 0x45 low byte, because some docs/code name 0x43/0x44/0x45 pairs differently */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        if (gus_global_write8_low_raw_hi_sweep(h, base, 0x45, hi8)) return 1;
        break;

    case 8:
        /* high first on 0x45, then low */
        if (gus_global_write8_low_raw_hi_sweep(h, base, 0x45, hi8)) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        break;

    case 9:
        /*
            Some emulators/firmware may treat reg 0x44 as a full 16-bit high/control
            latch, but only after the low address is written to data port once.
        */
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), 0x00)) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x44, hi16)) return 1;
        if (gus_global_write16_raw_hi_sweep(h, base, 0x43, lo16)) return 1;
        break;
    }

    return 0;
}

static int gus_dram_write_pattern_mode_hi_sweep(HANDLE h, uint16_t base, int mode, uint32_t addr, uint8_t value, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (gus_dram_set_addr_mode_hi_sweep(h, base, addr + i, mode)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static int gus_dram_dump_inline_mode_hi_sweep(HANDLE h, uint16_t base, int mode, uint32_t addr, uint32_t count)
{
    printf("%06x:", addr);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t v = 0;
        if (gus_dram_set_addr_mode_hi_sweep(h, base, addr + i, mode)) return 1;
        if (port_in8(h, (uint16_t)(base + 0x107), &v)) return 1;
        printf(" %02x", v);
    }
    printf("\n");
    return 0;
}

static int cmd_gus_dram_hiaddr_mode_sweep_safe(HANDLE h, int ac, char **av)
{
    if (ac < 3) {
        puts("gus-dram-hiaddr-mode-sweep-safe <base>");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);

    puts("GUS/PicoGUS DRAM high-address mode sweep");
    puts("Success pattern should be:");
    puts("  000000: 11 ...");
    puts("  008000: 22 ...");
    puts("  010000: 33 ...");
    puts("  018000: 44 ...");
    puts("  020000: 55 ...");

    for (int mode = 0; mode <= 9; mode++) {
        printf("\n================ MODE %d ================\n", mode);

        if (gus_dram_write_pattern_mode_hi_sweep(h, base, mode, 0x00000, 0x11, 32)) return 1;
        if (gus_dram_write_pattern_mode_hi_sweep(h, base, mode, 0x08000, 0x22, 32)) return 1;
        if (gus_dram_write_pattern_mode_hi_sweep(h, base, mode, 0x10000, 0x33, 32)) return 1;
        if (gus_dram_write_pattern_mode_hi_sweep(h, base, mode, 0x18000, 0x44, 32)) return 1;
        if (gus_dram_write_pattern_mode_hi_sweep(h, base, mode, 0x20000, 0x55, 32)) return 1;

        if (gus_dram_dump_inline_mode_hi_sweep(h, base, mode, 0x00000, 16)) return 1;
        if (gus_dram_dump_inline_mode_hi_sweep(h, base, mode, 0x08000, 16)) return 1;
        if (gus_dram_dump_inline_mode_hi_sweep(h, base, mode, 0x10000, 16)) return 1;
        if (gus_dram_dump_inline_mode_hi_sweep(h, base, mode, 0x18000, 16)) return 1;
        if (gus_dram_dump_inline_mode_hi_sweep(h, base, mode, 0x20000, 16)) return 1;
    }

    puts("\nmode sweep done.");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Full/high-DRAM GF1 clean WAV playback                                      */
/* ------------------------------------------------------------------------- */

#ifndef GUS_FULL_CLEAN_MAX_UPLOAD
#define GUS_FULL_CLEAN_MAX_UPLOAD (512u * 1024u)
#endif

static void gus_xor80_buffer_fullclean(uint8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        buf[i] ^= 0x80;
    }
}

static int gus_guard_fill_range_fullclean(HANDLE h, uint16_t base, uint32_t addr, uint32_t count, uint8_t value)
{
    for (uint32_t i = 0; i < count; i++) {
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static int cmd_gus_wav_play_full_gf1_clean_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-play-full-gf1-clean-safe <base> <wavfile> [dram_addr] [sample_offset] [sample_count] [gain_x100] [freq] [listen_ms]");
        puts("default: dram=0x10000 offset=0 count=remaining/clamped gain=400 freq=0x03c0 listen_ms=0/no-autostop");
        puts("uses confirmed high-DRAM addr mode3, xor80 encoding, GF1 packed voice addresses");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    uint32_t dram_addr     = (ac > 4) ? u32(av[4]) : 0x10000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 0;
    uint32_t sample_count  = (ac > 6) ? u32(av[6]) : 0;
    uint32_t gain_x100     = (ac > 7) ? u32(av[7]) : 400;
    uint16_t freq          = (ac > 8) ? (uint16_t)u32(av[8]) : 0x03c0;
    uint32_t listen_ms     = (ac > 9) ? u32(av[9]) : 0;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    uint32_t bytes_per_sample = 0;
    uint32_t bytes_per_frame = 0;
    uint32_t total_frames = 0;

    if (wav_read_info(path, &wi)) return 1;

    bytes_per_sample = (uint32_t)(wi.bits_per_sample / 8);
    if (bytes_per_sample == 0) bytes_per_sample = 1;
    bytes_per_frame = (uint32_t)wi.channels * bytes_per_sample;
    if (bytes_per_frame == 0) bytes_per_frame = 1;
    total_frames = (uint32_t)(wi.data_size / bytes_per_frame);

    if (sample_offset >= total_frames) {
        printf("wav: sample_offset %u >= frames %u\n", sample_offset, total_frames);
        return 1;
    }

    if (sample_count == 0) {
        sample_count = total_frames - sample_offset;
    }

    if (sample_count > GUS_FULL_CLEAN_MAX_UPLOAD) {
        printf("sample_count %u clamped to %u for safe port upload\n", sample_count, GUS_FULL_CLEAN_MAX_UPLOAD);
        sample_count = GUS_FULL_CLEAN_MAX_UPLOAD;
    }

    if (wav_convert_window_to_u8_gain(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) return 1;

    if (n < 64) {
        free(buf);
        puts("wav: too few converted samples");
        return 1;
    }

    gus_xor80_buffer_fullclean(buf, n);

    /*
        Fill a little before and after the target region with signed silence.
        Keep this bounded so the command does not take forever.
    */
    {
        uint32_t guard_start = (dram_addr >= 4096) ? (dram_addr - 4096) : 0;
        uint32_t guard_count = n + 8192;
        if (guard_count > GUS_FULL_CLEAN_MAX_UPLOAD + 8192) {
            guard_count = GUS_FULL_CLEAN_MAX_UPLOAD + 8192;
        }
        puts("full-clean: bounded guard-fill around target region with 0x00 xor80/signed silence");
        if (gus_guard_fill_range_fullclean(h, base, guard_start, guard_count, 0x00)) {
            free(buf);
            return 1;
        }
    }

    if (gus_enable_dac_sequence(h, base)) {
        free(buf);
        return 1;
    }

    printf("gus-wav-play-full-gf1-clean-safe file=%s src_offset=%u dram=0x%06x samples=%u src_rate=%u freq=0x%04x gain=%u encoding=xor80 ctrl=0x00 active=0x0d vol=0xf000 volctrl=0x00 pan=0x08 listen_ms=%u\n",
           path, sample_offset, dram_addr, n, wi.sample_rate, freq, gain_x100, listen_ms);

    gus_print_u8_preview("encoded xor80 first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    free(buf);

    if (gus_start_voice_gf1addr(h, base, dram_addr, n,
                                freq,
                                0x00,   /* one-shot */
                                0x0d,
                                0xf000,
                                0x00,
                                0x08)) {
        return 1;
    }

    puts("started full-clean; no post-start polling/dumps");

    if (listen_ms > 0) {
        if (listen_ms > 30000) listen_ms = 30000;
        Sleep(listen_ms);
        if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
        puts("auto-stopped");
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Full WAV upload + verify, no 262144 clamp                                  */
/* ------------------------------------------------------------------------- */

static uint8_t gus_clip_u8_fullverify(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static uint8_t gus_apply_gain_u8_fullverify(uint8_t u, uint32_t gain_x100)
{
    int centered = (int)u - 128;
    int scaled = (centered * (int)gain_x100) / 100;
    return gus_clip_u8_fullverify(128 + scaled);
}

static uint32_t gus_hash32_fullverify(const uint8_t *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static int gus_wav_convert_window_to_xor80_noclamp_fullverify(
    const char *path,
    const GUS_WAV_INFO *wi,
    uint32_t sample_offset,
    uint32_t sample_count,
    uint32_t gain_x100,
    uint8_t **out_buf,
    uint32_t *out_n)
{
    FILE *f = NULL;
    uint8_t *buf = NULL;
    uint32_t bytes_per_sample = 0;
    uint32_t bytes_per_frame = 0;
    uint32_t total_frames = 0;
    uint32_t n = 0;

    *out_buf = NULL;
    *out_n = 0;

    bytes_per_sample = (uint32_t)(wi->bits_per_sample / 8);
    if (bytes_per_sample == 0) bytes_per_sample = 1;
    bytes_per_frame = (uint32_t)wi->channels * bytes_per_sample;
    if (bytes_per_frame == 0) bytes_per_frame = 1;

    total_frames = (uint32_t)(wi->data_size / bytes_per_frame);

    if (sample_offset >= total_frames) {
        printf("wav-noclamp: sample_offset %u >= frames %u\n", sample_offset, total_frames);
        return 1;
    }

    if (sample_count == 0 || sample_count > total_frames - sample_offset) {
        sample_count = total_frames - sample_offset;
    }

    if (sample_count == 0) {
        puts("wav-noclamp: no samples requested");
        return 1;
    }

    if (sample_count > (1024u * 1024u)) {
        printf("wav-noclamp: sample_count %u too large for this safety command; clamp manually <=1048576\n", sample_count);
        return 1;
    }

    buf = (uint8_t *)malloc(sample_count);
    if (!buf) {
        puts("wav-noclamp: malloc failed");
        return 1;
    }

    f = fopen(path, "rb");
    if (!f) {
        free(buf);
        printf("wav-noclamp: open failed: %s\n", path);
        return 1;
    }

    if (fseek(f, (long)(wi->data_offset + (uint64_t)sample_offset * bytes_per_frame), SEEK_SET)) {
        fclose(f);
        free(buf);
        puts("wav-noclamp: seek failed");
        return 1;
    }

    for (n = 0; n < sample_count; n++) {
        int mono = 0;
        uint8_t u8 = 0x80;

        if (wi->bits_per_sample == 8) {
            int accum = 0;
            for (uint16_t ch = 0; ch < wi->channels; ch++) {
                int c = fgetc(f);
                if (c < 0) goto done_reading_fullverify;
                accum += c & 0xff;
            }
            mono = accum / (int)wi->channels;
            u8 = (uint8_t)mono;
        } else if (wi->bits_per_sample == 16) {
            int accum = 0;
            for (uint16_t ch = 0; ch < wi->channels; ch++) {
                int lo = fgetc(f);
                int hi = fgetc(f);
                int16_t s;
                if (lo < 0 || hi < 0) goto done_reading_fullverify;
                s = (int16_t)((lo & 0xff) | ((hi & 0xff) << 8));
                accum += (int)s;
            }
            mono = accum / (int)wi->channels;
            u8 = gus_clip_u8_fullverify(128 + (mono / 256));
        } else {
            printf("wav-noclamp: unsupported bits_per_sample=%u\n", wi->bits_per_sample);
            fclose(f);
            free(buf);
            return 1;
        }

        u8 = gus_apply_gain_u8_fullverify(u8, gain_x100);

        /* Critical clean-path encoding: unsigned WAV -> signed-style GF1/PicoGUS */
        buf[n] = (uint8_t)(u8 ^ 0x80);
    }

done_reading_fullverify:
    fclose(f);

    if (n < 64) {
        free(buf);
        printf("wav-noclamp: too few samples read: %u\n", n);
        return 1;
    }

    *out_buf = buf;
    *out_n = n;
    return 0;
}

static int gus_read_dram_bytes_fullverify(HANDLE h, uint16_t base, uint32_t addr, uint8_t *dst, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_in8(h, (uint16_t)(base + 0x107), &dst[i])) return 1;
    }
    return 0;
}

static int gus_verify_dram_pages_fullverify(HANDLE h, uint16_t base, uint32_t dram_addr, const uint8_t *buf, uint32_t n)
{
    uint8_t tmp[4096];
    uint32_t pos = 0;
    uint32_t total_bad = 0;
    uint32_t page_no = 0;

    puts("verify: per-64KiB page, reading first/mid/last 4096-byte windows when present");

    while (pos < n) {
        uint32_t page_left = n - pos;
        uint32_t page_n = page_left > 65536u ? 65536u : page_left;
        uint32_t checks[3];
        uint32_t check_count = 0;

        checks[check_count++] = 0;
        if (page_n > 8192) checks[check_count++] = page_n / 2;
        if (page_n > 4096) checks[check_count++] = page_n - 4096;

        printf("  page %u dram=0x%06x bytes=%u\n", page_no, dram_addr + pos, page_n);

        for (uint32_t ci = 0; ci < check_count; ci++) {
            uint32_t off = checks[ci];
            uint32_t cn = page_n - off;
            uint32_t bad = 0;
            if (cn > sizeof(tmp)) cn = sizeof(tmp);

            if (gus_read_dram_bytes_fullverify(h, base, dram_addr + pos + off, tmp, cn)) return 1;

            for (uint32_t i = 0; i < cn; i++) {
                if (tmp[i] != buf[pos + off + i]) {
                    bad++;
                    if (bad <= 4) {
                        printf("    mismatch +0x%05x got=%02x exp=%02x\n",
                               off + i, tmp[i], buf[pos + off + i]);
                    }
                }
            }

            printf("    win +0x%05x len=%u exp_hash=%08x got_hash=%08x bad=%u\n",
                   off, cn,
                   gus_hash32_fullverify(buf + pos + off, cn),
                   gus_hash32_fullverify(tmp, cn),
                   bad);

            total_bad += bad;
        }

        pos += page_n;
        page_no++;
    }

    printf("verify total_bad=%u\n", total_bad);
    return total_bad ? 2 : 0;
}

static int gus_guard_fill_range_fullverify(HANDLE h, uint16_t base, uint32_t addr, uint32_t count, uint8_t value)
{
    for (uint32_t i = 0; i < count; i++) {
        if (gus_dram_set_addr(h, base, addr + i)) return 1;
        if (port_out8(h, (uint16_t)(base + 0x107), value)) return 1;
    }
    return 0;
}

static int cmd_gus_wav_upload_full_verify_safe(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        puts("gus-wav-upload-full-verify-safe <base> <wavfile> [dram_addr] [sample_offset] [sample_count] [gain_x100] [freq] [play]");
        puts("default: dram=0x10000 offset=0 count=whole remaining gain=400 freq=0x03c0 play=1");
        puts("no 262144 conversion clamp; converts PCM to xor80, uploads, verifies sampled page windows, optionally plays");
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];
    uint32_t dram_addr     = (ac > 4) ? u32(av[4]) : 0x10000;
    uint32_t sample_offset = (ac > 5) ? u32(av[5]) : 0;
    uint32_t sample_count  = (ac > 6) ? u32(av[6]) : 0;
    uint32_t gain_x100     = (ac > 7) ? u32(av[7]) : 400;
    uint16_t freq          = (ac > 8) ? (uint16_t)u32(av[8]) : 0x03c0;
    int play               = (ac > 9) ? (int)u32(av[9]) : 1;

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    int vrc = 0;

    if (wav_read_info(path, &wi)) return 1;

    if (gus_wav_convert_window_to_xor80_noclamp_fullverify(path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) {
        return 1;
    }

    printf("gus-wav-upload-full-verify-safe file=%s src_offset=%u dram=0x%06x samples=%u src_rate=%u bits=%u ch=%u gain=%u freq=0x%04x play=%d encoding=xor80\n",
           path, sample_offset, dram_addr, n, wi.sample_rate, wi.bits_per_sample, wi.channels, gain_x100, freq, play);

    gus_print_u8_preview("encoded xor80 first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    {
        uint32_t guard_start = (dram_addr >= 4096) ? dram_addr - 4096 : 0;
        uint32_t guard_count = n + 8192;
        puts("guard-fill around target with 0x00");
        if (gus_guard_fill_range_fullverify(h, base, guard_start, guard_count, 0x00)) {
            free(buf);
            return 1;
        }
    }

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    vrc = gus_verify_dram_pages_fullverify(h, base, dram_addr, buf, n);
    if (vrc == 1) {
        free(buf);
        return 1;
    }

    if (play) {
        if (gus_enable_dac_sequence(h, base)) {
            free(buf);
            return 1;
        }

        if (gus_start_voice_gf1addr(h, base, dram_addr, n,
                                    freq,
                                    0x00,
                                    0x0d,
                                    0xf000,
                                    0x00,
                                    0x08)) {
            free(buf);
            return 1;
        }

        puts("started verified full upload; no post-start polling/dumps");
    }

    free(buf);
    return vrc;
}

/* ------------------------------------------------------------------------- */
/* Official known-good GUS WAV player                                         */
/* ------------------------------------------------------------------------- */

static void usage_gus_play_safe_official(void)
{
    puts("gus-play-safe <base> <wavfile> [options]");
    puts("");
    puts("Confirmed-good defaults:");
    puts("  dram=0x10000 offset=0 count=whole-WAV gain=400 freq=0x0400 verify=off play=on");
    puts("");
    puts("Options:");
    puts("  --verify | verify       sampled per-page readback verification");
    puts("  --no-play               upload/verify only");
    puts("  --dram <hex/dec>        default 0x10000");
    puts("  --offset <hex/dec>      WAV sample/frame offset, default 0");
    puts("  --count <hex/dec>       sample/frame count, default whole remaining");
    puts("  --gain <hex/dec>        gain x100, default 400");
    puts("  --freq <hex/dec>        GF1 frequency register, default 0x0400");
    puts("  --listen <ms>           auto-stop after ms, default 0/no-autostop");
    puts("");
    puts("Examples:");
    puts("  gus-play-safe 0x8240 .\\test.wav");
    puts("  gus-play-safe 0x8240 .\\test.wav --verify");
    puts("  gus-play-safe 0x8240 .\\test.wav --verify --freq 0x03c0");
    puts("  gus-play-safe 0x8240 .\\test.wav --no-play --verify");
}

static int cmd_gus_play_safe_official(HANDLE h, int ac, char **av)
{
    if (ac < 4) {
        usage_gus_play_safe_official();
        return 2;
    }

    uint16_t base = (uint16_t)u32(av[2]);
    const char *path = av[3];

    uint32_t dram_addr     = 0x10000;
    uint32_t sample_offset = 0;
    uint32_t sample_count  = 0;       /* 0 = whole remaining WAV */
    uint32_t gain_x100     = 400;
    uint16_t freq          = 0x0400;
    uint32_t listen_ms     = 0;
    int verify             = 0;
    int play               = 1;

    for (int i = 4; i < ac; i++) {
        if (!_stricmp(av[i], "--verify") || !_stricmp(av[i], "verify")) {
            verify = 1;
        } else if (!_stricmp(av[i], "--no-verify")) {
            verify = 0;
        } else if (!_stricmp(av[i], "--no-play")) {
            play = 0;
        } else if (!_stricmp(av[i], "--play")) {
            play = 1;
        } else if ((!_stricmp(av[i], "--dram") || !_stricmp(av[i], "dram")) && i + 1 < ac) {
            dram_addr = u32(av[++i]);
        } else if ((!_stricmp(av[i], "--offset") || !_stricmp(av[i], "offset")) && i + 1 < ac) {
            sample_offset = u32(av[++i]);
        } else if ((!_stricmp(av[i], "--count") || !_stricmp(av[i], "count")) && i + 1 < ac) {
            sample_count = u32(av[++i]);
        } else if ((!_stricmp(av[i], "--gain") || !_stricmp(av[i], "gain")) && i + 1 < ac) {
            gain_x100 = u32(av[++i]);
        } else if ((!_stricmp(av[i], "--freq") || !_stricmp(av[i], "freq")) && i + 1 < ac) {
            freq = (uint16_t)u32(av[++i]);
        } else if ((!_stricmp(av[i], "--listen") || !_stricmp(av[i], "listen")) && i + 1 < ac) {
            listen_ms = u32(av[++i]);
        } else if (!_stricmp(av[i], "--help") || !_stricmp(av[i], "help")) {
            usage_gus_play_safe_official();
            return 0;
        } else {
            printf("gus-play-safe: unknown option: %s\n", av[i]);
            usage_gus_play_safe_official();
            return 2;
        }
    }

    GUS_WAV_INFO wi;
    uint8_t *buf = NULL;
    uint32_t n = 0;
    int vrc = 0;

    if (wav_read_info(path, &wi)) return 1;

    if (gus_wav_convert_window_to_xor80_noclamp_fullverify(
            path, &wi, sample_offset, sample_count, gain_x100, &buf, &n)) {
        return 1;
    }

    printf("gus-play-safe file=%s dram=0x%06x offset=%u samples=%u src_rate=%u bits=%u ch=%u gain=%u freq=0x%04x verify=%d play=%d listen_ms=%u\n",
           path, dram_addr, sample_offset, n, wi.sample_rate, wi.bits_per_sample,
           wi.channels, gain_x100, freq, verify, play, listen_ms);

    gus_print_u8_preview("encoded xor80 first bytes:", buf, n);
    wav_analyze_u8_buffer(buf, n);

    {
        uint32_t guard_start = (dram_addr >= 4096) ? dram_addr - 4096 : 0;
        uint32_t guard_count = n + 8192;
        puts("gus-play-safe: bounded guard-fill around target with 0x00");
        if (gus_guard_fill_range_fullverify(h, base, guard_start, guard_count, 0x00)) {
            free(buf);
            return 1;
        }
    }

    if (gus_upload_u8_buffer_safe(h, base, dram_addr, buf, n)) {
        free(buf);
        return 1;
    }

    if (verify) {
        vrc = gus_verify_dram_pages_fullverify(h, base, dram_addr, buf, n);
        if (vrc == 1) {
            free(buf);
            return 1;
        }
        if (vrc != 0) {
            printf("gus-play-safe: verify failed vrc=%d; not starting playback\n", vrc);
            free(buf);
            return vrc;
        }
    } else {
        puts("gus-play-safe: verify skipped");
    }

    free(buf);
    buf = NULL;

    if (play) {
        if (gus_enable_dac_sequence(h, base)) {
            return 1;
        }

        if (gus_start_voice_gf1addr(h, base, dram_addr, n,
                                    freq,
                                    0x00,   /* one-shot */
                                    0x0d,   /* active voices */
                                    0xf000, /* volume */
                                    0x00,   /* volctrl */
                                    0x08))  /* pan center */
        {
            return 1;
        }

        puts("gus-play-safe: started; no post-start polling/dumps");

        if (listen_ms > 0) {
            if (listen_ms > 60000) listen_ms = 60000;
            Sleep(listen_ms);
            if (gus_global_write8_high(h, base, 0x00, 0x03)) return 1;
            puts("gus-play-safe: auto-stopped");
        }
    } else {
        puts("gus-play-safe: no-play requested; upload complete");
    }

    return 0;
}
static int cmd_simple(HANDLE h, DWORD code) {
  DWORD r;
  return ioctl(h, code, NULL, 0, NULL, 0, &r) ? 0 : 1;
}
static int cmd_vout(HANDLE h, int ac, char **av) {
  if (ac < 4) {
    puts("vout <port> <value>");
    return 2;
  }
  IT8888_8237_PORT_OP op = {(uint16_t)u32(av[2]), (uint8_t)u32(av[3]), 0};
  DWORD r;
  return ioctl(h, IOCTL_IT8888_8237_OUT, &op, sizeof(op), NULL, 0, &r) ? 0 : 1;
}
static int cmd_vin(HANDLE h, int ac, char **av) {
  if (ac < 3) {
    puts("vin <port>");
    return 2;
  }
  IT8888_8237_PORT_OP op = {(uint16_t)u32(av[2]), 0, 0};
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_8237_IN, &op, sizeof(op), &op, sizeof(op), &r))
    return 1;
  printf("vin[%x]=0x%02x\n", op.Port, op.Value);
  return 0;
}
static int cmd_vsnap(HANDLE h) {
  IT8888_8237_SNAPSHOT s;
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_8237_SNAPSHOT, NULL, 0, &s, sizeof(s), &r))
    return 1;
  for (int i = 0; i < 8; i++) {
    printf("ch%d base=%04x cur=%04x count=%04x page=%02x mode=%02x mask=%u "
           "tc=%u addr=%08x bytes=%u dir=%u 16=%u\n",
           i, s.Ch[i].BaseAddr, s.Ch[i].CurAddr, s.Ch[i].CurCount, s.Ch[i].Page,
           s.Ch[i].Mode, s.Ch[i].Masked, s.Ch[i].TerminalCount,
           s.Ch[i].LegacyAddress, s.Ch[i].ByteCount, s.Ch[i].Direction,
           s.Ch[i].Is16Bit);
  }
  printf("mask0=%02x mask1=%02x ff0=%u ff1=%u status0=%02x status1=%02x\n",
         s.Mask0, s.Mask1, s.FlipFlop0, s.FlipFlop1, s.Status0, s.Status1);
  return 0;
}
static int cmd_vprepare(HANDLE h, int ac, char **av) {
  if (ac < 3) {
    puts("vprepare <ch>");
    return 2;
  }
  IT8888_8237_PREPARE p;
  memset(&p, 0, sizeof(p));
  p.Channel = (uint8_t)u32(av[2]);
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_8237_PREPARE, &p, sizeof(p), &p, sizeof(p), &r))
    return 1;
  printf("ch%u addr=%08x bytes=%u dir=%u mode=%02x masked=%u 16=%u\n",
         p.Channel, p.LegacyAddress, p.ByteCount, p.Direction, p.Mode, p.Masked,
         p.Is16Bit);
  return 0;
}
static void print_ddma(IT8888_DDMA_STATUS *s) {
  printf("armed=%u ch=%u dir=%u base=%04x logical=0x%llx count=%u flags=%08x "
         "cmd=%02x mode=%02x status=%02x pci=%04x complete=%u errors=%u\n",
         (unsigned)s->Armed, (unsigned)s->Channel, (unsigned)s->Direction,
         (unsigned)s->Base, (unsigned long long)s->LogicalAddress,
         (unsigned)s->Count, (unsigned)s->Flags, (unsigned)s->LastCommand,
         (unsigned)s->ModeReg, (unsigned)s->StatusReg,
         (unsigned)s->LastPciStatus, (unsigned)s->CompletionCount,
         (unsigned)s->ErrorCount);
}
static int cmd_ddma_arm(HANDLE h, int ac, char **av) {
  if (ac < 6) {
    puts("ddma-arm <ch> <dir:0/1/2> <buf-off> <count> [flags]");
    return 2;
  }
  IT8888_DDMA_REQUEST q = {
      (uint8_t)u32(av[2]), (uint8_t)u32(av[3]), 0,
      u32(av[4]),          u32(av[5]),          ac > 6 ? u32(av[6]) : 0};
  IT8888_DDMA_STATUS s;
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_DDMA_ARM, &q, sizeof(q), &s, sizeof(s), &r))
    return 1;
  print_ddma(&s);
  return 0;
}
static int cmd_ddma_status(HANDLE h, DWORD code) {
  IT8888_DDMA_STATUS s;
  DWORD r;
  if (!ioctl(h, code, NULL, 0, &s, sizeof(s), &r))
    return 1;
  print_ddma(&s);
  return 0;
}
static int cmd_irq_status(HANDLE h) {
  IT8888_IRQ_STATUS s;
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_IRQ_STATUS, NULL, 0, &s, sizeof(s), &r))
    return 1;
  printf("irq_count=%u pending=%u vector=%u status=%08x\n", s.IrqCount,
         s.Pending, s.LastVector, s.LastStatus);
  return 0;
}
static int cmd_wait_irq(HANDLE h, int ac, char **av) {
  IT8888_WAIT_IRQ_REQUEST q = {ac > 2 ? u32(av[2]) : 5000, 0};
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_WAIT_IRQ, &q, sizeof(q), NULL, 0, &r))
    return 1;
  puts("irq signaled");
  return 0;
}
static const char *tn(uint32_t t) {
  switch (t) {
  case IT8888_TRACE_INFO:
    return "INFO";
  case IT8888_TRACE_CFG_READ:
    return "CFGR";
  case IT8888_TRACE_CFG_WRITE:
    return "CFGW";
  case IT8888_TRACE_PORT_READ:
    return "IOR";
  case IT8888_TRACE_PORT_WRITE:
    return "IOW";
  case IT8888_TRACE_DMA:
    return "DMA";
  case IT8888_TRACE_VDMA:
    return "VDMA";
  case IT8888_TRACE_DDMA:
    return "DDMA";
  case IT8888_TRACE_IRQ:
    return "IRQ";
  case IT8888_TRACE_ERROR:
    return "ERR";
  default:
    return "?";
  }
}
static int cmd_trace(HANDLE h) {
  IT8888_TRACE_PACKET p;
  DWORD r;
  if (!ioctl(h, IOCTL_IT8888_TRACE_GET, NULL, 0, &p, sizeof(p), &r))
    return 1;
  printf("trace count=%u dropped=%u\n", p.Count, p.Dropped);
  for (uint32_t i = 0; i < p.Count; i++) {
    IT8888_TRACE_ENTRY *e = &p.Entries[i];
    printf("%llu %-5s a=%08x b=%llx c=%llx qpc=%llu\n",
           (unsigned long long)e->Sequence, tn(e->Type), e->A,
           (unsigned long long)e->B, (unsigned long long)e->C,
           (unsigned long long)e->Qpc);
  }
  return 0;
}
static void usage(void) {
  puts("it8888ctl commands:\n info dumpcfg init cfgread cfgwrite in out "
       "dma-alloc dma-info dma-fill dma-dump dma-check dma-free\n vreset vout vin vsnap vprepare\n "
       "ddma-arm ddma-start ddma-poll ddma-status ddma-clear\n irq-status "
       "irq-ack wait-irq\n trace trace-clear panic-reset clear-errors gus-dram-poke gus-dram-peek gus-dram-dump gus-dram-fill gus-dram-ramp gus-dram-square gus-voice-test gus-voice-loop-test gus-audible-test gus-wav-info gus-wav-load gus-wav-play gus-wav-load-safe gus-wav-play-safe gus-wav-play-once-safe gus-wav-play-loop-safe gus-wav-analyze-window gus-wav-play-window-safe gus-wav-play-guarded-poll-safe gus-wav-play-calibrated-safe gus-wav-analyze-window-signed gus-wav-play-window-signed-safe gus-gen-wave-test-safe gus-const-test-safe gus-gen-wave-period-test-safe gus-gen-wave-addrmode-test-safe gus-addrmode-sweep-safe gus-dram-unsigned-to-signed gus-dram-fill-safe gus-dram-square-safe gus-voice-dump gus-voice-stop ddma-probe-ch-safe gus-ddma-sweep-watch-ch ddma-probe-all(DISABLED) gus-dma-status-loop(DISABLED) gus-ddma-sweep-watch(DISABLED) ddma-probe-ch-safe gus-ddma-sweep-watch-ch gus-reg-read16 gus-reg-write16 gus-dma-kick gus-dma-sweep ddma-probe-all(DISABLED) gus-dma-status-loop(DISABLED) gus-ddma-sweep-watch(DISABLED) ddma-probe-ch-safe gus-ddma-sweep-watch-ch");
}
int main(int ac, char **av) {
  if (ac < 2) {
    usage();
    return 2;
  }
  HANDLE h = open_dev();
  if (h == INVALID_HANDLE_VALUE)
    return 1;
  int rc = 0;
  char *c = av[1];
#define IS(x) strcmp(c, x) == 0
  if (IS("info"))
    rc = cmd_info(h);
  else if (IS("dumpcfg"))
    rc = cmd_dumpcfg(h); else if (IS("pci-dumpcfg")) rc = cmd_pci_dumpcfg(h, ac, av); else if (IS("pci-cfgread")) rc = cmd_pci_cfgread(h, ac, av); else if (IS("pci-cfgwrite")) rc = cmd_pci_cfgwrite(h, ac, av); else if (IS("bridge-iowin")) rc = cmd_bridge_iowin(h, ac, av);
  else if (IS("init"))
    rc = cmd_init(h);
  else if (IS("cfgread"))
    rc = cmd_cfgread(h, ac, av);
  else if (IS("cfgwrite"))
    rc = cmd_cfgwrite(h, ac, av);
  else if (IS("in"))
    rc = cmd_in(h, ac, av);
  else if (IS("out"))
    rc = cmd_out(h, ac, av);
  else if (IS("dma-alloc"))
    rc = cmd_dma_alloc(h, ac, av);
  else if (IS("dma-info"))
    rc = cmd_dma_info(h); else if (IS("dma-fill")) rc = cmd_dma_fill(h, ac, av); else if (IS("dma-dump")) rc = cmd_dma_dump(h, ac, av); else if (IS("dma-check")) rc = cmd_dma_check(h, ac, av);
  else if (IS("dma-free"))
    rc = cmd_simple(h, IOCTL_IT8888_DMA_FREE);
  else if (IS("vreset"))
    rc = cmd_simple(h, IOCTL_IT8888_8237_RESET);
  else if (IS("vout"))
    rc = cmd_vout(h, ac, av);
  else if (IS("vin"))
    rc = cmd_vin(h, ac, av);
  else if (IS("vsnap"))
    rc = cmd_vsnap(h);
  else if (IS("vprepare"))
    rc = cmd_vprepare(h, ac, av);
  else if (IS("ddma-arm"))
    rc = cmd_ddma_arm(h, ac, av);
  else if (IS("ddma-start"))
    rc = cmd_ddma_status(h, IOCTL_IT8888_DDMA_START);
  else if (IS("ddma-poll"))
    rc = cmd_ddma_status(h, IOCTL_IT8888_DDMA_POLL);
  else if (IS("ddma-status"))
    rc = cmd_ddma_status(h, IOCTL_IT8888_DDMA_STATUS); else if (IS("ddma-r8")) rc = cmd_ddma_r8(h, ac, av); else if (IS("ddma-w8")) rc = cmd_ddma_w8(h, ac, av); else if (IS("ddma-probe")) rc = cmd_ddma_probe(h, ac, av);
  else if (IS("ddma-clear"))
    rc = cmd_simple(h, IOCTL_IT8888_DDMA_CLEAR);
  else if (IS("irq-status"))
    rc = cmd_irq_status(h);
  else if (IS("irq-ack"))
    rc = cmd_simple(h, IOCTL_IT8888_IRQ_ACK);
  else if (IS("wait-irq"))
    rc = cmd_wait_irq(h, ac, av);
  else if (IS("trace"))
    rc = cmd_trace(h);
  else if (IS("trace-clear"))
    rc = cmd_simple(h, IOCTL_IT8888_TRACE_CLEAR);
  else if (IS("panic-reset"))
    rc = cmd_simple(h, IOCTL_IT8888_PANIC_RESET);
  else if (IS("clear-errors"))
    rc = cmd_simple(h, IOCTL_IT8888_CLEAR_ERRORS);
  else if (IS("pgus-protocol")) rc = cmd_pgus_protocol(h, ac, av);
else if (IS("pgus-fwstring")) rc = cmd_pgus_fwstring(h, ac, av);
else if (IS("pgus-magic")) rc = cmd_pgus_magic(h, ac, av);
else if (IS("pgus-read8")) rc = cmd_pgus_read8(h, ac, av);
else if (IS("pgus-write8")) rc = cmd_pgus_write8(h, ac, av);
else if (IS("pgus-write16")) rc = cmd_pgus_write16(h, ac, av);
else if (IS("pgus-gus-get")) rc = cmd_pgus_gus_get(h, ac, av);
else if (IS("pgus-gus-set")) rc = cmd_pgus_gus_set(h, ac, av);
else if (IS("gus-probe")) rc = cmd_gus_probe(h, ac, av);
else if (IS("gus-reset")) rc = cmd_gus_reset(h, ac, av);else if (IS("gus-reg-read")) rc = cmd_gus_reg_read(h, ac, av);
else if (IS("gus-reg-write")) rc = cmd_gus_reg_write(h, ac, av);
else if (IS("gus-reg-probe")) rc = cmd_gus_reg_probe(h, ac, av);
else if (IS("gus-reg-reset-safe")) rc = cmd_gus_reg_reset_safe(h, ac, av);
else if (IS("gus-map-test")) rc = cmd_gus_map_test(h, ac, av);else if (IS("gus-reg-read16")) rc = cmd_gus_reg_read16(h, ac, av);
else if (IS("gus-reg-write16")) rc = cmd_gus_reg_write16(h, ac, av);
else if (IS("gus-dma-kick")) rc = cmd_gus_dma_kick(h, ac, av);
else if (IS("gus-dma-sweep")) rc = cmd_gus_dma_sweep(h, ac, av);else if (IS("ddma-probe-all")) rc = cmd_ddma_probe_all(h, ac, av);
else if (IS("gus-dma-status-loop")) rc = cmd_gus_dma_status_loop(h, ac, av);
else if (IS("gus-ddma-sweep-watch")) rc = cmd_gus_ddma_sweep_watch(h, ac, av);else if (IS("ddma-probe-ch-safe")) rc = cmd_ddma_probe_ch_safe(h, ac, av);
else if (IS("gus-ddma-sweep-watch-ch")) rc = cmd_gus_ddma_sweep_watch_ch(h, ac, av);else if (IS("gus-dram-poke")) rc = cmd_gus_dram_poke(h, ac, av);
else if (IS("gus-dram-peek")) rc = cmd_gus_dram_peek(h, ac, av);
else if (IS("gus-dram-dump")) rc = cmd_gus_dram_dump(h, ac, av);
else if (IS("gus-dram-fill")) rc = cmd_gus_dram_fill(h, ac, av);
else if (IS("gus-dram-ramp")) rc = cmd_gus_dram_ramp(h, ac, av);
else if (IS("gus-voice-test")) rc = cmd_gus_voice_test(h, ac, av);else if (IS("gus-dram-square")) rc = cmd_gus_dram_square(h, ac, av);
else if (IS("gus-voice-dump")) rc = cmd_gus_voice_dump(h, ac, av);
else if (IS("gus-voice-stop")) rc = cmd_gus_voice_stop(h, ac, av);
else if (IS("gus-voice-loop-test")) rc = cmd_gus_voice_loop_test(h, ac, av);else if (IS("gus-audible-test")) rc = cmd_gus_audible_test(h, ac, av);else if (IS("gus-wav-info")) rc = cmd_gus_wav_info(h, ac, av);
else if (IS("gus-wav-load")) rc = cmd_gus_wav_load(h, ac, av);
else if (IS("gus-wav-play")) rc = cmd_gus_wav_play(h, ac, av);else if (IS("gus-dram-fill-safe")) rc = cmd_gus_dram_fill_safe(h, ac, av);
else if (IS("gus-dram-square-safe")) rc = cmd_gus_dram_square_safe(h, ac, av);
else if (IS("gus-wav-load-safe")) rc = cmd_gus_wav_load_safe(h, ac, av);
else if (IS("gus-wav-play-safe")) rc = cmd_gus_wav_play_safe(h, ac, av);else if (IS("gus-wav-play-once-safe")) rc = cmd_gus_wav_play_once_safe(h, ac, av);
else if (IS("gus-wav-play-loop-safe")) rc = cmd_gus_wav_play_loop_safe(h, ac, av);else if (IS("gus-wav-analyze-window")) rc = cmd_gus_wav_analyze_window(h, ac, av);
else if (IS("gus-wav-play-window-safe")) rc = cmd_gus_wav_play_window_safe(h, ac, av);else if (IS("gus-wav-analyze-window-signed")) rc = cmd_gus_wav_analyze_window_signed(h, ac, av);
else if (IS("gus-wav-play-window-signed-safe")) rc = cmd_gus_wav_play_window_signed_safe(h, ac, av);
else if (IS("gus-dram-unsigned-to-signed")) rc = cmd_gus_dram_unsigned_to_signed(h, ac, av);else if (IS("gus-gen-wave-test-safe gus-const-test-safe gus-gen-wave-period-test-safe")) rc = cmd_gus_gen_wave_test_safe(h, ac, av);
else if (IS("gus-gen-wave-addrmode-test-safe")) rc = cmd_gus_gen_wave_addrmode_test_safe(h, ac, av);
else if (IS("gus-addrmode-sweep-safe")) rc = cmd_gus_addrmode_sweep_safe(h, ac, av);else if (IS("gus-gen-wave-addrmode-test-safe")) rc = cmd_gus_gen_wave_addrmode_test_safe(h, ac, av);
else if (IS("gus-addrmode-sweep-safe")) rc = cmd_gus_addrmode_sweep_safe(h, ac, av);else if (IS("gus-const-test-safe")) rc = cmd_gus_const_test_safe(h, ac, av);
else if (IS("gus-gen-wave-period-test-safe")) rc = cmd_gus_gen_wave_period_test_safe(h, ac, av);else if (IS("gus-wav-play-guarded-poll-safe")) rc = cmd_gus_wav_play_guarded_poll_safe(h, ac, av);else if (IS("gus-wav-play-calibrated-safe")) rc = cmd_gus_wav_play_calibrated_safe(h, ac, av);else if (IS("gus-output-sweep-safe")) rc = cmd_gus_output_sweep_safe(h, ac, av);else if (IS("gus-wav-play-loud-safe")) rc = cmd_gus_wav_play_loud_safe(h, ac, av);else if (IS("gus-wav-play-oldloud-loop-safe")) rc = cmd_gus_wav_play_oldloud_loop_safe(h, ac, av);else if (IS("gus-gf1addr-square-test-safe")) rc = cmd_gus_gf1addr_square_test_safe(h, ac, av);else if (IS("gus-wav-play-gf1addr-safe")) rc = cmd_gus_wav_play_gf1addr_safe(h, ac, av);else if (IS("gus-dram-hiaddr-test-safe")) rc = cmd_gus_dram_hiaddr_test_safe(h, ac, av);else if (IS("gus-wav-play64-gf1addr-safe")) rc = cmd_gus_wav_play64_gf1addr_safe(h, ac, av);else if (IS("gus-wav-play64-audition-safe")) rc = cmd_gus_wav_play64_audition_safe(h, ac, av);else if (IS("gus-wav-play64-quality-matrix-safe")) rc = cmd_gus_wav_play64_quality_matrix_safe(h, ac, av);else if (IS("gus-wav-play64-enc-audition-safe")) rc = cmd_gus_wav_play64_enc_audition_safe(h, ac, av);else if (IS("gus-wav-play64-clean-safe")) rc = cmd_gus_wav_play64_clean_safe(h, ac, av);else if (IS("gus-dram-hiaddr-mode-sweep-safe")) rc = cmd_gus_dram_hiaddr_mode_sweep_safe(h, ac, av);else if (IS("gus-wav-play-full-gf1-clean-safe")) rc = cmd_gus_wav_play_full_gf1_clean_safe(h, ac, av);else if (IS("gus-wav-upload-full-verify-safe")) rc = cmd_gus_wav_upload_full_verify_safe(h, ac, av);else if (IS("gus-play-safe")) rc = cmd_gus_play_safe_official(h, ac, av);else {
    usage();
    rc = 2;
  }
  CloseHandle(h);
  return rc;
}












































