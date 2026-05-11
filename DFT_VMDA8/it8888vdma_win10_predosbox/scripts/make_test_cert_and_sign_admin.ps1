param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [string]$CertName = 'IT8888VDMA Test Certificate'
)
$ErrorActionPreference = 'Stop'
$current = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($current)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script as Administrator.'
}

$Root = Resolve-Path (Join-Path $PSScriptRoot '..')
$Dist = Join-Path $Root "dist\${Configuration}_${Platform}"
$Sys = Join-Path $Dist 'it8888vdma.sys'
$Cat = Join-Path $Dist 'it8888vdma.cat'
$Inf = Join-Path $Dist 'it8888vdma.inf'
if (-not (Test-Path $Dist)) { throw "Missing dist folder: $Dist. Run build_all.bat first." }
if (-not (Test-Path $Sys)) { throw "Missing SYS: $Sys" }
if (-not (Test-Path $Inf)) { throw "Missing INF: $Inf" }

function Find-Signtool {
    $candidates = @()
    $kits = @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "${env:ProgramFiles}\Windows Kits\10\bin")
    foreach ($base in $kits) {
        if (Test-Path $base) {
            $candidates += Get-ChildItem -Path $base -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
                Sort-Object FullName -Descending
        }
    }
    if ($candidates.Count -eq 0) { throw 'signtool.exe not found. Install the Windows 10 SDK/WDK.' }
    return $candidates[0].FullName
}
function Find-Inf2Cat {
    $candidates = @()
    $kits = @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "${env:ProgramFiles}\Windows Kits\10\bin")
    foreach ($base in $kits) {
        if (Test-Path $base) {
            $candidates += Get-ChildItem -Path $base -Recurse -Filter inf2cat.exe -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match '\\x64\\inf2cat\.exe$' } |
                Sort-Object FullName -Descending
        }
    }
    if ($candidates.Count -eq 0) { return $null }
    return $candidates[0].FullName
}

$cert = Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.Subject -eq "CN=$CertName" } | Select-Object -First 1
if (-not $cert) {
    Write-Host "Creating test certificate: $CertName"
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=$CertName" -CertStoreLocation Cert:\LocalMachine\My -KeyUsage DigitalSignature -KeyExportPolicy Exportable -NotAfter (Get-Date).AddYears(10)
}

Write-Host 'Trusting certificate in LocalMachine Root and TrustedPublisher...'
$rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store('Root','LocalMachine')
$rootStore.Open('ReadWrite'); $rootStore.Add($cert); $rootStore.Close()
$pubStore = New-Object System.Security.Cryptography.X509Certificates.X509Store('TrustedPublisher','LocalMachine')
$pubStore.Open('ReadWrite'); $pubStore.Add($cert); $pubStore.Close()

$inf2cat = Find-Inf2Cat
if ($inf2cat) {
    Write-Host "Generating CAT with $inf2cat"
    & $inf2cat /driver:$Dist /os:10_X64
    if ($LASTEXITCODE -ne 0) { throw "inf2cat failed with $LASTEXITCODE" }
} else {
    Write-Warning 'inf2cat.exe not found; using existing CAT if present.'
}

$signtool = Find-Signtool
Write-Host "Signing with $signtool"
foreach ($file in @($Sys, $Cat) | Where-Object { Test-Path $_ }) {
    Write-Host "  signing $file"
    & $signtool sign /v /fd SHA256 /s My /n $CertName /tr http://timestamp.digicert.com /td SHA256 $file
    if ($LASTEXITCODE -ne 0) {
        Write-Warning 'Timestamp signing failed; retrying without timestamp.'
        & $signtool sign /v /fd SHA256 /s My /n $CertName $file
        if ($LASTEXITCODE -ne 0) { throw "signtool failed for $file" }
    }
}
Write-Host 'Signed. You still need test-signing mode enabled and a reboot before installation.'
