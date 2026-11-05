param(
    [string]$Firmware,
    # Legacy EIDE auto-selected port hint. Never overrides project settings.
    [string]$Port,
    [string]$OverridePort,
    [int]$Baud,
    [switch]$ProbeOnly,
    [switch]$ListPorts,
    [switch]$DryRun,
    [int]$WaitResetSeconds = -1,
    [switch]$InvertBoot
)
$ErrorActionPreference = 'Stop'
try {
    $projectRoot = Split-Path -Parent $PSScriptRoot
    $settings = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'flash-settings.json') -Raw | ConvertFrom-Json
    if (-not ('Pj26Flash.SerialDiscovery' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'Stm32Serial.cs') }
    . (Join-Path $PSScriptRoot 'serial-ports.ps1')
    $devices = @([Pj26Flash.SerialDiscovery]::Devices())
    if ($ListPorts) {
        $devices | Select-Object Port, Name, HardwareId | Format-Table -AutoSize
        exit 0
    }
    $portHint = $Port
    $Port = Resolve-Pj26Port -Devices $devices -UsbHardwareIds $settings.usbHardwareIds -PortSetting $settings.port -OverridePort $OverridePort
    if ($portHint -and $portHint -ne $Port) {
        Write-Host "Ignoring EIDE port hint $portHint; using selected port $Port."
    }
    if ($Baud -eq 0) { $Baud = [int]$settings.baud }
    if ($WaitResetSeconds -lt 0) { $WaitResetSeconds = [int]$settings.waitResetSeconds }
    if ($Port -notmatch '^COM[1-9][0-9]*$') { throw 'Set an explicit COM port in tools/flash-settings.json.' }
    $Port = $Port.ToUpperInvariant()
    if ($Baud -lt 1200 -or $Baud -gt 460800) { throw 'Invalid UART baud rate.' }
    if ($WaitResetSeconds -lt 0 -or $WaitResetSeconds -gt 120) { throw 'Invalid reset timeout.' }
    $hexFile = ''
    if (-not $ProbeOnly) {
        if (-not $Firmware) { throw 'Specify the built HEX with -Firmware.' }
        $hexFile = (Resolve-Path -LiteralPath $Firmware).Path
        $image = [Pj26Flash.Image]::Load($hexFile)
        Write-Host "HEX: $hexFile ($($image.Length) bytes)"
    }
    if ($DryRun) { Write-Host "DRY RUN OK. Port=$Port, baud=$Baud. No device access."; exit 0 }
    $bootRts = [bool]$settings.bootRts
    if ($InvertBoot) { $bootRts = -not $bootRts }
    [Pj26Flash.Programmer]::Run($hexFile, $Port, $Baud, $bootRts,
        [bool]$settings.resetDtr, $WaitResetSeconds, [bool]$ProbeOnly,
        (Join-Path $projectRoot 'build/backups'))
    exit 0
} catch {
    [Console]::Error.WriteLine('FLASH FAILED: ' + $_.Exception.GetBaseException().Message)
    exit 1
}
