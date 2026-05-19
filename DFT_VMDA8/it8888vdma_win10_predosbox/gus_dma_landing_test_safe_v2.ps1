# gus_dma_landing_test_safe_v2.ps1
# Run from:
#   DFT_VMDA8\it8888vdma_win10_predosbox
#
# Fixes v1 bug:
#   The helper used parameter name $Args, which collides/confuses PowerShell's
#   automatic $args variable. Commands were invoked with no arguments, so
#   it8888ctl only printed usage and exited 2.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_test_safe_v2.ps1
#
# Variants:
#   powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_test_safe_v2.ps1 -Target 0x0000 -Ctrl 0x01
#   powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_test_safe_v2.ps1 -Target 0x30000 -Ctrl 0x03
#   powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_test_safe_v2.ps1 -Target 0x30000 -Dir 1 -Ctrl 0x01

param(
    [string]$Exe = ".\dist\Debug_x64\it8888ctl.exe",
    [string]$Base = "0x8240",
    [string]$Target = "0x30000",
    [int]$Count = 4096,
    [string]$Value = "0xA5",
    [int]$Channel = 1,
    [int]$Dir = 2,
    [int]$Offset = 0,
    [string]$Flags = "0x2C0",
    [string]$Ctrl = "0x01",
    [int]$SettleMs = 250,
    [int]$AllocSize = 65536,
    [switch]$SkipAlloc,
    [switch]$NoTrace
)

$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [string]$Name,
        [string[]]$CmdArgs,
        [switch]$IgnoreFailure
    )

    Write-Host ""
    Write-Host "============================================================"
    Write-Host $Name
    Write-Host "------------------------------------------------------------"
    Write-Host "$Exe $($CmdArgs -join ' ')"
    Write-Host "============================================================"

    & $Exe @CmdArgs
    $code = $LASTEXITCODE
    if ($code -ne 0 -and -not $IgnoreFailure) {
        throw "$Name failed with exit code $code"
    }
    if ($code -ne 0 -and $IgnoreFailure) {
        Write-Host "[ignored] $Name exit code $code"
    }
}

if (-not (Test-Path $Exe)) {
    throw "Missing $Exe. Run this from the project root or pass -Exe."
}

Write-Host "============================================================"
Write-Host "GUS DMA landing test v2"
Write-Host "Exe:       $Exe"
Write-Host "Base:      $Base"
Write-Host "Target:    $Target"
Write-Host "Count:     $Count"
Write-Host "Value:     $Value"
Write-Host "Channel:   $Channel"
Write-Host "Dir:       $Dir"
Write-Host "Offset:    $Offset"
Write-Host "Flags:     $Flags"
Write-Host "Ctrl:      $Ctrl"
Write-Host "SettleMs:  $SettleMs"
Write-Host "============================================================"

Invoke-Step "Stop voice 0" @("gus-voice-stop", $Base, "0") -IgnoreFailure

Invoke-Step "Clear GUS DRAM target" @("gus-dram-fill-safe", $Base, $Target, "$Count", "0x00")
Invoke-Step "Dump target before DMA" @("gus-dram-dump", $Base, $Target, "128")

if (-not $SkipAlloc) {
    Invoke-Step "Allocate DMA buffer" @("dma-alloc", "$AllocSize")
} else {
    Write-Host ""
    Write-Host "[skip] dma-alloc"
}

Invoke-Step "Fill DMA buffer marker" @("dma-fill", "$Offset", "$Count", $Value)
Invoke-Step "DMA info" @("dma-info")

Invoke-Step "DDMA clear" @("ddma-clear")
if (-not $NoTrace) {
    Invoke-Step "Trace clear" @("trace-clear")
}

Invoke-Step "Arm DDMA" @("ddma-arm", "$Channel", "$Dir", "$Offset", "$Count", $Flags)
Invoke-Step "DDMA status after arm" @("ddma-status")
Invoke-Step "DDMA probe channel before kick" @("ddma-probe-ch-safe", "$Channel")

Invoke-Step "Kick GUS DMA" @("gus-dma-kick", $Base, $Target, $Ctrl, "$SettleMs")

Invoke-Step "DDMA status after kick" @("ddma-status")
Invoke-Step "IRQ status" @("irq-status")
Invoke-Step "DDMA probe channel after kick" @("ddma-probe-ch-safe", "$Channel")

if (-not $NoTrace) {
    Invoke-Step "Trace" @("trace")
}

Invoke-Step "Dump target after DMA" @("gus-dram-dump", $Base, $Target, "256")

Write-Host ""
Write-Host "============================================================"
Write-Host "Interpretation"
Write-Host "============================================================"
Write-Host "SUCCESS: dump at $Target starts with repeated $Value bytes."
Write-Host "FAIL:    dump remains 00/80/old bytes, or DDMA status/errors never change."
Write-Host ""
Write-Host "If fail, next sweeps:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_test_safe_v2.ps1 -Target 0x0000 -Ctrl 0x01"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_test_safe_v2.ps1 -Target 0x30000 -Ctrl 0x03"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_test_safe_v2.ps1 -Target 0x30000 -Dir 1 -Ctrl 0x01"
Write-Host "============================================================"
