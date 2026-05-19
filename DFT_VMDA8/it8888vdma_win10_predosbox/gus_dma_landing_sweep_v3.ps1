# gus_dma_landing_sweep_v3.ps1
# Run from:
#   DFT_VMDA8\it8888vdma_win10_predosbox
#
# Purpose:
#   Focused DMA landing sweep after v2 showed:
#     - CPU port upload/GF1 playback is known-good
#     - DDMA arms
#     - gus-dma-kick runs
#     - but GUS DRAM remains unchanged
#
# This sweep is intentionally small/count=256 by default and automatically
# checks whether the target DRAM starts with the marker byte.
#
# Default sweep:
#   target = 0x0000
#   channel = 1
#   dir = 1,2
#   ctrl = common GF1 DMA control values
#
# Optional deeper sweep:
#   -Deep adds target 0x30000 and channel 3.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_sweep_v3.ps1
#
# Deep:
#   powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_sweep_v3.ps1 -Deep
#
# Custom:
#   powershell -ExecutionPolicy Bypass -File .\gus_dma_landing_sweep_v3.ps1 -Targets 0x0000,0x30000 -Channels 1,3 -Dirs 1,2

param(
    [string]$Exe = ".\dist\Debug_x64\it8888ctl.exe",
    [string]$Base = "0x8240",
    [string[]]$Targets = @("0x0000"),
    [int[]]$Channels = @(1),
    [int[]]$Dirs = @(1,2),
    [string[]]$Ctrls = @("0x01","0x03","0x05","0x09","0x0B","0x21","0x23","0x25","0x29","0x2B"),
    [string[]]$FlagsList = @("0x2C0"),
    [int]$Offset = 0,
    [int]$Count = 256,
    [int]$AllocSize = 65536,
    [int]$SettleMs = 250,
    [switch]$Deep,
    [switch]$StopOnSuccess,
    [switch]$NoTrace
)

$ErrorActionPreference = "Stop"

if ($Deep) {
    if ($Targets.Count -eq 1 -and $Targets[0] -eq "0x0000") {
        $Targets = @("0x0000","0x30000")
    }
    if ($Channels.Count -eq 1 -and $Channels[0] -eq 1) {
        $Channels = @(1,3)
    }
}

function Run-Ctl {
    param([string[]]$CmdArgs, [switch]$AllowFail)
    $out = & $Exe @CmdArgs 2>&1
    $code = $LASTEXITCODE
    if ($code -ne 0 -and -not $AllowFail) {
        Write-Host ($out -join "`n")
        throw "$Exe $($CmdArgs -join ' ') failed with exit code $code"
    }
    return ,@($code, $out)
}

function Marker-Hex {
    param([int]$CaseNo)
    # avoid 00, 11, 22, 33, 44, 55 from prior tests; use changing high-entropy marker
    $v = (0xA0 + ($CaseNo % 0x1F)) -band 0xFF
    if ($v -eq 0x00) { $v = 0xA5 }
    return ("0x{0:X2}" -f $v)
}

function Dump-Has-Marker {
    param([string[]]$DumpLines, [string]$Marker)
    $m = $Marker.ToLower().Replace("0x","")
    $joined = ($DumpLines -join "`n").ToLower()
    # success if first dump line includes at least eight repeated marker bytes
    $pattern = (" {0} {0} {0} {0} {0} {0} {0} {0}" -f $m)
    return $joined.Contains($pattern)
}

if (-not (Test-Path $Exe)) {
    throw "Missing $Exe. Run from project root or pass -Exe."
}

Write-Host "============================================================"
Write-Host "GUS DMA landing sweep v3"
Write-Host "Exe:       $Exe"
Write-Host "Base:      $Base"
Write-Host "Targets:   $($Targets -join ', ')"
Write-Host "Channels:  $($Channels -join ', ')"
Write-Host "Dirs:      $($Dirs -join ', ')"
Write-Host "Ctrls:     $($Ctrls -join ', ')"
Write-Host "Flags:     $($FlagsList -join ', ')"
Write-Host "Count:     $Count"
Write-Host "SettleMs:  $SettleMs"
Write-Host "============================================================"

# One initial allocation.
Write-Host ""
Write-Host "[setup] dma-alloc $AllocSize"
Run-Ctl @("dma-alloc", "$AllocSize") | Out-Null
Run-Ctl @("dma-info") | ForEach-Object { if ($_ -is [array]) { $_[1] } } | Out-Null

$caseNo = 0
$successes = @()

foreach ($target in $Targets) {
    foreach ($ch in $Channels) {
        foreach ($dir in $Dirs) {
            foreach ($flags in $FlagsList) {
                foreach ($ctrl in $Ctrls) {
                    $caseNo++
                    $marker = Marker-Hex $caseNo

                    Write-Host ""
                    Write-Host "------------------------------------------------------------"
                    Write-Host ("CASE {0}: target={1} ch={2} dir={3} flags={4} ctrl={5} marker={6}" -f $caseNo,$target,$ch,$dir,$flags,$ctrl,$marker)
                    Write-Host "------------------------------------------------------------"

                    Run-Ctl @("gus-voice-stop", $Base, "0") -AllowFail | Out-Null

                    # Clear target and also low 0 in case current gus-dma-kick masks high addresses internally.
                    Run-Ctl @("gus-dram-fill-safe", $Base, $target, "$Count", "0x00") | Out-Null
                    if ($target -ne "0x0000") {
                        Run-Ctl @("gus-dram-fill-safe", $Base, "0x0000", "$Count", "0x00") | Out-Null
                    }

                    Run-Ctl @("dma-fill", "$Offset", "$Count", $marker) | Out-Null
                    Run-Ctl @("ddma-clear") | Out-Null
                    if (-not $NoTrace) { Run-Ctl @("trace-clear") | Out-Null }

                    $arm = Run-Ctl @("ddma-arm", "$ch", "$dir", "$Offset", "$Count", $flags)
                    ($arm[1] -join "`n") | Write-Host

                    $kick = Run-Ctl @("gus-dma-kick", $Base, $target, $ctrl, "$SettleMs")
                    ($kick[1] -join "`n") | Write-Host

                    $status = Run-Ctl @("ddma-status")
                    ($status[1] -join "`n") | Write-Host

                    $dumpTarget = Run-Ctl @("gus-dram-dump", $Base, $target, "64")
                    $dumpTargetText = $dumpTarget[1]
                    ($dumpTargetText -join "`n") | Write-Host

                    $okTarget = Dump-Has-Marker $dumpTargetText $marker
                    $okLow = $false

                    if ($target -ne "0x0000") {
                        $dumpLow = Run-Ctl @("gus-dram-dump", $Base, "0x0000", "64")
                        $dumpLowText = $dumpLow[1]
                        Write-Host "[low 0x0000 check, because current gus-dma-kick may mask high target]"
                        ($dumpLowText -join "`n") | Write-Host
                        $okLow = Dump-Has-Marker $dumpLowText $marker
                    }

                    if ($okTarget -or $okLow) {
                        $where = if ($okTarget) { $target } else { "0x0000" }
                        $msg = "SUCCESS case=$caseNo landed_at=$where target=$target ch=$ch dir=$dir flags=$flags ctrl=$ctrl marker=$marker"
                        Write-Host ""
                        Write-Host "************************************************************"
                        Write-Host $msg
                        Write-Host "************************************************************"
                        $successes += $msg
                        if ($StopOnSuccess) {
                            Write-Host "StopOnSuccess set; exiting."
                            exit 0
                        }
                    }
                }
            }
        }
    }
}

Write-Host ""
Write-Host "============================================================"
Write-Host "Sweep complete"
Write-Host "============================================================"
if ($successes.Count -eq 0) {
    Write-Host "No DMA landing detected."
    Write-Host ""
    Write-Host "Interpretation:"
    Write-Host "  DDMA can be armed, and GUS DMA control changes, but no bytes arrive in GUS DRAM."
    Write-Host "  Next source patch should instrument/fix gus-dma-kick:"
    Write-Host "    - print requested target vs programmed GUS DMA address"
    Write-Host "    - sweep GUS DMA address scaling/address register 0x42 values"
    Write-Host "    - verify PicoGUS/GUS DMA channel mapping from ULTRASND"
} else {
    $successes | ForEach-Object { Write-Host $_ }
}
