# restore_8888_8893_min.ps1
# Minimal bridge restore: WRITE ONLY, no pci-dumpcfg, no info, no IT8888 reads.
# Use this after dangerous tests or after PicoGUS reboot/save commands.
#
# Run from DFT_VMDA8\it8888vdma_win10_predosbox:
#   powershell -ExecutionPolicy Bypass -File .\restore_8888_8893_min.ps1

$ErrorActionPreference = "Stop"
$Ctl = Join-Path (Get-Location) "dist\Debug_x64\it8888ctl.exe"
if (-not (Test-Path $Ctl)) { throw "Missing $Ctl" }

function Ctl([Parameter(ValueFromRemainingArguments=$true)][string[]]$Args) {
    & $Ctl @Args
    if ($LASTEXITCODE -ne 0) {
        throw "it8888ctl failed: $($Args -join ' ') exit=$LASTEXITCODE"
    }
}

Write-Host "[restore-min] Disabling root-port I/O forwarding..."
Ctl pci-cfgwrite 0 28 3 0x04 2 0x0404
Ctl pci-cfgwrite 0 28 3 0x1c 1 0xf0
Ctl pci-cfgwrite 0 28 3 0x1d 1 0x00
Ctl pci-cfgwrite 0 28 3 0x30 2 0x0000
Ctl pci-cfgwrite 0 28 3 0x32 2 0x0000

Write-Host "[restore-min] Disabling IT8893 I/O forwarding..."
Ctl pci-cfgwrite 3 0 0 0x04 2 0x0407
Ctl pci-cfgwrite 3 0 0 0x1c 1 0xf1
Ctl pci-cfgwrite 3 0 0 0x1d 1 0x01
Ctl pci-cfgwrite 3 0 0 0x30 2 0x0000
Ctl pci-cfgwrite 3 0 0 0x32 2 0x0000

Write-Host "[restore-min] Done. No verification reads were performed."
