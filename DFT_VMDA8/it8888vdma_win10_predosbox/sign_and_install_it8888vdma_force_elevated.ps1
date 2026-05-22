# sign_and_install_it8888vdma_force_elevated_v4.ps1
# Run from:
#   DFT_VMDA8\it8888vdma_win10_predosbox
#
# v4 fix:
#   v3 passed signtool:
#       /n IT8888VDMA Test Cert
#   through Start-Process -ArgumentList, and signtool received:
#       /n IT8888VDMA Test Cert
#   as split tokens, so it signed but also complained:
#       File not found: Test
#       File not found: Cert
#
# v4 signs by SHA1 thumbprint instead:
#       /sha1 <thumbprint>
#   so spaces in the cert subject cannot break argument parsing.
#
# Normal:
#   powershell -ExecutionPolicy Bypass -File .\sign_and_install_it8888vdma_force_elevated_v4.ps1 -RemoveOldPackages
#
# With testsigning:
#   powershell -ExecutionPolicy Bypass -File .\sign_and_install_it8888vdma_force_elevated_v4.ps1 -RemoveOldPackages -EnableTestSigning

param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$HardwareIdNeedle = "PCI\VEN_1283&DEV_8888",
    [string]$CertName = "IT8888VDMA Test Cert",
    [switch]$RemoveOldPackages,
    [switch]$NoDisableEnable,
    [switch]$NoRescan,
    [switch]$EnableTestSigning,
    [switch]$NoPauseAtEnd,
    [switch]$AlreadyElevated
)

$ErrorActionPreference = "Stop"

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Quote-Arg([string]$s) {
    if ($null -eq $s -or $s.Length -eq 0) { return '""' }
    if ($s -match '[\s"`$&|<>^]') { return '"' + ($s -replace '"','\"') + '"' }
    return $s
}

function Relaunch-Elevated {
    $scriptPath = $PSCommandPath
    if (-not $scriptPath) { $scriptPath = $MyInvocation.MyCommand.Path }
    if (-not $scriptPath) { throw "Could not determine script path." }

    $args = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass",
        "-File", $scriptPath,
        "-Configuration", $Configuration,
        "-Platform", $Platform,
        "-HardwareIdNeedle", $HardwareIdNeedle,
        "-CertName", $CertName,
        "-AlreadyElevated"
    )
    if ($RemoveOldPackages) { $args += "-RemoveOldPackages" }
    if ($NoDisableEnable)   { $args += "-NoDisableEnable" }
    if ($NoRescan)          { $args += "-NoRescan" }
    if ($EnableTestSigning) { $args += "-EnableTestSigning" }
    if ($NoPauseAtEnd)      { $args += "-NoPauseAtEnd" }

    $argLine = ($args | ForEach-Object { Quote-Arg "$_" }) -join " "
    Write-Host "Requesting Administrator elevation..." -ForegroundColor Yellow
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

    # Use a single quoted command line for Start-Process. This avoids it
    # splitting paths/arguments with spaces when invoking native tools.
    $argLine = ($Args | ForEach-Object { Quote-Arg "$_" }) -join " "

    $tmpBase = Join-Path $env:TEMP ("it8888_cmd_{0}_{1}" -f ([Guid]::NewGuid().ToString("N")), [IO.Path]::GetFileNameWithoutExtension($File))
    $outFile = "$tmpBase.out.txt"
    $errFile = "$tmpBase.err.txt"

    try {
        $p = Start-Process -FilePath $File `
            -ArgumentList $argLine `
            -NoNewWindow `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $outFile `
            -RedirectStandardError $errFile

        if (Test-Path $outFile) {
            $stdout = Get-Content $outFile -Raw
            if ($stdout) { Write-Host $stdout.TrimEnd() }
        }
        if (Test-Path $errFile) {
            $stderr = Get-Content $errFile -Raw
            if ($stderr) { Write-Host $stderr.TrimEnd() -ForegroundColor DarkYellow }
        }

        $code = [int]$p.ExitCode
        Write-Host "<<< exit $code" -ForegroundColor DarkGray
        return $code
    }
    finally {
        Remove-Item $outFile,$errFile -Force -ErrorAction SilentlyContinue
    }
}

function Find-KitTool {
    param([string]$Name)

    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "${env:ProgramFiles}\Windows Kits\10\bin"
    ) | Where-Object { $_ -and (Test-Path $_) }

    $candidates = @()
    foreach ($root in $roots) {
        $candidates += Get-ChildItem -Path $root -Recurse -Filter $Name -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending
    }

    $x64 = @($candidates | Where-Object { $_.FullName -match '\\x64\\' })
    if ($x64.Count -gt 0) { return $x64[0].FullName }

    $x86 = @($candidates | Where-Object { $_.FullName -match '\\x86\\' })
    if ($x86.Count -gt 0) { return $x86[0].FullName }

    if ($candidates.Count -gt 0) { return $candidates[0].FullName }

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    throw "Could not find $Name. Install Windows SDK/WDK components that include $Name."
}

function Ensure-TestCertificate {
    param([string]$SubjectName)

    $subject = "CN=$SubjectName"

    $cert = Get-ChildItem Cert:\CurrentUser\My -ErrorAction SilentlyContinue |
        Where-Object { $_.Subject -eq $subject -and $_.HasPrivateKey } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1

    if (-not $cert) {
        Write-Host "Creating self-signed test certificate: $SubjectName" -ForegroundColor Yellow
        $cert = New-SelfSignedCertificate `
            -Type CodeSigningCert `
            -Subject $subject `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -KeyAlgorithm RSA `
            -KeyLength 2048 `
            -HashAlgorithm SHA256 `
            -NotAfter (Get-Date).AddYears(10)
    } else {
        Write-Host "Using existing test certificate: $($cert.Subject) thumbprint=$($cert.Thumbprint)"
    }

    foreach ($storeName in @("Root", "TrustedPublisher")) {
        $store = New-Object System.Security.Cryptography.X509Certificates.X509Store($storeName, "LocalMachine")
        $store.Open("ReadWrite")
        try {
            $exists = $false
            foreach ($c in $store.Certificates) {
                if ($c.Thumbprint -eq $cert.Thumbprint) { $exists = $true; break }
            }
            if (-not $exists) {
                Write-Host "Adding cert to LocalMachine\$storeName"
                $store.Add($cert)
            }
        } finally {
            $store.Close()
        }
    }

    return $cert
}

function Get-InfCatalogFiles {
    param([string]$InfPath)
    $cats = @()
    foreach ($line in Get-Content $InfPath) {
        if ($line -match '^\s*CatalogFile(?:\.[^=]+)?\s*=\s*(.+?)\s*$') {
            $cat = $matches[1].Trim().Trim('"')
            if ($cat.Length -gt 0) { $cats += $cat }
        }
    }
    return @($cats | Select-Object -Unique)
}

function Get-It8888Devices {
    try {
        $all = Get-PnpDevice -PresentOnly:$false -ErrorAction Stop
        $hits = @()
        foreach ($d in $all) {
            $ids = @()
            try {
                $ids = (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName 'DEVPKEY_Device_HardwareIds' -ErrorAction Stop).Data
            } catch {}
            $idText = (($ids | ForEach-Object { "$_" }) -join "`n")
            if ($d.InstanceId -like "*VEN_1283*DEV_8888*" -or $idText -like "*VEN_1283*DEV_8888*") {
                $hits += $d
            }
        }
        return $hits
    } catch {
        Write-Warning "Get-PnpDevice failed: $($_.Exception.Message)"
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
        Write-Warning "No matching device found."
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

    Write-Host "============================================================"
    Write-Host "IT8888VDMA sign + force install / bind v4"
    Write-Host "Elevated:   $(Test-Admin)"
    Write-Host "Root:       $Root"
    Write-Host "Dist:       $Dist"
    Write-Host "INF:        $Inf"
    Write-Host "HWID:       $HardwareIdNeedle"
    Write-Host "RemoveOld:  $RemoveOldPackages"
    Write-Host "============================================================"

    foreach ($p in @($Inf, $Sys)) {
        if (-not (Test-Path $p)) { throw "Missing required staged file: $p" }
    }

    if ($EnableTestSigning) {
        Write-Host ""
        Write-Host "=== Enabling Windows TESTSIGNING ===" -ForegroundColor Yellow
        [int]$bc = Run-Cmd bcdedit "/set" "testsigning" "on"
        if ($bc -ne 0) { throw "bcdedit testsigning failed with exit code $bc" }
        Write-Warning "TESTSIGNING change requires reboot before it affects kernel driver loading."
    }

    Write-Host ""
    Write-Host "=== Generate catalog with inf2cat ===" -ForegroundColor Yellow
    $inf2cat = Find-KitTool "inf2cat.exe"
    Write-Host "inf2cat: $inf2cat"

    $infCatalogNames = Get-InfCatalogFiles $Inf
    if ($infCatalogNames.Count -eq 0) {
        Write-Warning "INF has no CatalogFile line. pnputil may reject this package."
        $infCatalogNames = @("it8888vdma.cat")
    } else {
        Write-Host "INF CatalogFile names:"
        foreach ($n in $infCatalogNames) { Write-Host "  $n" }
    }

    Get-ChildItem -Path $Dist -Filter "*.cat" -File -ErrorAction SilentlyContinue | Remove-Item -Force

    $inf2catArgsList = @(
        @("/driver:$Dist", "/os:10_X64", "/verbose"),
        @("/driver:$Dist", "/os:10_GE_X64", "/verbose"),
        @("/driver:$Dist", "/os:10_RS5_X64", "/verbose")
    )

    foreach ($args in $inf2catArgsList) {
        [int]$code = Run-Cmd $inf2cat @args
        $catsNow = @(Get-ChildItem -Path $Dist -Filter "*.cat" -File -ErrorAction SilentlyContinue)
        if ($code -eq 0 -and $catsNow.Count -gt 0) {
            Write-Host "CAT files present after inf2cat:"
            foreach ($c in $catsNow) { Write-Host "  $($c.Name) size=$($c.Length)" }
            break
        }
    }

    $cats = @(Get-ChildItem -Path $Dist -Filter "*.cat" -File -ErrorAction SilentlyContinue)
    if ($cats.Count -eq 0) {
        Write-Host ""
        Write-Host "Directory listing for debugging:" -ForegroundColor Yellow
        Get-ChildItem -Path $Dist | Format-Table Name, Length, LastWriteTime
        throw "inf2cat produced no *.cat file in $Dist"
    }

    foreach ($catName in $infCatalogNames) {
        $catPath = Join-Path $Dist $catName
        if (-not (Test-Path $catPath)) {
            if ($cats.Count -eq 1) {
                Write-Warning "INF expects $catName, but inf2cat generated $($cats[0].Name). Copying generated CAT to expected name."
                Copy-Item $cats[0].FullName $catPath -Force
            } else {
                throw "INF expects $catName but it is missing, and multiple CATs exist."
            }
        }
    }

    $catsToSign = @()
    foreach ($catName in $infCatalogNames) {
        $catsToSign += (Join-Path $Dist $catName)
    }
    $catsToSign = @($catsToSign | Select-Object -Unique)

    Write-Host ""
    Write-Host "=== Create/trust test cert and sign catalog(s) ===" -ForegroundColor Yellow
    $cert = Ensure-TestCertificate $CertName
    $thumb = ($cert.Thumbprint -replace '\s+', '')
    Write-Host "Signing by thumbprint: $thumb"

    $signtool = Find-KitTool "signtool.exe"
    Write-Host "signtool: $signtool"

    foreach ($catPath in $catsToSign) {
        if (-not (Test-Path $catPath)) {
            throw "CAT to sign missing: $catPath"
        }

        [int]$signCode = Run-Cmd $signtool "sign" "/v" "/fd" "SHA256" "/s" "My" "/sha1" "$thumb" "$catPath"
        if ($signCode -ne 0) { throw "signtool sign failed for $catPath with exit code $signCode" }

        [int]$verifyCode = Run-Cmd $signtool "verify" "/v" "/pa" "$catPath"
        if ($verifyCode -ne 0) { Write-Warning "signtool verify /pa returned $verifyCode for $catPath" }
    }

    Show-DeviceDriverState

    if (-not $NoDisableEnable) {
        Write-Host ""
        Write-Host "=== Disabling matching IT8888 device(s) before driver bind ===" -ForegroundColor Yellow
        foreach ($d in Get-It8888Devices) {
            Write-Host "Disabling: $($d.InstanceId)"
            [int]$dc = Run-Cmd pnputil "/disable-device" "$($d.InstanceId)"
            Write-Host "disable exit=$dc"
        }
    }

    if ($RemoveOldPackages) {
        Write-Host ""
        Write-Host "=== Removing old it8888vdma.inf driver-store packages ===" -ForegroundColor Yellow
        $pkgs = Get-DriverPackagesByOriginalName "it8888vdma.inf"
        if ($pkgs.Count -eq 0) { Write-Host "No old it8888vdma.inf packages found." }
        foreach ($p in $pkgs) {
            Write-Host ("Removing {0} Provider={1} Version={2}" -f $p.PublishedName, $p.ProviderName, $p.DriverVersion)
            [int]$delCode = Run-Cmd pnputil "/delete-driver" "$($p.PublishedName)" "/uninstall" "/force"
            Write-Host "delete-driver exit=$delCode"
        }
    }

    Write-Host ""
    Write-Host "=== Adding signed driver package and requesting install ===" -ForegroundColor Yellow
    [int]$addCode = Run-Cmd pnputil "/add-driver" "$Inf" "/install"
    if ($addCode -ne 0) { throw "pnputil /add-driver /install failed with exit code $addCode" }

    if (-not $NoRescan) {
        Write-Host ""
        Write-Host "=== Device rescan ===" -ForegroundColor Yellow
        [int]$scanCode = Run-Cmd pnputil "/scan-devices"
        Write-Host "scan-devices exit=$scanCode"
        Start-Sleep -Seconds 2
    }

    if (-not $NoDisableEnable) {
        Write-Host ""
        Write-Host "=== Enabling/restarting matching IT8888 device(s) ===" -ForegroundColor Yellow
        foreach ($d in Get-It8888Devices) {
            Write-Host "Enabling: $($d.InstanceId)"
            [int]$enCode = Run-Cmd pnputil "/enable-device" "$($d.InstanceId)"
            Write-Host "enable exit=$enCode"
            Start-Sleep -Milliseconds 500

            Write-Host "Restarting: $($d.InstanceId)"
            [int]$rsCode = Run-Cmd pnputil "/restart-device" "$($d.InstanceId)"
            Write-Host "restart exit=$rsCode"
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
    Write-Host "If install succeeds but the device later shows Code 52, rerun with -EnableTestSigning and reboot."
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
