param(
    [string]$FirmwarePath = "",
    [string]$FirmwareFileName = "HermesX_0.2.8-beta0001-update.bin",
    [string]$CliConfigPath = "",
    [string]$CliConfigFileName = "CLI.md",
    [int]$PostFlashWaitSeconds = 60,
    [int]$BatchSize = 5,
    [int]$BatchWaitSeconds = 0,
    [int]$ReapplyMaxPasses = 1,
    [int]$MeshtasticTimeoutSeconds = 120,
    [int]$MeshtasticRetryCount = 3,
    [int]$MeshtasticRetryDelaySeconds = 5,
    [string]$LogPath = "",
    [bool]$RebootAfterConfig = $true,
    [int]$PostConfigRebootWaitSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Initialize-Log {
    param([string]$Path)

    if (-not $Path) {
        $Path = Join-Path $PSScriptRoot "flash_and_config.log"
    }
    $Script:LogPath = $Path
    $header = ("==== {0} ====" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    Set-Content -LiteralPath $Script:LogPath -Value $header
}

function Write-Log {
    param([string]$Message)

    $line = ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $Message)
    Write-Host $line
    if ($Script:LogPath) {
        Add-Content -LiteralPath $Script:LogPath -Value $line
    }
}

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

function Convert-CliConfigToMap {
    param([string]$ConfigText)

    $map = @{}
    $stack = @()
    $inChannelsBlock = $false
    $lines = $ConfigText -split "`r?`n"

    foreach ($line in $lines) {
        $expanded = $line -replace "`t", "  "
        $trimmed = $expanded.Trim()
        if (-not $trimmed) {
            if ($inChannelsBlock) {
                $inChannelsBlock = $false
            }
            continue
        }
        if ($trimmed -match '^Channels:') {
            $inChannelsBlock = $true
            continue
        }
        if ($inChannelsBlock) {
            continue
        }
        if ($trimmed -match '^Primary channel URL:' -or $trimmed -match '^Complete URL') {
            continue
        }

        $indent = ($expanded -replace "^(\\s*).*", '$1').Length
        if ($indent -le 1) {
            $stack = @()
        }

        $key = $null
        $value = $null
        if ($expanded -match '^\s*"?([A-Za-z0-9_]+)"?\s*:\s*(.*)$') {
            $key = $Matches[1]
            $value = $Matches[2]
        } else {
            continue
        }

        $stack = @($stack | Where-Object { $_.Indent -lt $indent })

        $value = $value.Trim()
        if ($value -eq "" -or $value -eq "{" -or $value -eq "[") {
            $stack += [PSCustomObject]@{ Indent = $indent; Key = $key }
            continue
        }
        if ($value -match '^[\{\[]') {
            continue
        }

        $normalizedValue = $value.Trim()
        if ($normalizedValue.EndsWith(",")) {
            $normalizedValue = $normalizedValue.TrimEnd(',').Trim()
        }
        $normalizedValue = $normalizedValue.Trim('"')
        if ($normalizedValue -eq "[]" -or $normalizedValue -eq "{}") {
            continue
        }

        $path = @()
        foreach ($entry in $stack) {
            $path += $entry.Key
        }
        $path += $key
        $map[($path -join ".")] = $normalizedValue
    }

    return $map
}

function Get-ChannelsBlockFromText {
    param([string]$Text)

    $lines = $Text -split "`r?`n"
    $collect = $false
    $block = @()

    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if (-not $collect) {
            if ($trimmed -match '^Channels:') {
                $collect = $true
            }
            continue
        }

        if (-not $trimmed) {
            break
        }
        if ($trimmed -match '^Primary channel URL:' -or $trimmed -match '^Complete URL') {
            break
        }
        $block += $trimmed
    }

    return @($block)
}

function Normalize-ChannelLines {
    param([array]$Lines)

    return @($Lines | ForEach-Object { ($_ -replace '\s+', ' ').Trim() })
}

function Compare-ChannelsBlock {
    param(
        [array]$Expected,
        [array]$Actual
    )

    $expectedLines = Normalize-ChannelLines -Lines $Expected
    $actualLines = Normalize-ChannelLines -Lines $Actual
    $expectedLines = @($expectedLines)
    $actualLines = @($actualLines)

    if ($expectedLines.Count -ne $actualLines.Count) {
        return $false
    }

    for ($i = 0; $i -lt $expectedLines.Count; $i++) {
        if ($expectedLines[$i] -ne $actualLines[$i]) {
            return $false
        }
    }
    return $true
}

function Get-RepoRoot {
    param([string]$StartPath)

    $current = Get-Item -LiteralPath $StartPath
    while ($current -ne $null) {
        if (Test-Path -LiteralPath (Join-Path $current.FullName ".git")) {
            return $current.FullName
        }
        $current = $current.Parent
    }

    return $StartPath
}

function Find-FilesByName {
    param(
        [string]$Root,
        [string]$FileName,
        [array]$PreferredSubDirs
    )

    $matches = @()
    foreach ($subdir in $PreferredSubDirs) {
        $dir = Join-Path $Root $subdir
        if (Test-Path -LiteralPath $dir) {
            $matches += Get-ChildItem -Path $dir -Filter $FileName -File -Recurse -ErrorAction SilentlyContinue
        }
    }

    if (-not $matches -or $matches.Count -eq 0) {
        $matches = @(Get-ChildItem -Path $Root -Filter $FileName -File -Recurse -ErrorAction SilentlyContinue)
    }

    $matches = @($matches | Where-Object { $_ -ne $null })
    return $matches
}

function Select-FileFromList {
    param(
        [array]$Items,
        [string]$Prompt
    )

    $Items = @($Items | Where-Object { $_ -ne $null } | Sort-Object FullName -Unique)
    if (-not $Items -or $Items.Count -eq 0) {
        return $null
    }
    if ($Items.Count -eq 1) {
        return $Items[0]
    }

    Write-Log $Prompt
    for ($i = 0; $i -lt $Items.Count; $i++) {
        Write-Log ("[{0}] {1}" -f $i, $Items[$i].FullName)
    }
    $choice = Read-Host "Select index"
    if ($choice -as [int] -and $choice -ge 0 -and $choice -lt $Items.Count) {
        return $Items[$choice]
    }
    throw "Invalid selection."
}

function Resolve-FirmwarePath {
    param(
        [string]$Path,
        [string]$FileName
    )

    if ($Path -and (Test-Path -LiteralPath $Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }

    $root = Get-RepoRoot -StartPath $PSScriptRoot
    $preferredDirs = @(
        "release",
        "released",
        "RELEASED",
        "auto_flash_tool\release"
    )

    $matches = @(Find-FilesByName -Root $root -FileName $FileName -PreferredSubDirs $preferredDirs)
    if (-not $matches -or $matches.Count -eq 0) {
        $matches = @(Find-FilesByName -Root $root -FileName "*update.bin" -PreferredSubDirs $preferredDirs)
    }

    $selected = Select-FileFromList -Items ($matches | Sort-Object LastWriteTime -Descending) -Prompt "Select firmware file"
    if ($selected) {
        return $selected.FullName
    }

    $inputPath = Read-Host "Enter firmware path"
    if ($inputPath -and (Test-Path -LiteralPath $inputPath)) {
        return (Resolve-Path -LiteralPath $inputPath).Path
    }
    throw "Firmware not found. Provide -FirmwarePath or place $FileName in a release folder."
}

function Resolve-CliConfigPath {
    param(
        [string]$Path,
        [string]$FileName
    )

    if ($Path -and (Test-Path -LiteralPath $Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }

    $root = Get-RepoRoot -StartPath $PSScriptRoot
    $preferredDirs = @(
        ".",
        "auto_flash_tool"
    )

    $matches = @(Find-FilesByName -Root $root -FileName $FileName -PreferredSubDirs $preferredDirs)
    $candidates = @()
    foreach ($file in $matches) {
        $content = Read-TextFileBestEncoding -Path $file.FullName
        if ($content -match "meshtastic\s+--set" -or $content -match "meshtastic\s+--set-canned-message") {
            $candidates += $file
        }
    }
    $candidates = @($candidates | Sort-Object FullName -Unique)

    if (-not $candidates -or $candidates.Count -eq 0) {
        $candidates = @()
        $scanDirs = @(
            $root,
            (Join-Path $root "auto_flash_tool")
        ) | Select-Object -Unique

        foreach ($dir in $scanDirs) {
            if (-not (Test-Path -LiteralPath $dir)) {
                continue
            }
            $docs = Get-ChildItem -Path $dir -File -Include "*.md","*.txt" -Recurse -ErrorAction SilentlyContinue
            foreach ($doc in $docs) {
                $content = Read-TextFileBestEncoding -Path $doc.FullName
                if ($content -match "meshtastic\s+--set" -or $content -match "meshtastic\s+--set-canned-message") {
                    $candidates += $doc
                }
            }
        }
        $candidates = @($candidates | Sort-Object FullName -Unique)
    }

    $selected = Select-FileFromList -Items $candidates -Prompt "Select CLI config file"
    if ($selected) {
        return $selected.FullName
    }

    $inputPath = Read-Host "Enter CLI config path"
    if ($inputPath -and (Test-Path -LiteralPath $inputPath)) {
        return (Resolve-Path -LiteralPath $inputPath).Path
    }
    throw "CLI config file not found. Provide -CliConfigPath or place $FileName in the repo."
}

function Get-SerialPorts {
    Get-CimInstance Win32_SerialPort |
        Select-Object Name, DeviceID, PNPDeviceID |
        Where-Object { $_.DeviceID -ne "COM1" }
}

function Select-SerialPort {
    param([string]$PreferredPort)

    $ports = @(Get-SerialPorts)
    if (-not $ports) {
        throw "No serial ports detected."
    }

    $candidates = @($ports | Where-Object {
        $_.PNPDeviceID -match "VID_303A" -or $_.Name -match "USB"
    })

    if ($PreferredPort -and ($candidates.DeviceID -contains $PreferredPort)) {
        return $PreferredPort
    }

    if ($candidates.Count -eq 1) {
        return $candidates[0].DeviceID
    }

    if ($candidates.Count -gt 1) {
        Write-Log "Multiple USB serial ports detected:"
        for ($i = 0; $i -lt $candidates.Count; $i++) {
            Write-Log ("[{0}] {1} ({2})" -f $i, $candidates[$i].DeviceID, $candidates[$i].Name)
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

    Write-Log "Flashing firmware to $Port..."
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

function Get-MeshtasticCommands {
    param([string]$Path)

    $text = Read-TextFileBestEncoding -Path $Path
    $lines = $text -split "`r?`n"
    $commands = @()
    $explicitSetCommands = @()
    $explicitCannedCommands = @()
    $primaryUrl = $null
    $completeUrl = $null
    $channelCommand = $null

    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if (-not $trimmed) {
            continue
        }
        if ($trimmed -match "^Primary channel URL:\s*(\S+)$") {
            if (-not $primaryUrl) {
                $primaryUrl = $Matches[1].Trim()
            }
            continue
        }
        if ($trimmed -match "^Complete URL.*:\s*(\S+)$") {
            $completeUrl = $Matches[1].Trim()
            continue
        }
        if ($trimmed -match "^meshtastic\s+--set-canned-message\s+(.+)$") {
            $message = $Matches[1].Trim()
            if ($message.StartsWith('"') -and $message.EndsWith('"')) {
                $message = $message.Substring(1, $message.Length - 2)
            }
            $explicitCannedCommands += [PSCustomObject]@{
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
            $explicitSetCommands += [PSCustomObject]@{
                Type = "SetField"
                Field = $field
                Value = $value
                Raw = $trimmed
            }
            continue
        }
    }

    $expectedMap = Convert-CliConfigToMap -ConfigText $text
    $setCommandMap = @{}
    $setCommandOrder = @()
    foreach ($key in ($expectedMap.Keys | Sort-Object)) {
        $value = $expectedMap[$key]
        if (-not $value) {
            continue
        }
        $cmd = [PSCustomObject]@{
            Type = "SetField"
            Field = $key
            Value = $value
            Raw = "meshtastic --set $key $value"
        }
        $setCommandMap[$key] = $cmd
        $setCommandOrder += $key
    }

    foreach ($cmd in $explicitSetCommands) {
        if (-not $setCommandMap.ContainsKey($cmd.Field)) {
            $setCommandOrder += $cmd.Field
        }
        $setCommandMap[$cmd.Field] = $cmd
    }

    foreach ($key in $setCommandOrder) {
        $commands += $setCommandMap[$key]
    }

    if ($completeUrl) {
        $channelCommand = [PSCustomObject]@{
            Type = "SetChannelUrl"
            Url = $completeUrl
            Raw = "meshtastic --ch-set-url $completeUrl"
        }
    } elseif ($primaryUrl) {
        $channelCommand = [PSCustomObject]@{
            Type = "SetChannelUrl"
            Url = $primaryUrl
            Raw = "meshtastic --ch-set-url $primaryUrl"
        }
    } else {
        throw "Channel URL not found in CLI config. Add Primary channel URL or Complete URL to $Path."
    }

    if ($channelCommand) {
        $commands = @($channelCommand) + $commands
    }

    if ($explicitCannedCommands.Count -gt 0) {
        $commands += $explicitCannedCommands
    }

    return @($commands)
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
        $args = @()
        if ($cmd.Type -eq "SetField") {
            $args = @("--set", $cmd.Field, $cmd.Value)
        } elseif ($cmd.Type -eq "SetCannedMessage") {
            $args = @("--set-canned-message", $cmd.Message)
        } elseif ($cmd.Type -eq "SetChannelUrl") {
            $args = @("--ch-set-url", $cmd.Url)
        } elseif ($cmd.Type -eq "AddChannelUrl") {
            $args = @("--ch-add-url", $cmd.Url)
        }
        if ($args.Count -gt 0) {
            Write-Log ("Executing: meshtastic --port {0} {1}" -f $Port, ($args -join " "))
        }
        Invoke-MeshtasticWithRetry -Port $Port -Args $args -Raw $cmd.Raw
        $count++
        if ($BatchSize -gt 0 -and $BatchWaitSeconds -gt 0 -and ($count % $BatchSize -eq 0)) {
            Write-Log "Batch complete, waiting ${BatchWaitSeconds}s..."
            Start-Sleep -Seconds $BatchWaitSeconds
        }
    }
}

function Invoke-MeshtasticWithRetry {
    param(
        [string]$Port,
        [array]$Args,
        [string]$Raw
    )

    for ($attempt = 1; $attempt -le $MeshtasticRetryCount; $attempt++) {
        $output = & meshtastic --port $Port --timeout $MeshtasticTimeoutSeconds @Args 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0) {
            return
        }
        if ($attempt -lt $MeshtasticRetryCount) {
            Write-Log "meshtastic failed (attempt $attempt/$MeshtasticRetryCount). Retrying in ${MeshtasticRetryDelaySeconds}s..."
            Start-Sleep -Seconds $MeshtasticRetryDelaySeconds
            continue
        }
        throw "meshtastic command failed: $Raw`n$output"
    }
}

function Get-MeshtasticInfo {
    param([string]$Port)

    for ($attempt = 1; $attempt -le $MeshtasticRetryCount; $attempt++) {
        $output = & meshtastic --port $Port --timeout $MeshtasticTimeoutSeconds --info 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0) {
            return $output
        }
        if ($attempt -lt $MeshtasticRetryCount) {
            Write-Log "meshtastic --info failed (attempt $attempt/$MeshtasticRetryCount). Retrying in ${MeshtasticRetryDelaySeconds}s..."
            Start-Sleep -Seconds $MeshtasticRetryDelaySeconds
            continue
        }
        throw "meshtastic --info failed.`n$output"
    }
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
            if ($indent -le 1) {
                $stack = @()
            }
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

    $normalized = $Value.Trim().Trim('"').TrimEnd(',').ToLowerInvariant()
    $number = 0.0
    if ([double]::TryParse($normalized, [ref]$number)) {
        return ("num:{0:R}" -f $number)
    }
    return $normalized
}

function Get-MissingCommands {
    param(
        [array]$Commands,
        [hashtable]$InfoMap,
        [ref]$UnverifiedRef
    )

    $missing = @()
    $missingUnverified = @()
    foreach ($cmd in $Commands) {
        if ($cmd.Type -ne "SetField") {
            continue
        }
        if (-not $InfoMap.ContainsKey($cmd.Field)) {
            $missingUnverified += $cmd
            continue
        }
        $expected = Normalize-Value $cmd.Value
        $actual = Normalize-Value $InfoMap[$cmd.Field]
        if ($expected -ne $actual) {
            $missing += $cmd
        }
    }
    $UnverifiedRef.Value = $missingUnverified
    return $missing
}

function Get-MismatchDetails {
    param(
        [array]$Missing,
        [hashtable]$InfoMap
    )

    $details = @()
    foreach ($cmd in $Missing) {
        if ($cmd.Type -ne "SetField") {
            continue
        }
        $actual = "<not in info>"
        if ($InfoMap.ContainsKey($cmd.Field)) {
            $actual = $InfoMap[$cmd.Field]
        }
        $details += [PSCustomObject]@{
            Command = $cmd.Raw
            Expected = $cmd.Value
            Actual = $actual
        }
    }
    return $details
}

if (-not (Test-CommandExists "python")) {
    throw "python is not available in PATH."
}
if (-not (Test-CommandExists "meshtastic")) {
    throw "meshtastic CLI is not available in PATH."
}

$FirmwarePath = Resolve-FirmwarePath -Path $FirmwarePath -FileName $FirmwareFileName
$CliConfigPath = Resolve-CliConfigPath -Path $CliConfigPath -FileName $CliConfigFileName

$LogPath = if ($LogPath) { $LogPath } else { "" }
Initialize-Log -Path $LogPath
Write-Log "Firmware file: $FirmwarePath"
Write-Log "CLI config file: $CliConfigPath"

$cliText = Read-TextFileBestEncoding -Path $CliConfigPath
$expectedChannels = Get-ChannelsBlockFromText -Text $cliText

$initialPort = Select-SerialPort
Write-Log "Using port: $initialPort"

Invoke-EsptoolFlash -Port $initialPort -Firmware $FirmwarePath
Write-Log "Flash complete. Waiting ${PostFlashWaitSeconds}s for device to reboot..."
Start-Sleep -Seconds $PostFlashWaitSeconds

$portAfter = Select-SerialPort -PreferredPort $initialPort
Write-Log "Using port after reboot: $portAfter"

$commands = Get-MeshtasticCommands -Path $CliConfigPath
if (-not $commands -or $commands.Count -eq 0) {
    throw "No meshtastic --set commands found in $CliConfigPath"
}

Write-Log "Total commands queued: $($commands.Count)"

Invoke-MeshtasticCommands -Port $portAfter -Commands $commands -BatchSize $BatchSize -BatchWaitSeconds $BatchWaitSeconds

if ($RebootAfterConfig) {
    Write-Log "Rebooting device to apply config..."
    Invoke-MeshtasticWithRetry -Port $portAfter -Args @("--reboot") -Raw "meshtastic --reboot"
    Write-Log "Waiting ${PostConfigRebootWaitSeconds}s for device to reboot..."
    Start-Sleep -Seconds $PostConfigRebootWaitSeconds
    $portAfter = Select-SerialPort -PreferredPort $portAfter
    Write-Log "Port after config reboot: $portAfter"
}

$info = Get-MeshtasticInfo -Port $portAfter
$infoMap = Convert-InfoToMap -InfoText $info
$actualChannels = Get-ChannelsBlockFromText -Text $info
$channelMismatch = $false
if ($expectedChannels.Count -gt 0) {
    $channelMismatch = -not (Compare-ChannelsBlock -Expected $expectedChannels -Actual $actualChannels)
}

$unverified = @()
$missing = Get-MissingCommands -Commands $commands -InfoMap $infoMap -UnverifiedRef ([ref]$unverified)
if ($channelMismatch) {
    $channelCommand = $commands | Where-Object { $_.Type -eq "SetChannelUrl" } | Select-Object -First 1
    if ($channelCommand) {
        $missing = @($channelCommand) + @($missing)
    }
}
if ($missing.Count -gt 0) {
    Write-Log "Missing settings detected: $($missing.Count)"
    $details = Get-MismatchDetails -Missing $missing -InfoMap $infoMap
    foreach ($item in $details) {
        Write-Log ("Mismatch: {0} | expected={1} actual={2}" -f $item.Command, $item.Expected, $item.Actual)
    }
    for ($pass = 1; $pass -le $ReapplyMaxPasses; $pass++) {
        Invoke-MeshtasticCommands -Port $portAfter -Commands $missing -BatchSize $BatchSize -BatchWaitSeconds $BatchWaitSeconds
        $info = Get-MeshtasticInfo -Port $portAfter
        $infoMap = Convert-InfoToMap -InfoText $info
        $actualChannels = Get-ChannelsBlockFromText -Text $info
        $channelMismatch = $false
        if ($expectedChannels.Count -gt 0) {
            $channelMismatch = -not (Compare-ChannelsBlock -Expected $expectedChannels -Actual $actualChannels)
        }
        $unverified = @()
        $missing = Get-MissingCommands -Commands $commands -InfoMap $infoMap -UnverifiedRef ([ref]$unverified)
        if ($channelMismatch) {
            $channelCommand = $commands | Where-Object { $_.Type -eq "SetChannelUrl" } | Select-Object -First 1
            if ($channelCommand) {
                $missing = @($channelCommand) + @($missing)
            }
        }
        if ($missing.Count -eq 0) {
            break
        }
    }
}

if ($missing.Count -eq 0) {
    Write-Log "All settings applied successfully."
} else {
    Write-Log "Some settings could not be verified via --info:"
    foreach ($cmd in $missing) {
        Write-Log ("- {0}" -f $cmd.Raw)
    }
}

if ($unverified.Count -gt 0) {
    Write-Log "Skipped verification for settings not present in --info: $($unverified.Count)"
}
