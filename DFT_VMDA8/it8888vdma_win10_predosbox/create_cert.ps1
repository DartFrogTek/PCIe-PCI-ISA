$cert = New-SelfSignedCertificate `
  -Type CodeSigningCert `
  -Subject "CN=DartFrogTek Test Driver Cert" `
  -CertStoreLocation "Cert:\LocalMachine\My"

Export-Certificate `
  -Cert $cert `
  -FilePath .\DartFrogTekTestDriverCert.cer

Import-Certificate `
  -FilePath .\DartFrogTekTestDriverCert.cer `
  -CertStoreLocation Cert:\LocalMachine\Root

Import-Certificate `
  -FilePath .\DartFrogTekTestDriverCert.cer `
  -CertStoreLocation Cert:\LocalMachine\TrustedPublisher