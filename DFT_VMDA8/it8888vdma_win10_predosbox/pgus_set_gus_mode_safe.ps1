# pgus_set_gus_mode_safe.ps1
# Set PicoGUS boot mode to GUS and save, then restore bridges.
#
# IMPORTANT:
#   Default behavior does NOT send CMD_REBOOT.
#   After this script succeeds, do a full power-cycle / cold reboot so PicoGUS
#   comes back in GUS mode cleanly.
#
# Run:
#   powershell -ExecutionPolicy Bypass -File .\pgus_set_gus_mode_safe.ps1
#
# Optional dangerous mode, not recommended unless you are testing:
#   powershell -ExecutionPolicy Bypass -File .\pgus_set_gus_mode_safe.ps1 -RebootCard

param(
    [switch]$RebootCard
)

$ErrorActionPreference = "Stop"
$Ctl = Join-Path (Get-Location) "dist\Debug_x64\it8888ctl.exe"
if (-not (Test-Path $Ctl)) { throw "Missing $Ctl" }

function Ctl([Parameter(ValueFromRemainingArguments=$true)][string[]]$Args) {
    & $Ctl @Args
    if ($LASTEXITCODE -ne 0) {
        throw "it8888ctl failed: $($Args -join ' ') exit=$LASTEXITCODE"
    }
}

try {
    Write-Host "[pgus-safe] Open high bridge window only..."
    Ctl bridge-iowin 0 28 3 0x8000 0x8fff
    Ctl bridge-iowin 3 0 0 0x8000 0x8fff

    Write-Host "[pgus-safe] Map PicoGUS control 0x1D0 -> 0x81D0..."
    Ctl cfgwrite 0x60 4 0xe20081d0

    Write-Host "[pgus-safe] Optional pre-check: protocol/fwstring..."
    Ctl pgus-protocol
    Ctl pgus-fwstring 32

    Write-Host "[pgus-safe] Set CMD_BOOTMODE=GUS_MODE(1)..."
    Ctl pgus-write8 0x03 0x01

    Write-Host "[pgus-safe] Save settings, CMD_SAVE=0xE1..."
    Ctl pgus-write8 0xE1 0xFF

    if ($RebootCard) {
        Write-Host "[pgus-safe] WARNING: sending CMD_REBOOT. No reads after this."
        Ctl pgus-write8 0xE2 0xFF
        Start-Sleep -Milliseconds 500
    } else {
        Write-Host "[pgus-safe] Not sending CMD_REBOOT. Do a full cold reboot/power-cycle next."
    }
}
finally {
    Write-Host "[pgus-safe] Restore bridge windows with write-only restore..."
    & powershell -ExecutionPolicy Bypass -File .\restore_8888_8893_min.ps1
}

Write-Host "[pgus-safe] Done."
if (-not $RebootCard) {
    Write-Host "Now power off fully, wait a few seconds, power back on, run init, then check pgus-fwstring."
}
