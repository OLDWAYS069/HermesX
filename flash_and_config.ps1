param(
    [string]$FirmwarePath = "G:\geek_guys_oldways\release\HermesX_0.2.8-beta0001-update.bin",
    [string]$CliConfigPath = "G:\geek_guys_oldways\CLI?誘銵?txt",
    [int]$PostFlashWaitSeconds = 60,
    [int]$BatchSize = 5,
    [int]$BatchWaitSeconds = 20,
    [int]$ReapplyMaxPasses = 1
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-TextFileBestEncoding {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        return [System.Text.Encoding]::UTF8.GetString($bytes)
    }

    $utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
    try {
        return $utf8Strict.GetString($bytes)
    } catch {
        $big5 = [System.Text.Encoding]::GetEncoding(950)
        return $big5.GetString($bytes)
    }
}

function Get-SerialPorts {
    Get-CimInstance Win32_SerialPort |
        Select-Object Name, DeviceID, PNPDeviceID |
        Where-Object { $_.DeviceID -ne "COM1" }
}

function Select-SerialPort {
    param([string]$PreferredPort)

    $ports = Get-SerialPorts
    if (-not $ports) {
        throw "No serial ports detected."
    }

    $candidates = $ports | Where-Object {
        $_.PNPDeviceID -match "VID_303A" -or $_.Name -match "USB"
    }

    if ($PreferredPort -and ($candidates.DeviceID -contains $PreferredPort)) {
        return $PreferredPort
    }

    if ($candidates.Count -eq 1) {
        return $candidates[0].DeviceID
    }

    if ($candidates.Count -gt 1) {
        Write-Host "Multiple USB serial ports detected:"
        for ($i = 0; $i -lt $candidates.Count; $i++) {
            Write-Host ("[{0}] {1} ({2})" -f $i, $candidates[$i].DeviceID, $candidates[$i].Name)
        }
        $choice = Read-Host "Select port index"
        if ($choice -as [int] -and $choice -ge 0 -and $choice -lt $candidates.Count) {
            return $candidates[$choice].DeviceID
        }
        throw "Invalid selection."
    }

    if ($ports.Count -eq 1) {
        return $ports[0].DeviceID
    }

    throw "Unable to auto-select a serial port."
}

function Test-CommandExists {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-EsptoolFlash {
    param(
        [string]$Port,
        [string]$Firmware
    )

    Write-Host "Flashing firmware to $Port..."
    $esptoolArgs = @(
        "-m", "esptool",
        "--chip", "esp32s3",
        "--port", $Port,
        "--baud", "115200",
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash",
        "0x10000",
        $Firmware
    )
    & python @esptoolArgs
    if ($LASTEXITCODE -ne 0) {
        throw "esptool failed with exit code $LASTEXITCODE."
    }
}

function Get-MeshtasticSetCommands {
    param([string]$Path)

    $text = Read-TextFileBestEncoding -Path $Path
    $lines = $text -split "`r?`n"
    $commands = @()

    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if (-not $trimmed) {
            continue
        }
        if ($trimmed -notmatch "^meshtastic\s") {
            continue
        }
        if ($trimmed -notmatch "--set") {
            continue
        }

        if ($trimmed -match "^meshtastic\s+--set-canned-message\s+(.+)$") {
            $message = $Matches[1].Trim()
            if ($message.StartsWith('"') -and $message.EndsWith('"')) {
                $message = $message.Substring(1, $message.Length - 2)
            }
            $commands += [PSCustomObject]@{
                Type = "SetCannedMessage"
                Message = $message
                Raw = $trimmed
            }
            continue
        }

        if ($trimmed -match "^meshtastic\s+--set\s+(\S+)\s+(.+)$") {
            $field = $Matches[1].Trim()
            $value = $Matches[2].Trim()
            if ($value.StartsWith('"') -and $value.EndsWith('"')) {
                $value = $value.Substring(1, $value.Length - 2)
            }
            $commands += [PSCustomObject]@{
                Type = "SetField"
                Field = $field
                Value = $value
                Raw = $trimmed
            }
            continue
        }
    }

    return $commands
}

function Invoke-MeshtasticCommands {
    param(
        [string]$Port,
        [array]$Commands,
        [int]$BatchSize,
        [int]$BatchWaitSeconds
    )

    $count = 0
    foreach ($cmd in $Commands) {
        if ($cmd.Type -eq "SetField") {
            & meshtastic --port $Port --set $cmd.Field $cmd.Value
        } elseif ($cmd.Type -eq "SetCannedMessage") {
            & meshtastic --port $Port --set-canned-message $cmd.Message
        }
        if ($LASTEXITCODE -ne 0) {
            throw "meshtastic command failed: $($cmd.Raw)"
        }
        $count++
        if ($count % $BatchSize -eq 0) {
            Write-Host "Batch complete, waiting ${BatchWaitSeconds}s..."
            Start-Sleep -Seconds $BatchWaitSeconds
        }
    }
}

function Get-MeshtasticInfo {
    param([string]$Port)

    $output = & meshtastic --port $Port --info 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "meshtastic --info failed."
    }
    return $output
}

function Convert-InfoToMap {
    param([string]$InfoText)

    $map = @{}
    $stack = @()
    $lines = $InfoText -split "`r?`n"

    foreach ($line in $lines) {
        $expanded = $line -replace "`t", "  "
        if ($expanded -match "^\s*([A-Za-z0-9_]+)\s*:\s*(.*)$") {
            $indent = ($expanded -replace "^(\\s*).*", '$1').Length
            $key = $Matches[1]
            $value = $Matches[2].Trim()

            $stack = @($stack | Where-Object { $_.Indent -lt $indent })

            if ($value -eq "") {
                $stack += [PSCustomObject]@{ Indent = $indent; Key = $key }
            } else {
                $path = @()
                foreach ($entry in $stack) {
                    $path += $entry.Key
                }
                $path += $key
                $normalizedValue = $value.Trim().Trim('"').TrimEnd(',')
                $map[($path -join ".")] = $normalizedValue
            }
        }
    }

    return $map
}

function Normalize-Value {
    param([string]$Value)
    return $Value.Trim().Trim('"').TrimEnd(',').ToLowerInvariant()
}

function Get-MissingCommands {
    param(
        [array]$Commands,
        [hashtable]$InfoMap
    )

    $missing = @()
    foreach ($cmd in $Commands) {
        if ($cmd.Type -ne "SetField") {
            continue
        }
        if (-not $InfoMap.ContainsKey($cmd.Field)) {
            $missing += $cmd
            continue
        }
        $expected = Normalize-Value $cmd.Value
        $actual = Normalize-Value $InfoMap[$cmd.Field]
        if ($expected -ne $actual) {
            $missing += $cmd
        }
    }
    return $missing
}

if (-not (Test-CommandExists "python")) {
    throw "python is not available in PATH."
}
if (-not (Test-CommandExists "meshtastic")) {
    throw "meshtastic CLI is not available in PATH."
}
if (-not (Test-Path -LiteralPath $FirmwarePath)) {
    throw "Firmware not found: $FirmwarePath"
}
if (-not (Test-Path -LiteralPath $CliConfigPath)) {
    throw "CLI config file not found: $CliConfigPath"
}

$initialPort = Select-SerialPort
Write-Host "Using port: $initialPort"

Invoke-EsptoolFlash -Port $initialPort -Firmware $FirmwarePath
Write-Host "Flash complete. Waiting ${PostFlashWaitSeconds}s for device to reboot..."
Start-Sleep -Seconds $PostFlashWaitSeconds

$portAfter = Select-SerialPort -PreferredPort $initialPort
Write-Host "Using port after reboot: $portAfter"

$commands = Get-MeshtasticSetCommands -Path $CliConfigPath
if (-not $commands -or $commands.Count -eq 0) {
    throw "No meshtastic --set commands found in $CliConfigPath"
}

Invoke-MeshtasticCommands -Port $portAfter -Commands $commands -BatchSize $BatchSize -BatchWaitSeconds $BatchWaitSeconds

$info = Get-MeshtasticInfo -Port $portAfter
$infoMap = Convert-InfoToMap -InfoText $info

$missing = Get-MissingCommands -Commands $commands -InfoMap $infoMap
if ($missing.Count -gt 0) {
    Write-Host "Missing settings detected: $($missing.Count)"
    for ($pass = 1; $pass -le $ReapplyMaxPasses; $pass++) {
        Invoke-MeshtasticCommands -Port $portAfter -Commands $missing -BatchSize $BatchSize -BatchWaitSeconds $BatchWaitSeconds
        $info = Get-MeshtasticInfo -Port $portAfter
        $infoMap = Convert-InfoToMap -InfoText $info
        $missing = Get-MissingCommands -Commands $commands -InfoMap $infoMap
        if ($missing.Count -eq 0) {
            break
        }
    }
}

if ($missing.Count -eq 0) {
    Write-Host "All settings applied successfully."
} else {
    Write-Host "Some settings could not be verified via --info:"
    foreach ($cmd in $missing) {
        Write-Host ("- {0}" -f $cmd.Raw)
    }
}
