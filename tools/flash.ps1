[CmdletBinding()]
param(
    [string]$Firmware,
    # Legacy EIDE auto-selected port hint. Never overrides project settings.
    [string]$Port,
    [string]$OverridePort,
    [int]$Baud,
    [switch]$ProbeOnly,
    [switch]$ListPorts,
    [switch]$DryRun,
    [switch]$Preview,
    [int]$WaitResetSeconds = -1,
    [switch]$InvertBoot
)
$ErrorActionPreference = 'Stop'
$uiReady = $false
try {
    $projectRoot = Split-Path -Parent $PSScriptRoot
    . (Join-Path $PSScriptRoot 'console.ps1')
    $mode = if ($Preview) { 'ПРЕДПРОСМОТР' } elseif ($DryRun) { 'ПРОВЕРКА HEX' } elseif ($ProbeOnly) { 'ПРОВЕРКА СВЯЗИ' } elseif ($ListPorts) { 'ПОИСК ПЛАТЫ' } else { 'ЗАПИСЬ FLASH' }
    Start-Pj26Console -ProjectRoot $projectRoot -Mode $mode -Preview:$Preview
    $uiReady = $true
    if ($Preview) { Show-Pj26ConsolePreview; exit 0 }
    $settings = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'flash-settings.json') -Raw | ConvertFrom-Json
    if (-not ('Pj26Flash.SerialDiscovery' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'Stm32Serial.cs') }
    . (Join-Path $PSScriptRoot 'serial-ports.ps1')
    $devices = @([Pj26Flash.SerialDiscovery]::Devices())
    Write-Pj26Log 'ports' (($devices | Select-Object Port,Name,HardwareId | ConvertTo-Json -Compress) -join '')
    if ($ListPorts) {
        $devices | Select-Object Port, Name | Format-Table -AutoSize | Out-Host
        Stop-Pj26Console -ListPorts
        exit 0
    }
    $portHint = $Port
    $Port = Resolve-Pj26Port -Devices $devices -UsbHardwareIds $settings.usbHardwareIds -PortSetting $settings.port -OverridePort $OverridePort
    if ($portHint -and $portHint -ne $Port) {
        Write-Pj26Log 'port.ignored-hint' ("Ignoring EIDE port hint $portHint; using selected port $Port.")
    }
    if ($Baud -eq 0) { $Baud = [int]$settings.baud }
    if ($WaitResetSeconds -lt 0) { $WaitResetSeconds = [int]$settings.waitResetSeconds }
    if ($Port -notmatch '^COM[1-9][0-9]*$') { throw 'Set an explicit COM port in tools/flash-settings.json.' }
    $Port = $Port.ToUpperInvariant()
    if ($Baud -lt 1200 -or $Baud -gt 460800) { throw 'Invalid UART baud rate.' }
    if ($WaitResetSeconds -lt 0 -or $WaitResetSeconds -gt 120) { throw 'Invalid reset timeout.' }
    $hexFile = ''
    $bytes = 0
    if (-not $ProbeOnly) {
        if (-not $Firmware) { throw 'Specify the built HEX with -Firmware.' }
        $hexFile = (Resolve-Path -LiteralPath $Firmware).Path
        $image = [Pj26Flash.Image]::Load($hexFile)
        $bytes = $image.Length
    }
    $device = @($devices | Where-Object { $_.Port -eq $Port })[0]
    Write-Pj26Metadata -File $hexFile -Bytes $bytes -Port $Port -Device $device.Name -Baud $Baud
    if ($DryRun) { Stop-Pj26Console -DryRun; exit 0 }
    $bootRts = [bool]$settings.bootRts
    if ($InvertBoot) { $bootRts = -not $bootRts }
    $report = [Action[string,string]] { param($kind, $text) Write-Pj26Event -Kind $kind -Text $text }
    [Pj26Flash.Programmer]::Run($hexFile, $Port, $Baud, $bootRts,
        [bool]$settings.resetDtr, $WaitResetSeconds, [bool]$ProbeOnly,
        (Join-Path $projectRoot 'build/backups'), $report)
    Stop-Pj26Console -ProbeOnly:$ProbeOnly
    exit 0
} catch {
    if ($uiReady) { Stop-Pj26ConsoleError -Message $_.Exception.GetBaseException().Message -Details $_.Exception.ToString() }
    else { [Console]::Error.WriteLine('FLASH FAILED: ' + $_.Exception.GetBaseException().Message) }
    exit 1
}
