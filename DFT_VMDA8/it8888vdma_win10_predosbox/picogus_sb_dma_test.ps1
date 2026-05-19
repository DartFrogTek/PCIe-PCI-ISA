# picogus_sb_dma_test.ps1
# Run from DFT_VMDA8\it8888vdma_win10_predosbox
#
# Experimental PicoGUS SB IRQ + DDMA playback test through:
#   Windows -> Intel Root Port -> IT8893 -> IT8888 -> ISA PicoGUS
#
# Defaults:
#   SB base = 0x220, IRQ 5, DMA 1, SB type 3 (SB 2.0), sample rate ~11025 Hz.
#
# Use:
#   powershell -ExecutionPolicy Bypass -File .\picogus_sb_dma_test.ps1
#   powershell -ExecutionPolicy Bypass -File .\picogus_sb_dma_test.ps1 -Irq 7 -Dma 1
#   powershell -ExecutionPolicy Bypass -File .\picogus_sb_dma_test.ps1 -RequestSbMode

param(
    [int]$RootBus = 0,
    [int]$RootDev = 28,
    [int]$RootFunc = 3,

    [int]$It8893Bus = 3,
    [int]$It8893Dev = 0,
    [int]$It8893Func = 0,

    [int]$SbPort = 0x220,
    [int]$SbAlias = 0x8220,

    [int]$PgusCtrl = 0x81D0,
    [int]$PgusDataLow = 0x81D1,
    [int]$PgusDataHigh = 0x81D2,

    [int]$Irq = 5,
    [int]$Dma = 1,
    [int]$SbType = 3,

    [int]$BufferBytes = 4096,
    [int]$ToneHalfPeriodBytes = 16,

    [switch]$RequestSbMode,
    [switch]$NoPlayback
)

$ErrorActionPreference = "Stop"

$Ctl = Join-Path (Get-Location) "dist\Debug_x64\it8888ctl.exe"
if (-not (Test-Path $Ctl)) {
    throw "Missing $Ctl. Run from project root after building Debug x64."
}

function RunCtl {
    param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Args)
    & $Ctl @Args
    if ($LASTEXITCODE -ne 0) {
        throw "it8888ctl failed: $($Args -join ' ') exit=$LASTEXITCODE"
    }
}

function TryCtl {
    param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Args)
    & $Ctl @Args
}

function Hex([int]$v) { "0x{0:x}" -f $v }

function Pgus-Knock() {
    RunCtl out (Hex $PgusCtrl) 1 0xCC
}

function Pgus-Write8([int]$cmd, [int]$val) {
    Pgus-Knock
    RunCtl out (Hex $PgusCtrl) 1 (Hex $cmd)
    RunCtl out (Hex $PgusDataHigh) 1 (Hex $val)
}

function Pgus-Write16([int]$cmd, [int]$val) {
    Pgus-Knock
    RunCtl out (Hex $PgusCtrl) 1 (Hex $cmd)
    RunCtl out (Hex $PgusDataLow) 2 (Hex $val)
}

function Sb-WriteDsp([int]$v) {
    $statusPort = $SbAlias + 0x0C
    for ($i = 0; $i -lt 1000; $i++) {
        $s = & $Ctl in (Hex $statusPort) 1
        if ($s -match "=0x([0-9a-fA-F]+)") {
            $b = [Convert]::ToInt32($Matches[1], 16)
            if (($b -band 0x80) -eq 0) {
                RunCtl out (Hex $statusPort) 1 (Hex $v)
                return
            }
        }
        Start-Sleep -Milliseconds 1
    }
    throw "SB DSP write timeout at port $(Hex $statusPort)"
}

function Sb-ReadDsp() {
    $dataPort = $SbAlias + 0x0A
    $availPort = $SbAlias + 0x0E
    for ($i = 0; $i -lt 1000; $i++) {
        $s = & $Ctl in (Hex $availPort) 1
        if ($s -match "=0x([0-9a-fA-F]+)") {
            $b = [Convert]::ToInt32($Matches[1], 16)
            if (($b -band 0x80) -ne 0) {
                & $Ctl in (Hex $dataPort) 1
                return
            }
        }
        Start-Sleep -Milliseconds 1
    }
    throw "SB DSP read timeout"
}

function Sb-Reset() {
    Write-Host "[sb] reset DSP through alias $(Hex $SbAlias)"
    RunCtl out (Hex ($SbAlias + 0x06)) 1 0x01
    Start-Sleep -Milliseconds 5
    RunCtl out (Hex ($SbAlias + 0x06)) 1 0x00
    Start-Sleep -Milliseconds 5
    Sb-ReadDsp
}

function Fill-SquareWave() {
    Write-Host "[dma] fill $BufferBytes bytes square wave, half-period=$ToneHalfPeriodBytes bytes"
    $off = 0
    $hi = $false
    while ($off -lt $BufferBytes) {
        $n = [Math]::Min($ToneHalfPeriodBytes, $BufferBytes - $off)
        $value = if ($hi) { 0xE0 } else { 0x20 }
        RunCtl dma-fill (Hex $off) $n (Hex $value)
        $off += $n
        $hi = -not $hi
    }
}

Write-Host "============================================================"
Write-Host "PicoGUS SB DMA/DDMA test"
Write-Host "Root port: $RootBus`:$RootDev.$RootFunc"
Write-Host "IT8893:    $It8893Bus`:$It8893Dev.$It8893Func"
Write-Host "SB alias:  $(Hex $SbAlias) -> logical SB $(Hex $SbPort)"
Write-Host "PGUS ctl:  $(Hex $PgusCtrl)"
Write-Host "IRQ/DMA:   IRQ $Irq DMA $Dma"
Write-Host "============================================================"

Write-Host "[bridge] open high I/O window 0x8000-0x8fff"
RunCtl bridge-iowin $RootBus $RootDev $RootFunc 0x8000 0x8fff
RunCtl bridge-iowin $It8893Bus $It8893Dev $It8893Func 0x8000 0x8fff

Write-Host "[it8888] map PicoGUS management control 0x1d0 -> 0x81d0 using cfg60"
RunCtl cfgwrite 0x60 4 0xe20081d0
RunCtl cfgread 0x60 4

Write-Host "[pgus] protocol"
Pgus-Knock
RunCtl out (Hex $PgusCtrl) 1 0x01
TryCtl in (Hex $PgusDataHigh) 1

Write-Host "[pgus] firmware string prefix"
Pgus-Knock
RunCtl out (Hex $PgusCtrl) 1 0x02
for ($i = 0; $i -lt 16; $i++) { TryCtl in (Hex $PgusDataHigh) 1 }

if ($RequestSbMode) {
    Write-Host "[pgus] request reboot to SB firmware/mode"
    Pgus-Knock
    RunCtl out (Hex $PgusCtrl) 1 0x03
    RunCtl out (Hex $PgusDataHigh) 1 0x05
    Start-Sleep -Milliseconds 100
    RunCtl out (Hex $PgusCtrl) 1 0xE2
    RunCtl out (Hex $PgusDataHigh) 1 0xFF
    Start-Sleep -Milliseconds 1500

    Write-Host "[pgus] after reboot, protocol"
    Pgus-Knock
    RunCtl out (Hex $PgusCtrl) 1 0x01
    TryCtl in (Hex $PgusDataHigh) 1
}

Write-Host "[pgus] configure SB resources through PicoGUS control protocol"
Pgus-Write16 0x06 $SbPort
Pgus-Write8  0x32 $Irq
Pgus-Write8  0x33 $Dma
Pgus-Write8  0x31 $SbType

Write-Host "[pgus] read back SB settings"
Pgus-Knock; RunCtl out (Hex $PgusCtrl) 1 0x06; TryCtl in (Hex $PgusDataLow) 2
Pgus-Knock; RunCtl out (Hex $PgusCtrl) 1 0x32; TryCtl in (Hex $PgusDataHigh) 1
Pgus-Knock; RunCtl out (Hex $PgusCtrl) 1 0x33; TryCtl in (Hex $PgusDataHigh) 1
Pgus-Knock; RunCtl out (Hex $PgusCtrl) 1 0x31; TryCtl in (Hex $PgusDataHigh) 1

Write-Host "[it8888] map SB alias 0x220 -> 0x8220"
RunCtl cfgwrite 0x58 4 0xe4008220
RunCtl cfgread 0x58 4

Write-Host "[sb] DSP reset"
RunCtl trace-clear
Sb-Reset
RunCtl trace

Write-Host "[irq] trigger SB 8-bit IRQ using DSP command 0xF2"
RunCtl trace-clear
TryCtl info
Sb-WriteDsp 0xF2
Start-Sleep -Milliseconds 50
TryCtl info
TryCtl in (Hex ($SbAlias + 0x0E)) 1
RunCtl trace

if ($NoPlayback) {
    Write-Host "[done] NoPlayback requested."
    exit 0
}

Write-Host "[dma] allocate/fill buffer"
TryCtl dma-info
try {
    RunCtl dma-alloc 65536
} catch {
    Write-Host "[dma] dma-alloc may already exist; continuing with dma-info"
}
RunCtl dma-info
Fill-SquareWave
RunCtl dma-dump 0 64

Write-Host "[ddma] arm channel $Dma for memory->device, count=$BufferBytes, flags=0x2c0"
RunCtl ddma-arm $Dma 2 0 $BufferBytes 0x2C0
RunCtl ddma-status

Write-Host "[sb] start 8-bit single-cycle DMA playback"
Sb-WriteDsp 0xD1
Sb-WriteDsp 0x40
Sb-WriteDsp 0xA5
$lenMinus1 = $BufferBytes - 1
Sb-WriteDsp 0x14
Sb-WriteDsp ($lenMinus1 -band 0xFF)
Sb-WriteDsp (($lenMinus1 -shr 8) -band 0xFF)

Start-Sleep -Milliseconds 500

Write-Host "[status] after playback window"
RunCtl ddma-status
TryCtl info
TryCtl in (Hex ($SbAlias + 0x0E)) 1
RunCtl trace

Write-Host "DONE. If routing/DMA/IRQ are correct, you may hear a short square-wave tone/click."
