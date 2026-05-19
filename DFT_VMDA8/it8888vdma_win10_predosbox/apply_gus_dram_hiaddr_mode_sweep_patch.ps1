# apply_gus_dram_hiaddr_mode_sweep_patch.ps1
# Run from:
#   DFT_VMDA8\it8888vdma_win10_predosbox
#
# Adds:
#
#   gus-dram-hiaddr-mode-sweep-safe <base>
#
# Purpose:
#   We proved real clean WAV playback works through the 64 KiB aperture:
#     DRAM addr=0x0000, count<=65536, GF1 packed voice addr, xor80 encoding.
#
#   Now we test whether PicoGUS/GF1 high DRAM addressing can be made to work
#   through the port-write upload path. Current behavior mirrors every 0x10000.
#
# This command writes unique markers at:
#   0x00000 -> 0x11
#   0x08000 -> 0x22
#   0x10000 -> 0x33
#   0x18000 -> 0x44
#   0x20000 -> 0x55
#
# using several alternate high-address latch sequences, then reads them back
# with the same mode. A working mode should show:
#
#   000000: 11 ...
#   008000: 22 ...
#   010000: 33 ...
#   018000: 44 ...
#   020000: 55 ...
#
# If every mode still shows 55/44/55/44/55, then the port upload path is
# effectively a 64 KiB aperture and we should build a clean chunk streamer
# around gus-wav-play64-clean-safe.

$ErrorActionPreference = "Stop"

$Root = Get-Location
$CtlC = Join-Path $Root "tools\it8888ctl\it8888ctl.c"

if (-not (Test-Path $CtlC)) {
    throw "Missing $CtlC. Run this from project root."
}

$text = Get-Content $CtlC -Raw

if ($text -match "cmd_gus_dram_hiaddr_mode_sweep_safe") {
    Write-Host "gus-dram-hiaddr-mode-sweep-safe already appears present."
    exit 0
}

foreach ($sym in @("port_out8", "port_in8")) {
    if ($text.IndexOf($sym) -lt 0) {
        throw "Missing helper $sym."
    }
}

$insert = @'

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

'@

$pos = $text.IndexOf("static int cmd_simple(HANDLE h, DWORD code)")
if ($pos -lt 0) {
    throw "Could not find insertion point before cmd_simple."
}
$text = $text.Insert($pos, $insert)

$dispatch = @'
else if (IS("gus-dram-hiaddr-mode-sweep-safe")) rc = cmd_gus_dram_hiaddr_mode_sweep_safe(h, ac, av);
'@

if ($text -notmatch 'IS\("gus-dram-hiaddr-mode-sweep-safe"\)') {
    $patterns = @(
        'else\s+if\s*\(\s*IS\s*\(\s*"help"\s*\)\s*\)',
        'else\s*\{\s*usage\s*\(\s*\)\s*;\s*rc\s*=\s*2\s*;\s*\}',
        'else\s*\{\s*usage\s*\(\s*\)\s*;\s*\}',
        'if\s*\(\s*IS\s*\(\s*"help"\s*\)'
    )

    $done = $false
    foreach ($pat in $patterns) {
        $m = [regex]::Match($text, $pat)
        if ($m.Success) {
            $text = $text.Insert($m.Index, $dispatch)
            $done = $true
            break
        }
    }

    if (-not $done) {
        throw "Could not find dispatch insertion point. Paste main()."
    }
}

Set-Content -Path $CtlC -Value $text -Encoding ASCII

Write-Host "Patched tools\it8888ctl\it8888ctl.c with gus-dram-hiaddr-mode-sweep-safe."
Write-Host ""
Write-Host "Rebuild/reinstall:"
Write-Host "  .\install_rebuild_it8888vdma_signed.bat Debug x64"
Write-Host ""
Write-Host "Run:"
Write-Host "  .\dist\Debug_x64\it8888ctl.exe gus-dram-hiaddr-mode-sweep-safe 0x8240"
Write-Host ""
Write-Host "Look for any mode where 000000/008000/010000/018000/020000 read back as 11/22/33/44/55."
