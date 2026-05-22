# install_it8888vdma_force_elevated.ps1
# Run from:
#   DFT_VMDA8\it8888vdma_win10_predosbox
#
# Self-elevating force install/bind script for IT8888VDMA.
#
# Normal use from non-admin PowerShell:
#   powershell -ExecutionPolicy Bypass -File .\install_it8888vdma_force_elevated.ps1
#
# Stronger cleanup:
#   powershell -ExecutionPolicy Bypass -File .\install_it8888vdma_force_elevated.ps1 -RemoveOldPackages
#
# This script does not rebuild. Rebuild separately if needed:
#   .\build_all_direct.bat Debug x64

param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$HardwareIdNeedle = "PCI\VEN_1283&DEV_8888",
    [switch]$RemoveOldPackages,
    [switch]$NoDisableEnable,
    [switch]$NoRescan,
    [switch]$NoPauseAtEnd,
    [switch]$AlreadyElevated
)

$ErrorActionPreference = "Stop"

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Quote-Arg {
    param([string]$s)
    if ($null -eq $s -or $s.Length -eq 0) { return '""' }
    if ($s -match '[\s"`$&|<>^]') {
        return '"' + ($s -replace '"','\"') + '"'
    }
    return $s
}

function Relaunch-Elevated {
    $scriptPath = $PSCommandPath
    if (-not $scriptPath) {
        $scriptPath = $MyInvocation.MyCommand.Path
    }
    if (-not $scriptPath) {
        throw "Could not determine script path for elevation relaunch."
    }

    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $scriptPath,
        "-Configuration", $Configuration,
        "-Platform", $Platform,
        "-HardwareIdNeedle", $HardwareIdNeedle,
        "-AlreadyElevated"
    )

    if ($RemoveOldPackages) { $args += "-RemoveOldPackages" }
    if ($NoDisableEnable)   { $args += "-NoDisableEnable" }
    if ($NoRescan)          { $args += "-NoRescan" }
    if ($NoPauseAtEnd)      { $args += "-NoPauseAtEnd" }

    $argLine = ($args | ForEach-Object { Quote-Arg "$_" }) -join " "

    Write-Host "Requesting Administrator elevation..." -ForegroundColor Yellow
    Write-Host "powershell.exe $argLine" -ForegroundColor DarkGray

    Start-Process -FilePath "powershell.exe" -ArgumentList $argLine -Verb RunAs -WorkingDirectory (Split-Path -Parent $scriptPath)
    exit 0
}

if (-not (Test-Admin)) {
    Relaunch-Elevated
}

function Run-Cmd {
    param(
        [Parameter(Mandatory=$true)][string]$File,
        [Parameter(ValueFromRemainingArguments=$true)][string[]]$Args
    )

    Write-Host ""
    Write-Host ">>> $File $($Args -join ' ')" -ForegroundColor Cyan
    & $File @Args
    $code = $LASTEXITCODE
    Write-Host "<<< exit $code" -ForegroundColor DarkGray
    return $code
}

function Get-It8888Devices {
    try {
        $all = Get-PnpDevice -PresentOnly:$false -ErrorAction Stop
        $hits = @()

        foreach ($d in $all) {
            $ids = @()
            try {
                $ids = (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName 'DEVPKEY_Device_HardwareIds' -ErrorAction Stop).Data
            } catch {
                $ids = @()
            }

            $idText = (($ids | ForEach-Object { "$_" }) -join "`n")
            if ($d.InstanceId -like "*VEN_1283*DEV_8888*" -or $idText -like "*VEN_1283*DEV_8888*") {
                $hits += $d
            }
        }

        return $hits
    } catch {
        Write-Warning "Get-PnpDevice unavailable/failed: $($_.Exception.Message)"
        return @()
    }
}

function Get-DriverPackagesByOriginalName {
    param([string]$OriginalName)

    $out = & pnputil /enum-drivers 2>&1
    $packages = @()
    $cur = @{}

    foreach ($line in $out) {
        if ($line -match '^\s*Published Name\s*:\s*(.+?)\s*$') {
            if ($cur.Count -gt 0) { $packages += [pscustomobject]$cur }
            $cur = @{ PublishedName = $matches[1].Trim() }
        } elseif ($line -match '^\s*Original Name\s*:\s*(.+?)\s*$') {
            $cur.OriginalName = $matches[1].Trim()
        } elseif ($line -match '^\s*Provider Name\s*:\s*(.+?)\s*$') {
            $cur.ProviderName = $matches[1].Trim()
        } elseif ($line -match '^\s*Driver Version\s*:\s*(.+?)\s*$') {
            $cur.DriverVersion = $matches[1].Trim()
        }
    }

    if ($cur.Count -gt 0) { $packages += [pscustomobject]$cur }

    return @($packages | Where-Object { $_.OriginalName -ieq $OriginalName })
}

function Show-DeviceDriverState {
    Write-Host ""
    Write-Host "=== PnP devices matching $HardwareIdNeedle ===" -ForegroundColor Yellow
    $devs = Get-It8888Devices

    if ($devs.Count -eq 0) {
        Write-Warning "No present/non-present PnP device found matching VEN_1283 DEV_8888."
        return
    }

    foreach ($d in $devs) {
        Write-Host ("Status={0} Class={1} Name={2}" -f $d.Status, $d.Class, $d.FriendlyName)
        Write-Host ("  InstanceId={0}" -f $d.InstanceId)

        try {
            $svc = (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName 'DEVPKEY_Device_Service' -ErrorAction Stop).Data
            Write-Host ("  Service={0}" -f $svc)
        } catch {}

        try {
            $drv = Get-CimInstance Win32_PnPSignedDriver |
                Where-Object { $_.DeviceID -eq $d.InstanceId } |
                Select-Object -First 1

            if ($drv) {
                Write-Host ("  DriverProvider={0}" -f $drv.DriverProviderName)
                Write-Host ("  DriverVersion={0}" -f $drv.DriverVersion)
                Write-Host ("  InfName={0}" -f $drv.InfName)
            }
        } catch {}
    }
}

try {
    $Root = Split-Path -Parent $MyInvocation.MyCommand.Path
    if (-not $Root) { $Root = (Get-Location).Path }

    Set-Location $Root

    $Dist = Join-Path $Root ("dist\{0}_{1}" -f $Configuration, $Platform)
    $Inf = Join-Path $Dist "it8888vdma.inf"
    $Sys = Join-Path $Dist "it8888vdma.sys"
    $Cat = Join-Path $Dist "it8888vdma.cat"

    Write-Host "============================================================"
    Write-Host "IT8888VDMA force install / bind"
    Write-Host "Elevated:   $(Test-Admin)"
    Write-Host "Root:       $Root"
    Write-Host "Dist:       $Dist"
    Write-Host "INF:        $Inf"
    Write-Host "HWID:       $HardwareIdNeedle"
    Write-Host "RemoveOld:  $RemoveOldPackages"
    Write-Host "============================================================"

    foreach ($p in @($Inf, $Sys, $Cat)) {
        if (-not (Test-Path $p)) {
            throw "Missing required staged file: $p"
        }
    }

    Show-DeviceDriverState

    if (-not $NoDisableEnable) {
        Write-Host ""
        Write-Host "=== Disabling matching IT8888 device(s) before driver bind ===" -ForegroundColor Yellow
        foreach ($d in Get-It8888Devices) {
            Write-Host "Disabling: $($d.InstanceId)"
            Run-Cmd pnputil "/disable-device" "$($d.InstanceId)" | Out-Null
        }
    }

    if ($RemoveOldPackages) {
        Write-Host ""
        Write-Host "=== Removing old it8888vdma.inf driver-store packages ===" -ForegroundColor Yellow
        $pkgs = Get-DriverPackagesByOriginalName "it8888vdma.inf"

        if ($pkgs.Count -eq 0) {
            Write-Host "No old it8888vdma.inf packages found."
        }

        foreach ($p in $pkgs) {
            Write-Host ("Removing {0} Provider={1} Version={2}" -f $p.PublishedName, $p.ProviderName, $p.DriverVersion)
            Run-Cmd pnputil "/delete-driver" "$($p.PublishedName)" "/uninstall" "/force" | Out-Null
        }
    } else {
        Write-Host ""
        Write-Host "=== Keeping existing driver-store packages; use -RemoveOldPackages to purge old it8888vdma.inf copies ===" -ForegroundColor Yellow
    }

    Write-Host ""
    Write-Host "=== Adding driver package and requesting install on matching devices ===" -ForegroundColor Yellow
    $code = Run-Cmd pnputil "/add-driver" "$Inf" "/install"
    if ($code -ne 0) {
        throw "pnputil /add-driver /install failed with exit code $code"
    }

    if (-not $NoRescan) {
        Write-Host ""
        Write-Host "=== Device rescan ===" -ForegroundColor Yellow
        Run-Cmd pnputil "/scan-devices" | Out-Null
        Start-Sleep -Seconds 2
    }

    if (-not $NoDisableEnable) {
        Write-Host ""
        Write-Host "=== Enabling/restarting matching IT8888 device(s) ===" -ForegroundColor Yellow
        foreach ($d in Get-It8888Devices) {
            Write-Host "Enabling: $($d.InstanceId)"
            Run-Cmd pnputil "/enable-device" "$($d.InstanceId)" | Out-Null
            Start-Sleep -Milliseconds 500

            Write-Host "Restarting: $($d.InstanceId)"
            Run-Cmd pnputil "/restart-device" "$($d.InstanceId)" | Out-Null
            Start-Sleep -Milliseconds 500
        }
    }

    Show-DeviceDriverState

    Write-Host ""
    Write-Host "=== Post-install sanity ===" -ForegroundColor Yellow
    $exe = Join-Path $Dist "it8888ctl.exe"
    if (Test-Path $exe) {
        Write-Host "Trying: $exe info"
        & $exe info
    } else {
        Write-Warning "No it8888ctl.exe at $exe"
    }

    Write-Host ""
    Write-Host "Done."
    Write-Host "If Device Service/InfName still shows a Microsoft/Windows driver, rerun with:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File .\install_it8888vdma_force_elevated.ps1 -RemoveOldPackages"
}
catch {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor DarkRed
    if (-not $NoPauseAtEnd) {
        Write-Host ""
        Read-Host "Press Enter to close"
    }
    exit 1
}

if (-not $NoPauseAtEnd -and $AlreadyElevated) {
    Write-Host ""
    Read-Host "Press Enter to close"
}
