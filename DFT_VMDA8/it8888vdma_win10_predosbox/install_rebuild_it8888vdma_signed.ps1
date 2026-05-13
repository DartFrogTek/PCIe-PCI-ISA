# install_rebuild_it8888vdma_signed.ps1
# Clean full replacement.
#
# Build, stage, test-sign, and reinstall the IT8888VDMA driver package.
#
# Usage from project root:
#   powershell -ExecutionPolicy Bypass -File .\install_rebuild_it8888vdma_signed.ps1 -Configuration Debug -Platform x64
#   .\install_rebuild_it8888vdma_signed.bat Debug x64
#
# Requires elevated PowerShell. If not elevated, this script relaunches itself with -NoExit.
# Requires test mode for easy local driver loading:
#   bcdedit /set testsigning on
#   reboot

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [string]$CertName = "IT8888VDMA Test Cert",

    [switch]$SkipBuild,

    [switch]$SkipUninstallOld,

    [switch]$SkipSigning,

    [switch]$NoPause
)

$ErrorActionPreference = "Stop"

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Relaunch-Elevated {
    $argsList = @(
        "-NoExit",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-Configuration", $Configuration,
        "-Platform", $Platform,
        "-CertName", "`"$CertName`""
    )

    if ($SkipBuild) { $argsList += "-SkipBuild" }
    if ($SkipUninstallOld) { $argsList += "-SkipUninstallOld" }
    if ($SkipSigning) { $argsList += "-SkipSigning" }
    if ($NoPause) { $argsList += "-NoPause" }

    Write-Host "[elevate] Relaunching as Administrator..."
    Start-Process powershell.exe -Verb RunAs -ArgumentList $argsList
    exit
}

function Find-Tool {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name,

        [string[]]$Roots = @(
            "C:\Program Files (x86)\Windows Kits\10\bin",
            "C:\Program Files\Windows Kits\10\bin",
            "C:\Program Files (x86)\Windows Kits\8.1\bin",
            "C:\Program Files\Windows Kits\8.1\bin"
        )
    )

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = New-Object System.Collections.Generic.List[object]

    foreach ($root in $Roots) {
        if (-not (Test-Path $root)) {
            continue
        }

        Get-ChildItem $root -Recurse -Filter $Name -File -ErrorAction SilentlyContinue | ForEach-Object {
            $full = $_.FullName

            $versionScore = [version]"0.0.0.0"
            if ($full -match "\\bin\\([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)\\") {
                try { $versionScore = [version]$Matches[1] } catch {}
            }

            $archScore = 0
            if ($full -match "\\x64\\") {
                $archScore = 3
            } elseif ($full -match "\\amd64\\") {
                $archScore = 3
            } elseif ($full -match "\\x86\\") {
                $archScore = 2
            } elseif ($full -match "\\arm64\\") {
                $archScore = 1
            }

            # inf2cat.exe is commonly only present in x86, and that is fine for x64 packages.
            if ($Name -ieq "inf2cat.exe" -and $full -match "\\x86\\") {
                $archScore = 4
            }

            $candidates.Add([pscustomobject]@{
                Path = $full
                Version = $versionScore
                ArchScore = $archScore
            })
        }
    }

    $best = $candidates |
        Sort-Object @{Expression="Version";Descending=$true}, @{Expression="ArchScore";Descending=$true}, @{Expression="Path";Descending=$true} |
        Select-Object -First 1

    if ($best) {
        return $best.Path
    }

    return $null
}

function Ensure-TestCert {
    param([string]$SubjectName)

    $subject = "CN=$SubjectName"
    $cert = Get-ChildItem Cert:\LocalMachine\My |
        Where-Object { $_.Subject -eq $subject } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1

    if (-not $cert) {
        Write-Host "[sign] Creating LocalMachine test certificate: $SubjectName"
        $cert = New-SelfSignedCertificate `
            -Type CodeSigningCert `
            -Subject $subject `
            -CertStoreLocation "Cert:\LocalMachine\My" `
            -KeyExportPolicy Exportable `
            -KeyLength 2048 `
            -HashAlgorithm SHA256 `
            -NotAfter (Get-Date).AddYears(10)
    } else {
        Write-Host "[sign] Reusing cert: $($cert.Subject) thumbprint=$($cert.Thumbprint)"
    }

    foreach ($storeName in @("TrustedPublisher", "Root")) {
        $existing = Get-ChildItem "Cert:\LocalMachine\$storeName" |
            Where-Object { $_.Thumbprint -eq $cert.Thumbprint } |
            Select-Object -First 1

        if (-not $existing) {
            Write-Host "[sign] Adding cert to LocalMachine\$storeName"
            $store = New-Object System.Security.Cryptography.X509Certificates.X509Store($storeName, "LocalMachine")
            $store.Open("ReadWrite")
            $store.Add($cert)
            $store.Close()
        }
    }

    return $cert
}

function Remove-OldIt8888Packages {
    Write-Host "`n[cleanup] Looking for old IT8888VDMA driver-store packages..."

    $enum = pnputil /enum-drivers
    $lines = $enum -split "`r?`n"

    $oldOems = New-Object System.Collections.Generic.List[string]
    $currentPublished = $null
    $currentProvider = $null
    $currentOriginal = $null

    function MaybeAddCurrent {
        if ($script:currentPublished -and (
            ($script:currentOriginal -ieq "it8888vdma.inf") -or
            ($script:currentProvider -match "DartFrog|IT8888|DartFrogTek") -or
            ($script:currentOriginal -match "it8888")
        )) {
            $script:oldOems.Add($script:currentPublished)
        }
    }

    foreach ($line in $lines) {
        if ($line -match "Published Name\s*:\s*(oem\d+\.inf)") {
            if ($currentPublished -and (
                ($currentOriginal -ieq "it8888vdma.inf") -or
                ($currentProvider -match "DartFrog|IT8888|DartFrogTek") -or
                ($currentOriginal -match "it8888")
            )) {
                $oldOems.Add($currentPublished)
            }

            $currentPublished = $Matches[1]
            $currentProvider = $null
            $currentOriginal = $null
        } elseif ($line -match "Original Name\s*:\s*(.*)$") {
            $currentOriginal = $Matches[1].Trim()
        } elseif ($line -match "Provider Name\s*:\s*(.*)$") {
            $currentProvider = $Matches[1].Trim()
        }
    }

    if ($currentPublished -and (
        ($currentOriginal -ieq "it8888vdma.inf") -or
        ($currentProvider -match "DartFrog|IT8888|DartFrogTek") -or
        ($currentOriginal -match "it8888")
    )) {
        $oldOems.Add($currentPublished)
    }

    if ($oldOems.Count -eq 0) {
        Write-Host "[cleanup] No old matching oem*.inf packages found."
        return
    }

    foreach ($oem in ($oldOems | Select-Object -Unique)) {
        Write-Host "[cleanup] Removing old package $oem ..."
        pnputil /delete-driver $oem /uninstall /force
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "pnputil /delete-driver $oem failed with exit code $LASTEXITCODE; continuing."
        }
    }
}

if (-not (Test-IsAdmin)) {
    Relaunch-Elevated
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$Dist = Join-Path $Root "dist\$($Configuration)_$($Platform)"
$DriverOut = Join-Path $Root "driver\$Platform\$Configuration"
$ToolOut = Join-Path $Root "tools\it8888ctl\$Platform\$Configuration"

$InfPath = Join-Path $Dist "it8888vdma.inf"
$SysPath = Join-Path $Dist "it8888vdma.sys"
$CatPath = Join-Path $Dist "it8888vdma.cat"

Write-Host "============================================================"
Write-Host "IT8888VDMA rebuild + sign + reinstall"
Write-Host "Root:          $Root"
Write-Host "Configuration: $Configuration"
Write-Host "Platform:      $Platform"
Write-Host "Dist:          $Dist"
Write-Host "Cert:          $CertName"
Write-Host "============================================================"

if (-not $SkipBuild) {
    Write-Host "`n[build] Running direct build..."
    $BuildScript = Join-Path $Root "build_all_direct.bat"
    if (-not (Test-Path $BuildScript)) {
        throw "Missing $BuildScript"
    }

    & cmd.exe /c "`"$BuildScript`" $Configuration $Platform"
    if ($LASTEXITCODE -ne 0) {
        throw "build_all_direct.bat failed with exit code $LASTEXITCODE"
    }
} else {
    Write-Host "`n[build] Skipped by -SkipBuild"
}

Write-Host "`n[stage] Ensuring dist folder exists..."
New-Item -ItemType Directory -Force -Path $Dist | Out-Null

$CopyList = @(
    @{ Src = Join-Path $DriverOut "it8888vdma.sys"; Dst = Join-Path $Dist "it8888vdma.sys"; Required = $true  },
    @{ Src = Join-Path $DriverOut "it8888vdma.pdb"; Dst = Join-Path $Dist "it8888vdma.pdb"; Required = $false },
    @{ Src = Join-Path $Root "driver\it8888vdma.inf"; Dst = Join-Path $Dist "it8888vdma.inf"; Required = $true  },
    @{ Src = Join-Path $Root "driver\public.h"; Dst = Join-Path $Dist "public.h"; Required = $false },
    @{ Src = Join-Path $ToolOut "it8888ctl.exe"; Dst = Join-Path $Dist "it8888ctl.exe"; Required = $true  },
    @{ Src = Join-Path $ToolOut "it8888ctl.pdb"; Dst = Join-Path $Dist "it8888ctl.pdb"; Required = $false }
)

foreach ($item in $CopyList) {
    if (Test-Path $item.Src) {
        Copy-Item -Force $item.Src $item.Dst
        Write-Host "[stage] copied $($item.Src) -> $($item.Dst)"
    } elseif ($item.Required) {
        throw "Required staged source missing: $($item.Src)"
    } else {
        Write-Host "[stage] optional missing: $($item.Src)"
    }
}

if (-not (Test-Path $InfPath)) { throw "Missing INF after staging: $InfPath" }
if (-not (Test-Path $SysPath)) { throw "Missing SYS after staging: $SysPath" }

if (-not $SkipSigning) {
    Write-Host "`n[sign] Locating WDK signing tools..."
    $Inf2Cat = Find-Tool "inf2cat.exe"
    $SignTool = Find-Tool "signtool.exe"

    if (-not $Inf2Cat) { throw "Could not find inf2cat.exe. Install WDK tools." }
    if (-not $SignTool) { throw "Could not find signtool.exe. Install WDK tools." }

    Write-Host "[sign] inf2cat: $Inf2Cat"
    Write-Host "[sign] signtool: $SignTool"

    if (Test-Path $CatPath) {
        Remove-Item -Force $CatPath
    }

    Write-Host "`n[sign] Generating catalog..."
    & $Inf2Cat /driver:"$Dist" /os:10_X64
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "inf2cat /os:10_X64 failed; retrying /os:10_GE_X64"
        & $Inf2Cat /driver:"$Dist" /os:10_GE_X64
        if ($LASTEXITCODE -ne 0) {
            throw "inf2cat failed with exit code $LASTEXITCODE"
        }
    }

    if (-not (Test-Path $CatPath)) {
        $generatedCat = Get-ChildItem $Dist -Filter *.cat | Select-Object -First 1
        if ($generatedCat) {
            $CatPath = $generatedCat.FullName
        } else {
            throw "inf2cat succeeded but no .cat file exists in $Dist"
        }
    }

    $cert = Ensure-TestCert -SubjectName $CertName

    Write-Host "`n[sign] Signing SYS..."
    & $SignTool sign /v /fd SHA256 /s My /sm /n "$CertName" "$SysPath"
    if ($LASTEXITCODE -ne 0) {
        throw "signtool sys signing failed with exit code $LASTEXITCODE"
    }

    Write-Host "`n[sign] Signing CAT..."
    & $SignTool sign /v /fd SHA256 /s My /sm /n "$CertName" "$CatPath"
    if ($LASTEXITCODE -ne 0) {
        throw "signtool cat signing failed with exit code $LASTEXITCODE"
    }

    Write-Host "`n[sign] Verifying CAT..."
    & $SignTool verify /v /pa "$CatPath"
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "signtool verify failed. Test mode may still load it; check output above."
    }
} else {
    Write-Host "`n[sign] Skipped by -SkipSigning"
}

Write-Host "`n[dist] Current staged files:"
Get-ChildItem $Dist | Sort-Object Name | Format-Table Mode, Length, LastWriteTime, Name -AutoSize

if (-not $SkipUninstallOld) {
    Remove-OldIt8888Packages
} else {
    Write-Host "`n[cleanup] Skipped by -SkipUninstallOld"
}

Write-Host "`n[install] Installing:"
Write-Host "  pnputil /add-driver `"$InfPath`" /install"
pnputil /add-driver "$InfPath" /install
if ($LASTEXITCODE -ne 0) {
    throw "pnputil install failed with exit code $LASTEXITCODE"
}

Write-Host "`n[status] Matching IT8888 devices:"
pnputil /enum-devices /connected | Select-String -Pattern "VEN_1283&DEV_8888|IT8888|VDMA|ISA bridge" -Context 0,4

Write-Host "`n[service] Service query:"
sc.exe query it8888vdma

Write-Host "`n[smoke] If service/device started, try:"
Write-Host "  `"$Dist\it8888ctl.exe`" info"
Write-Host "  `"$Dist\it8888ctl.exe`" dumpcfg"

Write-Host "`nDONE."

if (-not $NoPause) {
    Write-Host ""
    pause
}
