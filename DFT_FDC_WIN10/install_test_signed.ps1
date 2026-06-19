param(
    [string]$Config = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$dist = Join-Path $PSScriptRoot "dist\${Config}_${Platform}"
$inf = Join-Path $dist "dftfdc.inf"

if (!(Test-Path $inf)) {
    throw "INF not found: $inf. Build first."
}

Write-Host "Installing $inf"
pnputil /add-driver $inf /install
