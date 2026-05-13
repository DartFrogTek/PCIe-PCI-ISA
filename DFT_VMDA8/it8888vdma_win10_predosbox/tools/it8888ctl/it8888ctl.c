#define WIN32_LEAN_AND_MEAN
#include "public.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

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
       "irq-ack wait-irq\n trace trace-clear panic-reset clear-errors");
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
    rc = cmd_dumpcfg(h);
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
  else {
    usage();
    rc = 2;
  }
  CloseHandle(h);
  return rc;
}



