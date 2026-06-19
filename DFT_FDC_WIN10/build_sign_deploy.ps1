param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [switch]$SkipBuild,
    [switch]$SkipInstall,
    [switch]$SkipSign,
    [switch]$RunVersionTest
)

$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Text)
    Write-Host ""
    Write-Host "==== $Text ====" -ForegroundColor Cyan
}

function Test-IsAdmin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-ProjectRoot {
    if ($PSScriptRoot -and (Test-Path $PSScriptRoot)) {
        return (Resolve-Path $PSScriptRoot).Path
    }
    return (Get-Location).Path
}

$ProjectRoot = Get-ProjectRoot
Set-Location $ProjectRoot

if (-not (Test-IsAdmin)) {
    Write-Host "Re-launching elevated..." -ForegroundColor Yellow
    $argList = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', ('"{0}"' -f $PSCommandPath),
        '-Configuration', ('"{0}"' -f $Configuration),
        '-Platform', ('"{0}"' -f $Platform)
    )
    if ($SkipBuild)      { $argList += '-SkipBuild' }
    if ($SkipInstall)    { $argList += '-SkipInstall' }
    if ($SkipSign)       { $argList += '-SkipSign' }
    if ($RunVersionTest) { $argList += '-RunVersionTest' }
    Start-Process -FilePath "powershell.exe" -ArgumentList ($argList -join ' ') -WorkingDirectory $ProjectRoot -Verb RunAs
    exit 0
}

Write-Host "DFT_FDC_WIN10 build/sign/deploy"
Write-Host "Project root: $ProjectRoot"
Write-Host "Configuration: $Configuration"
Write-Host "Platform: $Platform"

if ($Platform -ne "x64") {
    throw "Only x64 is supported by this pass-1 deploy script."
}

$DriverInf = Join-Path $ProjectRoot "driver\dftfdc.inf"
$DistDir = Join-Path $ProjectRoot "dist\${Configuration}_${Platform}"
$DistInf = Join-Path $DistDir "dftfdc.inf"
$DistCat = Join-Path $DistDir "dftfdc.cat"
$BuildBat = Join-Path $ProjectRoot "build_all_direct.bat"
$KitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10"

function Invoke-Checked {
    param(
        [Parameter(Mandatory=$true)][string]$Exe,
        [string[]]$Args = @(),
        [string]$WorkingDirectory = $null
    )

    $old = Get-Location
    if ($WorkingDirectory) {
        Set-Location $WorkingDirectory
    }
    try {
        Write-Host "> `"$Exe`" $($Args -join ' ')"
        & $Exe @Args
        $code = $LASTEXITCODE
        if ($null -eq $code) { $code = 0 }
        if ($code -ne 0) {
            throw "Command failed with exit code $code`: $Exe $($Args -join ' ')"
        }
    }
    finally {
        Set-Location $old
    }
}

function Invoke-CmdChecked {
    param(
        [Parameter(Mandatory=$true)][string]$CommandLine,
        [string]$WorkingDirectory = $null
    )

    $old = Get-Location
    if ($WorkingDirectory) {
        Set-Location $WorkingDirectory
    }
    try {
        Write-Host "> cmd.exe /c $CommandLine"
        & cmd.exe /c $CommandLine
        $code = $LASTEXITCODE
        if ($code -ne 0) {
            throw "Command failed with exit code $code`: cmd.exe /c $CommandLine"
        }
    }
    finally {
        Set-Location $old
    }
}

function Get-BuildCompatibleKmdfVersion {
    # This intentionally mimics build_all_direct.bat behavior:
    #   dir /b /ad ... | sort /r
    # That means string-sort descending, not semantic-version descending.
    # On the test PC this correctly selects 1.9 instead of 1.35.
    $incRoot = Join-Path $KitsRoot "Include\wdf\kmdf"
    $libRootA = Join-Path $KitsRoot "Lib\wdf\kmdf\x64"
    $libRootB = Join-Path $KitsRoot "Lib\wdf\kmdf"

    if (-not (Test-Path $incRoot)) {
        throw "KMDF include root not found: $incRoot"
    }

    $dirs = @(Get-ChildItem -Path $incRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)
    foreach ($d in $dirs) {
        $ver = $d.Name
        $wdfh = Join-Path $d.FullName "wdf.h"
        $libA = Join-Path $libRootA "$ver\WdfDriverEntry.lib"
        $ldrA = Join-Path $libRootA "$ver\WdfLdr.lib"
        $libB = Join-Path $libRootB "$ver\x64\WdfDriverEntry.lib"
        $ldrB = Join-Path $libRootB "$ver\x64\WdfLdr.lib"
        if ((Test-Path $wdfh) -and (((Test-Path $libA) -and (Test-Path $ldrA)) -or ((Test-Path $libB) -and (Test-Path $ldrB)))) {
            return $ver
        }
    }

    # Last fallback for unusual WDK layouts with unversioned KMDF include/lib.
    if ((Test-Path (Join-Path $incRoot "wdf.h")) -and (Test-Path (Join-Path $KitsRoot "Lib\wdf\kmdf\x64\WdfDriverEntry.lib"))) {
        return "1.9"
    }

    throw "Could not determine KMDF version with matching include/lib files."
}

function Patch-KmdfVersionInInf {
    param(
        [Parameter(Mandatory=$true)][string]$InfPath,
        [Parameter(Mandatory=$true)][string]$Version
    )

    if (-not (Test-Path $InfPath)) {
        throw "INF not found: $InfPath"
    }

    $lines = @(Get-Content -Path $InfPath)
    $found = $false
    $out = foreach ($line in $lines) {
        if ($line -match '^\s*KmdfLibraryVersion\s*=') {
            $found = $true
            "KmdfLibraryVersion=$Version"
        } else {
            $line
        }
    }

    if (-not $found) {
        throw "Could not find KmdfLibraryVersion=... in $InfPath"
    }

    Set-Content -Path $InfPath -Value $out -Encoding ASCII
    Write-Host "Patched INF KMDF version: $Version -> $InfPath" -ForegroundColor Green
}

function Enable-TestSigningIfNeeded {
    $bcdText = (& bcdedit /enum 2>$null) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Could not query bcdedit. Continuing."
        return
    }

    if ($bcdText -match '(?im)^testsigning\s+Yes\s*$') {
        Write-Host "Windows test-signing mode is already enabled."
        return
    }

    Write-Host "Enabling Windows test-signing mode..." -ForegroundColor Yellow
    & bcdedit /set testsigning on
    if ($LASTEXITCODE -ne 0) {
        throw "bcdedit failed. Secure Boot may be enabled; disable Secure Boot for test-signed drivers."
    }
    Write-Warning "Test-signing was just enabled. Reboot before loading the driver."
}

function Find-WdkTool {
    param([Parameter(Mandatory=$true)][string]$ToolName)

    $cmd = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) {
        return $cmd.Source
    }

    $roots = @()
    if ($env:WindowsSdkDir) {
        $p = Join-Path $env:WindowsSdkDir "bin"
        if (Test-Path $p) { $roots += $p }
    }
    $p1 = "C:\Program Files (x86)\Windows Kits\10\bin"
    $p2 = "C:\Program Files\Windows Kits\10\bin"
    if (Test-Path $p1) { $roots += $p1 }
    if (Test-Path $p2) { $roots += $p2 }
    $roots = @($roots | Select-Object -Unique)

    if ($roots.Length -eq 0) {
        throw "No WDK bin search roots found while looking for $ToolName"
    }

    $hits = @()
    foreach ($root in $roots) {
        Write-Host "Searching for $ToolName under: $root"
        $hits += @(Get-ChildItem -Path $root -Recurse -Filter $ToolName -File -ErrorAction SilentlyContinue)
    }
    $hits = @($hits | Sort-Object FullName -Descending)

    if ($hits.Length -eq 0) {
        throw "Could not find WDK tool $ToolName under: $($roots -join '; ')"
    }

    $preferred = @($hits | Where-Object { $_.FullName -match "\\$Platform\\" })
    if ($preferred.Length -gt 0) { return $preferred[0].FullName }

    $x86 = @($hits | Where-Object { $_.FullName -match "\\x86\\" })
    if ($x86.Length -gt 0) { return $x86[0].FullName }

    return $hits[0].FullName
}

function Ensure-TestCertificate {
    param([string]$OutputDirectory)

    $subject = "CN=DartFrogTek Test Driver Cert"
    $cert = Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.Subject -eq $subject } | Sort-Object NotAfter -Descending | Select-Object -First 1
    if (-not $cert) {
        Write-Host "Creating local test code-signing certificate..." -ForegroundColor Yellow
        $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject -CertStoreLocation "Cert:\LocalMachine\My"
    } else {
        Write-Host "Using existing test certificate: $($cert.Thumbprint)"
    }

    $cerPath = Join-Path $OutputDirectory "DartFrogTekTest.cer"
    Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null
    Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
    Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
    return $cert
}

$kmdfVersion = Get-BuildCompatibleKmdfVersion
Write-Host "Selected build-compatible KMDF version: $kmdfVersion" -ForegroundColor Green
Patch-KmdfVersionInInf -InfPath $DriverInf -Version $kmdfVersion

Enable-TestSigningIfNeeded

if (-not $SkipBuild) {
    if (-not (Test-Path $BuildBat)) {
        throw "Build script not found: $BuildBat"
    }
    Write-Section "Build"
    Invoke-CmdChecked -CommandLine ('call "{0}" {1} {2}' -f $BuildBat, $Configuration, $Platform) -WorkingDirectory $ProjectRoot
} else {
    Write-Host "Skipping build."
}

if (-not (Test-Path $DistDir)) {
    throw "Dist directory not found after build: $DistDir"
}
if (-not (Test-Path $DistInf)) {
    Copy-Item -Force $DriverInf $DistInf
}
Patch-KmdfVersionInInf -InfPath $DistInf -Version $kmdfVersion

if (-not $SkipSign) {
    Write-Section "Catalog and signing"
    $inf2cat = Find-WdkTool -ToolName "Inf2Cat.exe"
    $signtool = Find-WdkTool -ToolName "signtool.exe"
    Write-Host "Inf2Cat:  $inf2cat" -ForegroundColor Green
    Write-Host "SignTool: $signtool" -ForegroundColor Green

    if (Test-Path $DistCat) {
        Remove-Item -Force $DistCat
    }

    Invoke-Checked -Exe $inf2cat -Args @('/driver:.', '/os:10_X64') -WorkingDirectory $DistDir

    if (-not (Test-Path $DistCat)) {
        throw "Inf2Cat did not produce expected catalog: $DistCat"
    }

    $null = Ensure-TestCertificate -OutputDirectory $DistDir
    Invoke-Checked -Exe $signtool -Args @('sign', '/v', '/fd', 'SHA256', '/s', 'My', '/n', 'DartFrogTek Test Driver Cert', '.\dftfdc.cat') -WorkingDirectory $DistDir
    Invoke-Checked -Exe $signtool -Args @('verify', '/v', '/pa', '.\dftfdc.cat') -WorkingDirectory $DistDir
} else {
    Write-Host "Skipping catalog/signing."
}

if (-not $SkipInstall) {
    Write-Section "Install"
    Invoke-Checked -Exe "pnputil.exe" -Args @('/add-driver', '.\dftfdc.inf', '/install') -WorkingDirectory $DistDir

    Write-Section "Status"
    & sc.exe query dftfdc
    & pnputil.exe /enum-devices /connected | findstr /i "Dart Frog DFTFDC 8888 dftfdc"
} else {
    Write-Host "Skipping install."
}

if ($RunVersionTest) {
    $ctl = Join-Path $DistDir "dftfdcctl.exe"
    if (Test-Path $ctl) {
        Write-Section "Version test"
        & $ctl version
    } else {
        Write-Warning "Cannot run version test; dftfdcctl.exe not found: $ctl"
    }
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "Staged output: $DistDir"
Write-Host "Try: `"$DistDir\dftfdcctl.exe`" version"
