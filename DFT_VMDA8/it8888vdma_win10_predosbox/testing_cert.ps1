cd D:\it8888vdma_win10_predosbox\dist\Debug_x64

# 1. Create cert in CurrentUser\My if it does not already exist
$cert = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq "CN=DartFrogTek Test Driver Cert" } |
    Select-Object -First 1

if (-not $cert) {
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject "CN=DartFrogTek Test Driver Cert" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -KeyExportPolicy Exportable
}

# 2. Export public cert
Export-Certificate `
    -Cert $cert `
    -FilePath .\DartFrogTekTestDriverCert.cer

# 3. Trust it system-wide
Import-Certificate `
    -FilePath .\DartFrogTekTestDriverCert.cer `
    -CertStoreLocation Cert:\LocalMachine\Root

Import-Certificate `
    -FilePath .\DartFrogTekTestDriverCert.cer `
    -CertStoreLocation Cert:\LocalMachine\TrustedPublisher

# 4. Regenerate catalog
& "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\Inf2Cat.exe" /driver:. /os:10_X64

# 5. Sign cat and sys using CurrentUser\My cert store
& "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\signtool.exe" sign /v /fd SHA256 /s My /n "DartFrogTek Test Driver Cert" .\it8888vdma.cat

& "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\signtool.exe" sign /v /fd SHA256 /s My /n "DartFrogTek Test Driver Cert" .\it8888vdma.sys

& "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\signtool.exe" verify /v .\it8888vdma.cat

& "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\signtool.exe" verify /v .\it8888vdma.sys