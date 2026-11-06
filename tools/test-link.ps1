[CmdletBinding()]
param(
    [string]$PortA,
    [string]$PortB,
    [ValidateRange(1,10000)][int]$Count = 20,
    [ValidateRange(24,255)][int]$Size = 64,
    [ValidateRange(1000,60000)][int]$TimeoutMs = 4000,
    [ValidateRange(100,60000)][int]$GapMs = 200,
    [switch]$Preview
)
$ErrorActionPreference = 'Stop'
$a = $null; $b = $null; $session = $null; $failure = $null
$projectRoot = Split-Path -Parent $PSScriptRoot
$reportBase = $null

function Save-LinkReport {
    param([string]$Base, [object]$Report)
    [Pj26Link.ReportFile]::Save(($Base + '.json'), ($Report | ConvertTo-Json -Depth 8))
}
try {
    Write-Host ''
    Write-Host '  LoRa NET / PJ26 — ТЕСТ РАДИОКАНАЛА' -ForegroundColor Cyan
    if ($Preview) { Write-Host '  ПРЕДПРОСМОТР: симуляция, COM-порты не открываются.' -ForegroundColor Yellow }
    else { Write-Host '  Закрой HTerm/MCUISP. На обеих платах — Radio. Во время теста не нажимай KEY.' -ForegroundColor DarkGray }
    if (-not ('Pj26Link.Session' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'RadioLinkTest.cs') }
    $options = New-Object Pj26Link.Options
    $options.Count = $Count; $options.Size = $Size; $options.TimeoutMs = $TimeoutMs; $options.GapMs = $GapMs
    $options.Validate()
    if ($Preview) {
        $PortA = 'SIM-A'; $PortB = 'SIM-B'
        $a = New-Object Pj26Link.MemoryEndpoint; $b = New-Object Pj26Link.MemoryEndpoint
        $a.Peer = $b; $b.Peer = $a
    } else {
        . (Join-Path $PSScriptRoot 'serial-ports.ps1')
        $settings = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'flash-settings.json') -Raw | ConvertFrom-Json
        if (-not ('Pj26Flash.SerialDiscovery' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'Stm32Serial.cs') }
        $devices = @([Pj26Flash.SerialDiscovery]::Devices())
        $pair = @(Resolve-Pj26PortPair -Devices $devices -UsbHardwareIds $settings.usbHardwareIds -PortA $PortA -PortB $PortB)
        $PortA = $pair[0]; $PortB = $pair[1]
        Write-Host ('  A = {0}   B = {1}   9600 8N1' -f $PortA, $PortB)
        # Application levels: release reset, select Flash boot. No ISP/reset pulses.
        $appDtr = -not [bool]$settings.resetDtr; $appRts = -not [bool]$settings.bootRts
        $a = [Pj26Link.SerialEndpoint]::new($PortA, 9600, $appDtr, $appRts)
        $b = [Pj26Link.SerialEndpoint]::new($PortB, 9600, $appDtr, $appRts)
    }
    $session = [Pj26Link.Session]::new($a, $b, $options, $PortA, $PortB)
    if ($Preview) { $session.Report.Mode = 'preview' }
    $directory = Join-Path $projectRoot 'build/link-tests'
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $prefix = if ($Preview) { 'preview' } else { 'link' }
    $reportBase = Join-Path $directory ($prefix + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $PID)
    Write-Host ('  {0} пакетов в каждую сторону, по {1} байт; тайм-аут {2} мс; пауза {3} мс' -f $Count, $Size, $TimeoutMs, $GapMs)
    Write-Host '  Задержка: ПК -> USB/UART -> радио -> UART/USB -> ПК.' -ForegroundColor DarkGray
    Write-Host '  Ctrl+C — остановить. Тест не прошивает платы.' -ForegroundColor DarkGray
    Write-Host ''
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $virtualMs = 0L
    $eventsWritten = 0
    $checkpointEvery = [Math]::Max(1, [int][Math]::Ceiling(2.0 * $Count / 20))
    do {
        $now = if ($Preview) { $virtualMs } else { $clock.ElapsedMilliseconds }
        $running = $session.Step($now)
        foreach ($line in $session.DrainEvents()) {
            $color = if ($line -like '*TIMEOUT*') { 'Yellow' } else { 'DarkGray' }
            Write-Host ('  ' + $line.Replace('TIMEOUT', 'нет ответа')) -ForegroundColor $color
            # Keep at most about 20 checkpoints, including large packet counts.
            $eventsWritten++
            if ($eventsWritten % $checkpointEvery -eq 0) { Save-LinkReport -Base $reportBase -Report $session.Report }
        }
        if ($Preview) { $virtualMs += 50L }
        elseif ($running) { Start-Sleep -Milliseconds 10 }
    } while ($running)
} catch {
    $failure = $_.Exception.GetBaseException().Message
    if ($session) { $session.Report.Error = $failure }
} finally {
    foreach ($endpoint in @($a,$b)) {
        if ($endpoint -is [IDisposable]) {
            try { $endpoint.Dispose() } catch {
                if (-not $failure) { $failure = $_.Exception.GetBaseException().Message }
                if ($session) { $session.Report.Error = $failure }
            }
        }
    }
    if ($session -and $reportBase) {
        if (-not $session.Report.Completed -and -not $session.Report.Error) { $session.Report.Error = 'Interrupted before completion.' }
        $session.Report.FinishedUtc = [DateTime]::UtcNow.ToString('o')
        Save-LinkReport -Base $reportBase -Report $session.Report
        $session.Report.Packets | Select-Object Direction,Sequence,Status,LatencyMs | Export-Csv -LiteralPath ($reportBase + '.csv') -NoTypeInformation -Encoding UTF8
        Write-Host ''
        foreach ($result in $session.Report.Directions) {
            Write-Host ('  {0}: отправлено {1}, получено {2}, потери {3} ({4:0.0}%), опоздали {5}, дубли {6}, эхо {7}, искажения {8}' -f
                $result.Direction,$result.Sent,$result.Received,$result.Lost,$result.LossPercent,$result.Late,$result.Duplicates,$result.Echoes,$result.Mismatches)
            if ($result.Received) { Write-Host ('          задержка мин/сред/макс: {0}/{1:0.0}/{2} мс' -f $result.MinMs,$result.AverageMs,$result.MaxMs) -ForegroundColor DarkGray }
        }
        Write-Host ('  Ошибки тестовых кадров A/B: {0}/{1}; посторонние кадры: {2}' -f $session.Report.BadTestFrames[0],$session.Report.BadTestFrames[1],$session.Report.ForeignFrames)
        Write-Host ('  Отчёты: ' + $reportBase + '.json / .csv') -ForegroundColor Cyan
    }
}
if ($failure) { Write-Host ('  ОШИБКА: ' + $failure) -ForegroundColor Red; exit 1 }
if ($session -and $session.Report.Passed) {
    $label = if ($Preview) { 'ПРЕДПРОСМОТР ЗАВЕРШЁН (это результат симуляции)' } else { 'УСПЕХ: все тестовые пакеты получены без искажений и в срок.' }
    Write-Host ('  ' + $label) -ForegroundColor Green; exit 0
}
Write-Host '  ЕСТЬ ПОТЕРИ/ОШИБКИ либо тест прерван. Подробности — в JSON/CSV.' -ForegroundColor Yellow
exit 2
