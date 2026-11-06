function Get-Pj26PortCandidates {
    param([object[]]$Devices, [string[]]$UsbHardwareIds)
    @($Devices | Where-Object {
        $device = $_
        $matchesId = $false
        foreach ($id in $UsbHardwareIds) {
            if ($device.HardwareId -match ([regex]::Escape($id) + '(?:&|;|$)')) { $matchesId = $true; break }
        }
        $device.Port -match '^COM[1-9][0-9]*$' -and
        $device.InstanceId -match '^USB\\' -and $matchesId
    } | Sort-Object { [int]$_.Port.Substring(3) })
}

function Resolve-Pj26Port {
    param([object[]]$Devices, [string[]]$UsbHardwareIds,
          [string]$PortSetting = 'AUTO', [string]$OverridePort)
    $chosen = if ($OverridePort) { $OverridePort } else { $PortSetting }
    if (-not $chosen -or $chosen -eq 'AUTO') {
        $candidates = @(Get-Pj26PortCandidates -Devices $Devices -UsbHardwareIds $UsbHardwareIds)
        if ($candidates.Count -eq 0) {
            throw 'No compatible USB-serial CH340/CH341 device found. Connect the PJ26 USB cable.'
        }
        if ($candidates.Count -gt 1) {
            $list = ($candidates | ForEach-Object { $_.Port + ': ' + $_.Name }) -join '; '
            throw "Multiple compatible USB-serial devices found: $list. Disconnect the other board or use -OverridePort COMx."
        }
        Write-Verbose ('Auto-detected: ' + $candidates[0].Port + ' - ' + $candidates[0].Name)
        return $candidates[0].Port.ToUpperInvariant()
    }
    if ($chosen -notmatch '^COM[1-9][0-9]*$') { throw 'Port must be AUTO or COMx.' }
    $present = @($Devices | Where-Object { $_.Port -eq $chosen })
    if ($present.Count -ne 1) { throw "Configured port $chosen is not present." }
    if ($present[0].InstanceId -match '^BTH') { throw "Port $chosen is Bluetooth, not the PJ26 USB interface." }
    return $chosen.ToUpperInvariant()
}

function Resolve-Pj26PortPair {
    param([object[]]$Devices, [string[]]$UsbHardwareIds, [string]$PortA, [string]$PortB)
    $candidates = @(Get-Pj26PortCandidates -Devices $Devices -UsbHardwareIds $UsbHardwareIds)
    if ([bool]$PortA -ne [bool]$PortB) { throw 'Specify both -PortA COMx and -PortB COMy, or neither for AUTO.' }
    if ($PortA -and $PortB) {
        if ($PortA -eq $PortB) { throw 'A and B must be different COM ports.' }
        foreach ($portName in @($PortA,$PortB)) {
            if ($portName -notmatch '^COM[1-9][0-9]*$' -or $candidates.Port -notcontains $portName) {
                throw "Port $portName is not a connected compatible CH340/CH341 device. Check List boards."
            }
        }
        return @($PortA.ToUpperInvariant(),$PortB.ToUpperInvariant())
    }
    if ($candidates.Count -ne 2) {
        $names = ($candidates | ForEach-Object { $_.Port }) -join ', '
        throw "AUTO needs exactly two compatible boards; found $($candidates.Count) [$names]. Connect both PJ26 boards, or specify -PortA COMx -PortB COMy."
    }
    return @($candidates[0].Port.ToUpperInvariant(),$candidates[1].Port.ToUpperInvariant())
}
