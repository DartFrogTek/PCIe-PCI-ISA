# Removes installed IT8888VDMA driver packages whose original INF name is it8888vdma.inf.
# Run from an elevated PowerShell.
$ErrorActionPreference = 'Stop'
$current = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($current)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script as Administrator.'
}

Write-Host 'Disabling/removing present IT8888 devices if possible...'
Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like 'PCI\VEN_1283&DEV_8888*' -or $_.FriendlyName -like '*IT8888*' } | ForEach-Object {
    Write-Host "  Device: $($_.InstanceId) [$($_.Status)]"
}

Write-Host 'Scanning pnputil driver store for it8888vdma.inf...'
$text = pnputil /enum-drivers
$blocks = ($text -join "`n") -split "(?m)^Published Name\s*:"
$toDelete = @()
foreach ($b in $blocks) {
    if ($b -match '^(\s*\S+)' -and $b -match 'Original Name\s*:\s*it8888vdma\.inf') {
        $toDelete += $matches[1].Trim()
    }
}
if (-not $toDelete) {
    Write-Host 'No it8888vdma.inf packages found.'
    exit 0
}
foreach ($oem in $toDelete) {
    Write-Host "Deleting driver package $oem"
    pnputil /delete-driver $oem /uninstall /force
}
