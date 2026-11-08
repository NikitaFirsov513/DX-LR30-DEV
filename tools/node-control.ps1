function Invoke-Pj26Node {
    param([object]$Endpoint, [string]$Command = 'INFO', [int]$Timeout = 4000)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $request = [Pj26Link.NodeRequest]::new($Endpoint,$Command,$Timeout)
    while ($request.Step($watch.ElapsedMilliseconds)) { Start-Sleep -Milliseconds 10 }
    return $request.Response
}

function Test-Pj26ForeignAddress {
    param([object]$Sender, [object]$Receiver, [object]$SenderInfo, [object]$ReceiverInfo,
          [object]$Result, [int]$WaitMs)
    $before = Invoke-Pj26Node -Endpoint $Receiver
    [uint32]$other = 0x7FFFFFFE
    while ($other -eq $SenderInfo.Id -or $other -eq $ReceiverInfo.Id) { $other-- }
    $to = Invoke-Pj26Node -Endpoint $Sender -Command ('TO {0:X8}' -f $other)
    [byte[]]$tag = [Guid]::NewGuid().ToByteArray()[0..7]
    [byte[]]$probe = [Pj26Link.Frame]::Create(64,$tag,0,1)
    $parser = [Pj26Link.StreamParser]::new()
    $forwarded = $false
    $Sender.Write($probe)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.ElapsedMilliseconds -lt $WaitMs) {
        foreach ($frame in $parser.Feed($Receiver.ReadAvailable())) {
            if ([Pj26Link.Frame]::Same($frame,$probe)) { $forwarded = $true }
        }
        $null = $Sender.ReadAvailable()
        Start-Sleep -Milliseconds 10
    }
    $after = Invoke-Pj26Node -Endpoint $Receiver
    $Result.Verify($before,$after,$SenderInfo.Id,$ReceiverInfo.Id,$other,$to.NextSequence,$forwarded)
    if (-not $Result.Passed) { throw $Result.Error }
}

