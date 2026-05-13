# install_rebuild_it8888vdma.ps1
# Build, stage, and reinstall the IT8888VDMA driver package.
#
# Usage from project root:
#   powershell -ExecutionPolicy Bypass -File .\install_rebuild_it8888vdma.ps1
#   powershell -ExecutionPolicy Bypass -File .\install_rebuild_it8888vdma.ps1 -Configuration Release
#
# This script must run elevated. If not elevated, it relaunches itself as Administrator.

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [switch]$SkipBuild,

    [switch]$SkipUninstallOld,

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
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-Configuration", $Configuration,
        "-Platform", $Platform
    )

    if ($SkipBuild) { $argsList += "-SkipBuild" }
    if ($SkipUninstallOld) { $argsList += "-SkipUninstallOld" }
    if ($NoPause) { $argsList += "-NoPause" }

    Write-Host "[elevate] Relaunching as Administrator..."
    Start-Process powershell.exe -Verb RunAs -ArgumentList $argsList
    exit
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

Write-Host "============================================================"
Write-Host "IT8888VDMA rebuild + reinstall"
Write-Host "Root:          $Root"
Write-Host "Configuration: $Configuration"
Write-Host "Platform:      $Platform"
Write-Host "Dist:          $Dist"
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

if (-not (Test-Path $InfPath)) {
    throw "Missing INF after staging: $InfPath"
}

Write-Host "`n[dist] Current staged files:"
Get-ChildItem $Dist | Sort-Object Name | Format-Table Mode, Length, LastWriteTime, Name -AutoSize

if (-not $SkipUninstallOld) {
    Write-Host "`n[cleanup] Looking for old IT8888VDMA driver-store packages..."
    $enum = pnputil /enum-drivers
    $lines = $enum -split "`r?`n"

    $oldOems = New-Object System.Collections.Generic.List[string]
    $currentPublished = $null
    $currentProvider = $null
    $currentClass = $null
    $currentOriginal = $null

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
            $currentClass = $null
            $currentOriginal = $null
        } elseif ($line -match "Original Name\s*:\s*(.*)$") {
            $currentOriginal = $Matches[1].Trim()
        } elseif ($line -match "Provider Name\s*:\s*(.*)$") {
            $currentProvider = $Matches[1].Trim()
        } elseif ($line -match "Class Name\s*:\s*(.*)$") {
            $currentClass = $Matches[1].Trim()
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
    } else {
        foreach ($oem in ($oldOems | Select-Object -Unique)) {
            Write-Host "[cleanup] Removing old package $oem ..."
            pnputil /delete-driver $oem /uninstall /force
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "pnputil /delete-driver $oem failed with exit code $LASTEXITCODE; continuing."
            }
        }
    }
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
