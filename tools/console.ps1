function Start-Pj26Console {
    param([string]$ProjectRoot, [string]$Mode = 'ЗАПИСЬ FLASH', [switch]$Preview)
    $logPath = $null
    if (-not $Preview) {
        $logDir = Join-Path $ProjectRoot 'build/flash-logs'
        [IO.Directory]::CreateDirectory($logDir) | Out-Null
        $logPath = Join-Path $logDir ('flash-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $PID + '.log')
    }
    $script:Pj26Ui = @{
        Clock = [Diagnostics.Stopwatch]::StartNew(); Log = $logPath
        Active = $false; Width = 0; Port = ''; Bytes = 0
        Labels = @{connect='Подключение'; backup='Резервная копия'; erase='Стирание'; write='Запись'; verify='Проверка'; run='Запуск'}
        Steps = @{connect=1; backup=2; erase=3; write=4; verify=5; run=6}
    }
    Write-Host ''
    Write-Host '  LoRa NET / PJ26' -ForegroundColor Cyan
    Write-Host ('  ' + $Mode) -ForegroundColor DarkGray
    Write-Host ('  ' + ('-' * 62)) -ForegroundColor DarkGray
    Write-Pj26Log 'session' $Mode
}

function Write-Pj26Log {
    param([string]$Kind, [string]$Text)
    if ($script:Pj26Ui -and $script:Pj26Ui.Log) {
        $line = (Get-Date -Format 'HH:mm:ss.fff') + ' [' + $Kind + '] ' + $Text + [Environment]::NewLine
        [IO.File]::AppendAllText($script:Pj26Ui.Log, $line, (New-Object Text.UTF8Encoding $false))
    }
    Write-Verbose ($Kind + ': ' + $Text)
}

function Clear-Pj26Progress {
    if ($script:Pj26Ui -and $script:Pj26Ui.Active) {
        Write-Host ("`r" + (' ' * $script:Pj26Ui.Width) + "`r") -NoNewline
        $script:Pj26Ui.Active = $false
        $script:Pj26Ui.Width = 0
    }
}

function Write-Pj26ProgressLine {
    param([string]$Text, [ConsoleColor]$Color = [ConsoleColor]::Cyan)
    $width = [Math]::Max($script:Pj26Ui.Width, $Text.Length)
    Write-Host ("`r" + $Text.PadRight($width)) -NoNewline -ForegroundColor $Color
    $script:Pj26Ui.Active = $true
    $script:Pj26Ui.Width = $width
}

function Format-Pj26Size {
    param([long]$Bytes)
    return ('{0:0.0} КиБ' -f ($Bytes / 1024.0))
}

function Write-Pj26Metadata {
    param([string]$File, [long]$Bytes, [string]$Port, [string]$Device, [int]$Baud)
    $script:Pj26Ui.Port = $Port
    $script:Pj26Ui.Bytes = $Bytes
    if ($File) {
        Write-Host ('  Файл     ' + [IO.Path]::GetFileName($File))
        Write-Host ('  Образ    ' + (Format-Pj26Size $Bytes) + ' / 64 КиБ') -ForegroundColor DarkGray
        Write-Pj26Log 'firmware' $File
    }
    $deviceLabel = $Device -replace '\s*\(COM\d+\)\s*$', ''
    Write-Host ('  Порт     ' + $Port + ' | ' + $deviceLabel + ' | ' + $Baud + ' бод')
    Write-Host ''
    Write-Pj26Log 'port' ($Port + ' / ' + $Device + ' / ' + $Baud + ' / 8E1')
}

function Write-Pj26Event {
    param([string]$Kind, [string]$Text)
    Write-Pj26Log $Kind $Text
    $parts = $Kind.Split('.')
    $stage = if ($parts.Length -gt 1) { $parts[1] } else { '' }
    if ($Kind -eq 'wait.reset' -or $Kind -eq 'wait.tick') {
        Write-Pj26ProgressLine ('  [RST] Нажми и отпусти RST на плате. Осталось ' + $Text + ' с') Yellow
        return
    }
    if ($Kind -eq 'done.probe') { return }
    $label = $script:Pj26Ui.Labels[$stage]
    $number = $script:Pj26Ui.Steps[$stage]
    if (-not $label) { return }
    $prefix = '  [{0}/6] {1,-18}' -f $number, $label
    switch ($parts[0]) {
        'stage' { Write-Pj26ProgressLine ($prefix + '...') }
        'progress' {
            $values = $Text.Split('/')
            $done = [long]$values[0]; $total = [long]$values[1]
            $percent = [int][Math]::Floor(100.0 * $done / $total)
            $filled = [int][Math]::Floor($percent / 5.0)
            $bar = '[' + ('#' * $filled) + ('-' * (20 - $filled)) + ']'
            Write-Pj26ProgressLine ($prefix + $bar + (' {0,3}%  ' -f $percent) + (Format-Pj26Size $done))
        }
        'done' {
            Clear-Pj26Progress
            $detail = switch ($stage) {
                'connect' { $Text }
                'backup' { '64 КиБ сохранено' }
                'erase' { $Text + ' страниц' }
                'write' { Format-Pj26Size ([long]$Text) }
                'verify' { 'совпадение всех байтов' }
                'run' { $Text }
            }
            Write-Host ($prefix + 'OK  ' + $detail) -ForegroundColor Green
        }
    }
}

function Stop-Pj26Console {
    param([switch]$DryRun, [switch]$ProbeOnly, [switch]$Preview, [switch]$ListPorts)
    Clear-Pj26Progress
    $elapsed = '{0:0.0}' -f $script:Pj26Ui.Clock.Elapsed.TotalSeconds
    $message = if ($Preview) { 'ПРИМЕР ВЫВОДА — плата не открывалась' }
        elseif ($DryRun) { 'HEX ПРОВЕРЕН — запись не выполнялась' }
        elseif ($ProbeOnly) { 'СВЯЗЬ ПРОВЕРЕНА — память не изменялась' }
        elseif ($ListPorts) { 'Поиск портов завершён' }
        else { 'ГОТОВО — прошивка проверена и запущена' }
    Write-Host ''
    Write-Host ('  ' + $message) -ForegroundColor Green
    Write-Host ('  Время: ' + $elapsed + ' с') -ForegroundColor DarkGray
    if ($script:Pj26Ui.Log) {
        Write-Host ('  Журнал: build/flash-logs/' + [IO.Path]::GetFileName($script:Pj26Ui.Log)) -ForegroundColor DarkGray
    }
    Write-Host ''
    Write-Pj26Log 'success' $message
}

function Stop-Pj26ConsoleError {
    param([string]$Message, [string]$Details)
    Clear-Pj26Progress
    Write-Pj26Log 'error' $Details
    $friendly = if ($Message -like '*Cannot open COM*') { 'COM-порт занят или недоступен. Закрой HTerm/MCUISP и повтори Flash.' }
        elseif ($Message -like '*No compatible USB-serial*') { 'Плата не найдена. Проверь USB-кабель и подключение CH340/CH341.' }
        elseif ($Message -like '*Multiple compatible USB-serial*') { 'Найдено несколько плат. Отключи лишнюю или укажи -OverridePort COMx.' }
        elseif ($Message -like '*No bootloader ACK*') { 'Загрузчик не ответил. Повтори Flash и нажми RST по приглашению.' }
        else { $Message }
    Write-Host ''
    Write-Host '  ОШИБКА — операция не завершена' -ForegroundColor Red
    Write-Host ('  ' + $friendly) -ForegroundColor Yellow
    if ($Message -ne $friendly) { Write-Host ('  Детали: ' + $Message) -ForegroundColor DarkGray }
    if ($script:Pj26Ui -and $script:Pj26Ui.Log) {
        Write-Host ('  Журнал: ' + $script:Pj26Ui.Log) -ForegroundColor DarkGray
    }
    Write-Host ''
}

function Show-Pj26ConsolePreview {
    Write-Pj26Metadata -File 'dx_lr30_pj26.hex' -Bytes 15872 -Port COM7 -Device 'USB-SERIAL CH340' -Baud 230400
    Write-Pj26Event 'stage.connect' 'COM7 / 230400 / 8E1'
    Write-Pj26Event 'wait.reset' '25'
    Write-Pj26Event 'done.connect' 'PID 0x0410 | Bootloader 2.2 | Flash 64 KiB'
    foreach ($stage in @('backup','erase','write','verify','run')) {
        $value = switch ($stage) { 'backup' {'example-backup.bin'} 'erase' {'16'} 'run' {'0x08000000'} default {'15872'} }
        Write-Pj26Event ('stage.' + $stage) $value
        if ($stage -in @('backup','write','verify')) {
            $total = if ($stage -eq 'backup') { 65536 } else { 15872 }
            foreach ($percent in @(0,50,100)) { Write-Pj26Event ('progress.' + $stage) (([int]($total * $percent / 100)).ToString() + '/' + $total) }
        }
        Write-Pj26Event ('done.' + $stage) $value
    }
    Stop-Pj26Console -Preview
}
