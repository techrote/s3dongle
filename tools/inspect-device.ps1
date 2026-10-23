$ErrorActionPreference = 'Stop'
# PnP evidence distinguishes USB recognition, driver binding, and a usable radio.
$devices = Get-PnpDevice -PresentOnly | Where-Object {
    $_.InstanceId -match '^USB\\VID_303A&PID_4080' -or $_.Class -eq 'Bluetooth'
}
$result = foreach ($device in $devices) {
    $props = Get-PnpDeviceProperty -InstanceId $device.InstanceId
    [pscustomobject]@{
        Name = $device.FriendlyName
        InstanceId = $device.InstanceId
        Status = $device.Status
        Class = $device.Class
        Service = ($props | Where-Object KeyName -eq 'DEVPKEY_Device_Service').Data
        ProblemCode = ($props | Where-Object KeyName -eq 'DEVPKEY_Device_ProblemCode').Data
        ProblemStatus = ($props | Where-Object KeyName -eq 'DEVPKEY_Device_ProblemStatus').Data
        HardwareIds = ($props | Where-Object KeyName -eq 'DEVPKEY_Device_HardwareIds').Data
        CompatibleIds = ($props | Where-Object KeyName -eq 'DEVPKEY_Device_CompatibleIds').Data
    }
}
ConvertTo-Json -InputObject @($result) -Depth 5
