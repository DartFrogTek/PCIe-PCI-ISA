cd D:\it8888vdma_win10_predosbox\dist\Debug_x64

$inf = ".\it8888vdma.inf"
$text = Get-Content $inf -Raw

if ($text -notmatch "\[SourceDisksNames\]") {
    $text += "`r`n[SourceDisksNames]`r`n1 = %DiskName%,,,`r`n"
}

if ($text -notmatch "\[SourceDisksFiles\]") {
    $text += "`r`n[SourceDisksFiles]`r`nit8888vdma.sys = 1`r`n"
}

if ($text -notmatch "DiskName\s*=") {
    $text = $text -replace "(\[Strings\]\r?\n)", "`$1DiskName = `"IT8888VDMA Install Disk`"`r`n"
}

Set-Content -Path $inf -Value $text -Encoding ASCII