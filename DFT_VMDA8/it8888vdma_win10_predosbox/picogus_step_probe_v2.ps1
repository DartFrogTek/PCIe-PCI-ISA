# picogus_step_probe_v2.ps1
# Safe staged PicoGUS probe. Run from DFT_VMDA8\it8888vdma_win10_predosbox.
#
# This avoids the previous likely hang:
#   DO NOT do 16-bit OUT to odd port 0x81D1.
#   PicoGUS word writes are sent as two byte writes: low -> 0x81D1, high -> 0x81D2.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\picogus_step_probe_v2.ps1
#   powershell -ExecutionPolicy Bypass -File .\picogus_step_probe_v2.ps1 -DoSbReset
#   powershell -ExecutionPolicy Bypass -File .\picogus_step_probe_v2.ps1 -DoIrqTest
#
# Start with no switches. If that succeeds, try -DoSbReset. Then -DoIrqTest.

param(
    [int]$RootBus = 0,
    [int]$RootDev = 28,
    [int]$RootFunc = 3,

    [int]$It8893Bus = 3,
    [int]$It8893Dev = 0,
    [int]$It8893Func = 0,

    [int]$PgusCtrl = 0x81D0,
    [int]$PgusDataLow = 0x81D1,
    [int]$PgusDataHigh = 0x81D2,

    [int]$SbBase = 0x8220,
    [int]$SbLogical = 0x220,

    [int]$Irq = 5,
    [int]$Dma = 1,
    [int]$SbType = 3,

    [int]$TimeoutMs = 2500,

    [switch]$DoSbReset,
    [switch]$DoIrqTest
)

$ErrorActionPreference = "Stop"
$Ctl = Join-Path (Get-Location) "dist\Debug_x64\it8888ctl.exe"
if (-not (Test-Path $Ctl)) { throw "Missing $Ctl" }

function Hex([int]$v) { "0x{0:x}" -f $v }

function Ctl {
    param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Args)

    $cmdline = "$Ctl " + ($Args -join " ")
    Write-Host ">>> $cmdline"

    $p = Start-Process -FilePath $Ctl -ArgumentList $Args -NoNewWindow -PassThru -RedirectStandardOutput "$env:TEMP\it8888ctl_out.txt" -RedirectStandardError "$env:TEMP\it8888ctl_err.txt"
    if (-not $p.WaitForExit($TimeoutMs)) {
        try { $p.Kill() } catch {}
        Write-Host "TIMEOUT/HANG after $TimeoutMs ms: $($Args -join ' ')" -ForegroundColor Red
        Write-Host "Last stdout:"
        if (Test-Path "$env:TEMP\it8888ctl_out.txt") { Get-Content "$env:TEMP\it8888ctl_out.txt" }
        Write-Host "Last stderr:"
        if (Test-Path "$env:TEMP\it8888ctl_err.txt") { Get-Content "$env:TEMP\it8888ctl_err.txt" }
        throw "stopping at hanging command"
    }

    $out = ""
    $err = ""
    if (Test-Path "$env:TEMP\it8888ctl_out.txt") { $out = Get-Content "$env:TEMP\it8888ctl_out.txt" -Raw }
    if (Test-Path "$env:TEMP\it8888ctl_err.txt") { $err = Get-Content "$env:TEMP\it8888ctl_err.txt" -Raw }
    if ($out) { Write-Host $out.TrimEnd() }
    if ($err) { Write-Host $err.TrimEnd() -ForegroundColor Yellow }
    if ($p.ExitCode -ne 0) { throw "it8888ctl exit $($p.ExitCode): $($Args -join ' ')" }

    return $out
}

function Pgus-Knock {
    Ctl out (Hex $PgusCtrl) 1 0xCC | Out-Null
}

function Pgus-Read8([int]$cmd) {
    Pgus-Knock
    Ctl out (Hex $PgusCtrl) 1 (Hex $cmd) | Out-Null
    Ctl in (Hex $PgusDataHigh) 1
}

function Pgus-Write8([int]$cmd, [int]$value) {
    Pgus-Knock
    Ctl out (Hex $PgusCtrl) 1 (Hex $cmd) | Out-Null
    Ctl out (Hex $PgusDataHigh) 1 (Hex $value) | Out-Null
}

function Pgus-Write16-Safe([int]$cmd, [int]$value) {
    # Important: avoid 16-bit OUT to odd address 0x81D1.
    # Do byte writes instead: low byte to DATA_LOW, high byte to DATA_HIGH.
    $lo = $value -band 0xFF
    $hi = ($value -shr 8) -band 0xFF
    Pgus-Knock
    Ctl out (Hex $PgusCtrl) 1 (Hex $cmd) | Out-Null
    Ctl out (Hex $PgusDataLow) 1 (Hex $lo) | Out-Null
    Ctl out (Hex $PgusDataHigh) 1 (Hex $hi) | Out-Null
}

function Sb-Reset-NoLoop {
    Ctl trace-clear | Out-Null
    Ctl out (Hex ($SbBase + 0x06)) 1 0x01 | Out-Null
    Start-Sleep -Milliseconds 5
    Ctl out (Hex ($SbBase + 0x06)) 1 0x00 | Out-Null
    Start-Sleep -Milliseconds 10
    Ctl in (Hex ($SbBase + 0x0E)) 1 | Out-Null
    Ctl in (Hex ($SbBase + 0x0A)) 1 | Out-Null
    Ctl trace | Out-Null
}

Write-Host "============================================================"
Write-Host "PicoGUS staged probe v2"
Write-Host "No 16-bit OUT to odd 0x81D1; all commands timeout."
Write-Host "============================================================"

Write-Host "[1] Open safe high bridge window"
Ctl bridge-iowin $RootBus $RootDev $RootFunc 0x8000 0x8fff | Out-Null
Ctl bridge-iowin $It8893Bus $It8893Dev $It8893Func 0x8000 0x8fff | Out-Null

Write-Host "[2] Map PicoGUS control alias 0x1D0 -> 0x81D0"
Ctl cfgwrite 0x60 4 0xe20081d0 | Out-Null
Ctl cfgread 0x60 4 | Out-Null

Write-Host "[3] Protocol read"
Pgus-Read8 0x01 | Out-Null

Write-Host "[4] Firmware string prefix"
Pgus-Knock
Ctl out (Hex $PgusCtrl) 1 0x02 | Out-Null
for ($i = 0; $i -lt 12; $i++) { Ctl in (Hex $PgusDataHigh) 1 | Out-Null }

Write-Host "[5] Configure PicoGUS SB resources safely"
Write-Host "    SBPORT=$([String]::Format('0x{0:x}', $SbLogical)) IRQ=$Irq DMA=$Dma TYPE=$SbType"
Pgus-Write16-Safe 0x06 $SbLogical
Pgus-Write8 0x32 $Irq
Pgus-Write8 0x33 $Dma
Pgus-Write8 0x31 $SbType

Write-Host "[6] Read back SB resource bytes"
Pgus-Read8 0x32 | Out-Null
Pgus-Read8 0x33 | Out-Null
Pgus-Read8 0x31 | Out-Null

Write-Host "[7] Map SB alias 0x220 -> 0x8220"
Ctl cfgwrite 0x58 4 0xe4008220 | Out-Null
Ctl cfgread 0x58 4 | Out-Null

if ($DoSbReset) {
    Write-Host "[8] SB reset no polling loop"
    Sb-Reset-NoLoop
} else {
    Write-Host "[8] Skipping SB reset. Add -DoSbReset next."
}

if ($DoIrqTest) {
    Write-Host "[9] IRQ test via DSP command 0xF2, no polling"
    Ctl trace-clear | Out-Null
    Ctl info | Out-Null
    Ctl out (Hex ($SbBase + 0x0C)) 1 0xF2 | Out-Null
    Start-Sleep -Milliseconds 50
    Ctl info | Out-Null
    Ctl in (Hex ($SbBase + 0x0E)) 1 | Out-Null
    Ctl trace | Out-Null
} else {
    Write-Host "[9] Skipping IRQ test. Add -DoIrqTest after SB reset works."
}

Write-Host "DONE"
