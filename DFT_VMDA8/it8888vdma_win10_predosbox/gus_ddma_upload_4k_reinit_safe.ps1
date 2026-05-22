# gus_ddma_upload_4k_reinit_safe_v3.ps1
#
# Safe external 4KiB DDMA WAV uploader/player for IT8888 + PicoGUS/GUS.
#
# v3:
#   - Adds -PlayWholeRegion, so playback can be one continuous GF1 region.
#   - Keeps -PlayEachChunk as a diagnostic mode only.
#
# Proven strategy:
#   - upload one 4096-byte DDMA chunk
#   - re-init bridge / IT8888 windows
#   - upload the next 4096-byte chunk
#   - re-init before playback
#   - play whole loaded DRAM region once
#
# Example:
#   powershell -ExecutionPolicy Bypass -File .\gus_ddma_upload_4k_reinit_safe_v3.ps1 `
#     -Exe .\dist\Debug_x64\it8888ctl.exe `
#     -Base 0x8240 `
#     -Wav .\test.wav `
#     -StartDram 0x000100 `
#     -TotalSamples 32768 `
#     -ChunkSamples 4096 `
#     -Gain 400 `
#     -Freq 0x03c0 `
#     -PlayWholeRegion `
#     -PlayMsWhole 3000

param(
    [string]$Exe = ".\dist\Debug_x64\it8888ctl.exe",
    [string]$Base = "0x8240",
    [string]$Wav = ".\test.wav",

    # Keep low target for now. Avoid 0x010000 until repeated DDMA is stable.
    [string]$StartDram = "0x000100",

    [int]$TotalSamples = 32768,
    [int]$ChunkSamples = 4096,

    [int]$Gain = 400,
    [string]$Freq = "0x03c0",
    [string]$DmaCtrl = "0x01",
    [int]$SettleMs = 1500,

    # Whole-region audition. This is the useful/public demo mode.
    [switch]$PlayWholeRegion = $true,
    [int]$PlayMsWhole = 3000,

    # Diagnostic mode. Plays each 4KiB chunk separately with tiny breaks.
    [switch]$PlayEachChunk = $false,
    [int]$PlayMsPerChunk = 140,

    [switch]$DumpChunks = $true
)

$ErrorActionPreference = "Stop"

function Invoke-It8888 {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string[]]$CmdArgs
    )

    Write-Host ""
    Write-Host "=== $Name ==="
    Write-Host "$Exe $($CmdArgs -join ' ')"

    & $Exe @CmdArgs
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "$Name failed with exit code $code"
    }
}

function Sleep-Ms {
    param([int]$Ms)
    if ($Ms -gt 0) {
        Start-Sleep -Milliseconds $Ms
    }
}

function To-U32 {
    param([string]$Value)
    if ($Value.StartsWith("0x") -or $Value.StartsWith("0X")) {
        return [Convert]::ToUInt32($Value.Substring(2), 16)
    }
    return [Convert]::ToUInt32($Value, 10)
}

function Hex6 {
    param([uint32]$Value)
    return ("0x{0:x6}" -f $Value)
}

function Init-Bridge {
    param([string]$Label = "INIT/REAPPLY BRIDGE")

    Write-Host ""
    Write-Host "============================================================"
    Write-Host $Label
    Write-Host "============================================================"

    Invoke-It8888 -Name "$Label : init" -CmdArgs @("init")
    Sleep-Ms 500

    Invoke-It8888 -Name "$Label : root bridge iowin" -CmdArgs @("bridge-iowin", "0", "28", "3", "0x8000", "0x8fff")
    Sleep-Ms 500

    Invoke-It8888 -Name "$Label : it8888 bridge iowin" -CmdArgs @("bridge-iowin", "3", "0", "0", "0x8000", "0x8fff")
    Sleep-Ms 500

    Invoke-It8888 -Name "$Label : cfgwrite 5C" -CmdArgs @("cfgwrite", "0x5C", "4", "0xe3008340")
    Sleep-Ms 500

    Invoke-It8888 -Name "$Label : cfgwrite 60" -CmdArgs @("cfgwrite", "0x60", "4", "0xe2008240")
    Sleep-Ms 500
}

if (-not (Test-Path $Exe)) {
    throw "Missing executable: $Exe"
}
if (-not (Test-Path $Wav)) {
    throw "Missing WAV: $Wav"
}
if ($ChunkSamples -le 0 -or $ChunkSamples -gt 4096) {
    throw "ChunkSamples must be 1..4096. Use 4096 for the proven path."
}
if ($TotalSamples -le 0) {
    throw "TotalSamples must be positive."
}

$start = [uint32](To-U32 $StartDram)
$chunks = [int][Math]::Ceiling($TotalSamples / [double]$ChunkSamples)

Write-Host "============================================================"
Write-Host "GUS DDMA 4KiB reinit-safe uploader/player v3"
Write-Host "Exe:             $Exe"
Write-Host "Base:            $Base"
Write-Host "WAV:             $Wav"
Write-Host "StartDram:       $(Hex6 $start)"
Write-Host "TotalSamples:    $TotalSamples"
Write-Host "ChunkSamples:    $ChunkSamples"
Write-Host "Chunks:          $chunks"
Write-Host "Gain:            $Gain"
Write-Host "Freq:            $Freq"
Write-Host "DmaCtrl:         $DmaCtrl"
Write-Host "SettleMs:        $SettleMs"
Write-Host "PlayWholeRegion: $PlayWholeRegion"
Write-Host "PlayMsWhole:     $PlayMsWhole"
Write-Host "PlayEachChunk:   $PlayEachChunk"
Write-Host "PlayMs/chunk:    $PlayMsPerChunk"
Write-Host "============================================================"

# Initial bridge setup and low DMA buffer.
Init-Bridge "INITIAL BRIDGE SETUP"

Invoke-It8888 -Name "DMA FREE" -CmdArgs @("dma-free")
Sleep-Ms 500

Invoke-It8888 -Name "DMA ALLOC LOW" -CmdArgs @("dma-alloc-low", "65536")
Sleep-Ms 500

# Upload chunks. Re-init before every chunk after chunk 0.
for ($i = 0; $i -lt $chunks; $i++) {
    $src = $i * $ChunkSamples
    $remaining = $TotalSamples - $src
    $count = [Math]::Min($ChunkSamples, $remaining)
    $dram = [uint32]($start + $src)

    if ($i -gt 0) {
        Init-Bridge "RE-INIT BEFORE CHUNK $i"
    }

    Invoke-It8888 -Name "DDMA CLEAR BEFORE CHUNK $i" -CmdArgs @("ddma-clear")
    Sleep-Ms 500

    Invoke-It8888 -Name "UPLOAD CHUNK $i src=$src dram=$(Hex6 $dram) count=$count" -CmdArgs @(
        "gus-ddma-wav-load-low-safe",
        $Base,
        $Wav,
        (Hex6 $dram),
        "$src",
        "$count",
        "$Gain",
        $Freq,
        $DmaCtrl,
        "$SettleMs",
        "0"
    )

    Sleep-Ms 1000
}

# Verify starts of chunks using CPU GUS DRAM dump.
if ($DumpChunks) {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host "DUMP CHUNK STARTS"
    Write-Host "============================================================"

    for ($i = 0; $i -lt $chunks; $i++) {
        $src = $i * $ChunkSamples
        $dram = [uint32]($start + $src)
        Invoke-It8888 -Name "DUMP CHUNK $i dram=$(Hex6 $dram)" -CmdArgs @("gus-dram-dump", $Base, (Hex6 $dram), "64")
        Sleep-Ms 500
    }
}

# Re-init once before playback. This should not erase GUS DRAM.
if ($PlayWholeRegion -or $PlayEachChunk) {
    Init-Bridge "RE-INIT BEFORE PLAYBACK"
}

if ($PlayWholeRegion) {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host "PLAY WHOLE LOADED REGION"
    Write-Host "============================================================"

    Invoke-It8888 -Name "PLAY WHOLE REGION dram=$(Hex6 $start) bytes=$TotalSamples" -CmdArgs @(
        "gus-play-loaded-region-safe",
        $Base,
        (Hex6 $start),
        "$TotalSamples",
        $Freq,
        "$PlayMsWhole"
    )

    Sleep-Ms 500
}

if ($PlayEachChunk) {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host "PLAY EACH LOADED CHUNK IN SEQUENCE"
    Write-Host "============================================================"

    for ($i = 0; $i -lt $chunks; $i++) {
        $src = $i * $ChunkSamples
        $remaining = $TotalSamples - $src
        $count = [Math]::Min($ChunkSamples, $remaining)
        $dram = [uint32]($start + $src)

        Invoke-It8888 -Name "PLAY CHUNK $i dram=$(Hex6 $dram) bytes=$count" -CmdArgs @(
            "gus-play-loaded-region-safe",
            $Base,
            (Hex6 $dram),
            "$count",
            $Freq,
            "$PlayMsPerChunk"
        )

        Sleep-Ms 250
    }
}

Write-Host ""
Write-Host "============================================================"
Write-Host "DONE"
Write-Host "============================================================"
