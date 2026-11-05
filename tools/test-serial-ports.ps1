$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'serial-ports.ps1')
$ids = @('VID_1A86&PID_7523', 'VID_1A86&PID_5523')
function Device($port, $hardware, $instance, $name) {
    [pscustomobject]@{Port=$port; HardwareId=$hardware; InstanceId=$instance; Name=$name}
}
function Assert($condition, $message) { if (-not $condition) { throw $message } }
function MustFail([scriptblock]$action, [string]$expected) {
    $message = ''
    try { & $action | Out-Null } catch { $message = $_.Exception.Message }
    Assert ($message -like "*$expected*") "Expected error containing '$expected'; got '$message'"
}
$bluetooth = Device COM5 'BTHENUM\serial' 'BTHENUM\test' 'Bluetooth'
$board = Device COM7 'USB\VID_1A86&PID_7523&REV_0264;USB\VID_1A86&PID_7523' 'USB\VID_1A86&PID_7523\1' 'USB-SERIAL CH340'
$other = Device COM12 'USB\VID_1A86&PID_7523' 'USB\VID_1A86&PID_7523\2' 'Second CH340'
$unknown = Device COM8 'USB\VID_1234&PID_5678' 'USB\VID_1234&PID_5678\3' 'CH340-looking name'
$nearId = Device COM9 'USB\VID_1A86&PID_75230' 'USB\VID_1A86&PID_75230\4' 'Wrong PID'
Assert ((Resolve-Pj26Port -Devices @($bluetooth,$board) -UsbHardwareIds $ids) -eq 'COM7') 'Bluetooth selected instead of board'
Assert ((Resolve-Pj26Port -Devices @($bluetooth,$other) -UsbHardwareIds $ids) -eq 'COM12') 'Changed COM number not detected'
MustFail { Resolve-Pj26Port -Devices @($bluetooth,$unknown,$nearId) -UsbHardwareIds $ids } 'No compatible'
MustFail { Resolve-Pj26Port -Devices @($board,$other) -UsbHardwareIds $ids } 'Multiple compatible'
Assert ((Resolve-Pj26Port -Devices @($board,$other) -UsbHardwareIds $ids -OverridePort COM12) -eq 'COM12') 'Explicit choice failed'
MustFail { Resolve-Pj26Port -Devices @($bluetooth,$board) -UsbHardwareIds $ids -OverridePort COM5 } 'Bluetooth'
MustFail { Resolve-Pj26Port -Devices @($board) -UsbHardwareIds $ids -PortSetting COM6 } 'not present'
Write-Host 'PASS: USB hardware IDs; Bluetooth excluded; changed COM number; zero/multiple devices; explicit selection'
