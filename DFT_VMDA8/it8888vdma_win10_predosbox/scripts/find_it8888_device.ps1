Write-Host 'Present IT8888-like PCI devices:'
Get-PnpDevice -PresentOnly | Where-Object {
    $_.InstanceId -like 'PCI\VEN_1283&DEV_8888*' -or $_.InstanceId -like 'PCI\VEN_1283*' -or $_.FriendlyName -like '*IT8888*'
} | Format-Table -AutoSize Status,Class,FriendlyName,InstanceId

Write-Host "`nRaw PCI VEN_1283 matches in registry:"
Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Enum\PCI' -ErrorAction SilentlyContinue |
    Where-Object { $_.PSChildName -like 'VEN_1283*' } |
    Select-Object -ExpandProperty PSChildName
