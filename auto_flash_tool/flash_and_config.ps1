param(
    [string]$FirmwarePath = "",
    [string]$FirmwareFileName = "HermesX_0.2.8-beta0002-update.bin",
    [string]$CliConfigPath = "",
    [string]$CliConfigFileName = "CLI.md",
    [int]$PostFlashWaitSeconds = 60,
    [int]$RebootBatchSize = 2,
    [int]$RebootWaitSeconds = 10,
    [int]$ReapplyMaxPasses = 2,
    [int]$PortDetectTimeoutSeconds = 60,
    [int]$PortDetectIntervalSeconds = 2,
    [int]$ReadyTimeoutSeconds = 30,
    [int]$ReadyPollSeconds = 2,
    [int]$ReadyCommandTimeoutSeconds = 10,
    [int]$MeshtasticTimeoutSeconds = 120,
    [int]$MeshtasticRetryCount = 3,
    [int]$MeshtasticRetryDelaySeconds = 5,
    [string]$LogPath = "",
    [bool]$RebootAfterConfig = $true,
    [int]$PostConfigRebootWaitSeconds = 10,
    [bool]$UseTransaction = $false
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
    $topLevelKeys = @(
        "mqtt",
        "lora",
        "position",
        "device",
        "network",
        "display",
        "power",
        "bluetooth",
        "security",
        "canned_message",
        "cannedMessage"
    )
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

        if ($topLevelKeys -contains $key) {
            $stack = @()
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

function Get-ChannelUrlsFromText {
    param([string]$Text)

    $primary = $null
    $complete = $null
    $lines = $Text -split "`r?`n"

    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^Primary channel URL:\s*(\S+)$') {
            $primary = $Matches[1].Trim()
            continue
        }
        if ($trimmed -match '^Complete URL[^:]*:\s*(\S+)$') {
            $complete = $Matches[1].Trim()
            continue
        }
    }

    return [PSCustomObject]@{
        Primary = $primary
        Complete = $complete
    }
}

function Normalize-ChannelUrl {
    param([string]$Url)

    if (-not $Url) {
        return $null
    }
    return $Url.Trim().Trim('"')
}

function Get-PreferredChannelUrl {
    param([PSCustomObject]$Urls)

    if ($Urls -and $Urls.Complete) {
        return Normalize-ChannelUrl -Url $Urls.Complete
    }
    if ($Urls -and $Urls.Primary) {
        return Normalize-ChannelUrl -Url $Urls.Primary
    }
    return $null
}

function Normalize-ChannelLines {
    param([array]$Lines)

    return @($Lines | ForEach-Object { ($_ -replace '\s+', ' ').Trim() })
}

function Convert-CamelToSnake {
    param([string]$Value)

    if (-not $Value) {
        return $Value
    }
    $snake = $Value -creplace '([a-z0-9])([A-Z])', '$1_$2'
    return $snake.ToLowerInvariant()
}

function Get-FieldAckVariants {
    param([string]$Field)

    if (-not $Field) {
        return @()
    }

    $parts = $Field -split '\.'
    if ($parts.Length -le 1) {
        return @($Field)
    }

    $prefix = ($parts[0..($parts.Length - 2)] -join ".") + "."
    $last = $parts[-1]
    $snakeLast = Convert-CamelToSnake -Value $last
    $variants = @($Field)
    $snakeField = $prefix + $snakeLast
    if ($snakeField -ne $Field) {
        $variants += $snakeField
    }

    return @($variants | Sort-Object -Unique)
}

function Normalize-InfoPath {
    param([string]$Path)

    if (-not $Path) {
        return $Path
    }

    if ($Path.StartsWith("Preferences.")) {
        $Path = $Path.Substring("Preferences.".Length)
    }
    if ($Path.StartsWith("Module preferences.")) {
        $Path = $Path.Substring("Module preferences.".Length)
    }
    if ($Path.StartsWith("cannedMessage.")) {
        $suffix = $Path.Substring("cannedMessage.".Length)
        $parts = $suffix -split '\.'
        $parts = $parts | ForEach-Object { Convert-CamelToSnake $_ }
        $Path = "canned_message." + ($parts -join ".")
    }

    return $Path
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

function Wait-ForSerialPort {
    param(
        [string]$PreferredPort,
        [int]$TimeoutSeconds,
        [int]$PollSeconds
    )

    $timeout = if ($TimeoutSeconds -gt 0) { $TimeoutSeconds } else { 60 }
    $poll = if ($PollSeconds -gt 0) { $PollSeconds } else { 2 }
    $deadline = (Get-Date).AddSeconds($timeout)

    do {
        try {
            return Select-SerialPort -PreferredPort $PreferredPort
        } catch {
            if ($_.Exception.Message -notmatch "No serial ports detected") {
                throw
            }
            if ((Get-Date) -ge $deadline) {
                throw "No serial ports detected after waiting ${timeout}s."
            }
            Write-Log "No serial ports detected yet. Waiting ${poll}s..."
            Start-Sleep -Seconds $poll
        }
    } while ($true)
}

function Wait-ForMeshtasticReady {
    param(
        [string]$Port,
        [int]$TimeoutSeconds,
        [int]$PollSeconds,
        [int]$CommandTimeoutSeconds
    )

    $timeout = if ($TimeoutSeconds -gt 0) { $TimeoutSeconds } else { 30 }
    $poll = if ($PollSeconds -gt 0) { $PollSeconds } else { 2 }
    $cmdTimeout = if ($CommandTimeoutSeconds -gt 0) { $CommandTimeoutSeconds } else { 10 }
    $deadline = (Get-Date).AddSeconds($timeout)

    do {
        $outputLines = @()
        $prevErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & meshtastic --port $Port --timeout $cmdTimeout --info 2>&1 | ForEach-Object {
                $line = $_.ToString()
                if ($null -ne $line) {
                    $outputLines += $line
                }
            }
        } finally {
            $ErrorActionPreference = $prevErrorActionPreference
        }

        if ($LASTEXITCODE -eq 0 -and ($outputLines -join "`n") -match "(?i)connected to radio") {
            Write-Log "Device ready."
            return
        }

        if ((Get-Date) -ge $deadline) {
            throw "Device not ready after ${timeout}s."
        }
        Write-Log "Device not ready yet. Waiting ${poll}s..."
        Start-Sleep -Seconds $poll
    } while ($true)
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
    $explicitChannel = $false
    $primaryUrl = $null
    $completeUrl = $null
    $channelInsertIndex = $null
    $skipChannelsBlock = $false
    $section = $null
    $sectionIndent = 0
    $topLevelKeys = @(
        "mqtt",
        "lora",
        "position",
        "device",
        "network",
        "display",
        "power",
        "bluetooth",
        "security",
        "canned_message",
        "cannedMessage"
    )

    foreach ($line in $lines) {
        $expanded = $line -replace "`t", "  "
        $trimmed = $expanded.Trim()
        if (-not $trimmed) {
            if ($skipChannelsBlock) {
                $skipChannelsBlock = $false
            }
            if ($section) {
                $section = $null
            }
            continue
        }
        if ($trimmed -match '^Channels:') {
            $skipChannelsBlock = $true
            $section = $null
            continue
        }
        if ($skipChannelsBlock) {
            continue
        }
        if ($trimmed -match "^Primary channel URL:\s*(\S+)$") {
            if (-not $primaryUrl) {
                $primaryUrl = $Matches[1].Trim()
            }
            if ($null -eq $channelInsertIndex) {
                $channelInsertIndex = $commands.Count
            }
            continue
        }
        if ($trimmed -match '^Complete URL[^:]*:\s*(\S+)$') {
            $completeUrl = $Matches[1].Trim()
            if ($null -eq $channelInsertIndex) {
                $channelInsertIndex = $commands.Count
            }
            continue
        }
        if ($trimmed -match '^meshtastic\s+--ch-set-url\s+(.+)$') {
            $explicitChannel = $true
            $commands += [PSCustomObject]@{
                Type = "SetChannelUrl"
                Url = $Matches[1].Trim()
                Raw = $trimmed
            }
            continue
        }
        if ($trimmed -match '^meshtastic\s+--ch-add-url\s+(.+)$') {
            $explicitChannel = $true
            $commands += [PSCustomObject]@{
                Type = "AddChannelUrl"
                Url = $Matches[1].Trim()
                Raw = $trimmed
            }
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

        $indent = ($expanded -replace "^(\\s*).*", '$1').Length
        if ($expanded -match '^\s*"?([A-Za-z0-9_]+)"?\s*:\s*\{\s*$') {
            $key = $Matches[1]
            if ($topLevelKeys -contains $key) {
                $section = $key
                $sectionIndent = $indent
                continue
            }
        }

        if ($section) {
            if ($indent -le $sectionIndent) {
                $section = $null
                continue
            }
            if ($expanded -match '^\s*"?([A-Za-z0-9_]+)"?\s*:\s*(.+)$') {
                $key = $Matches[1]
                $value = $Matches[2].Trim()
                if ($value -eq "" -or $value -eq "{" -or $value -eq "[") {
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
                $field = "$section.$key"
                $commands += [PSCustomObject]@{
                    Type = "SetField"
                    Field = $field
                    Value = $normalizedValue
                    Raw = "meshtastic --set $field $normalizedValue"
                }
            }
        }
    }

    if (-not $explicitChannel -and ($completeUrl -or $primaryUrl)) {
        $url = if ($completeUrl) { $completeUrl } else { $primaryUrl }
        $channelCommand = [PSCustomObject]@{
            Type = "SetChannelUrl"
            Url = $url
            Raw = "meshtastic --ch-set-url $url"
        }
        if ($null -eq $channelInsertIndex -or $channelInsertIndex -le 0) {
            $commands = @($channelCommand) + $commands
        } elseif ($channelInsertIndex -ge $commands.Count) {
            $commands += $channelCommand
        } else {
            $head = @($commands[0..($channelInsertIndex - 1)])
            $tail = @($commands[$channelInsertIndex..($commands.Count - 1)])
            $commands = @($head) + @($channelCommand) + @($tail)
        }
    }

    return @($commands)
}

function Test-RebootCommand {
    param([PSCustomObject]$Command)

    if ($Command.Type -eq "SetChannelUrl" -or $Command.Type -eq "AddChannelUrl") {
        return $true
    }
    if ($Command.Type -eq "SetField") {
        $field = $Command.Field
        if ($field) {
            $field = $field.ToLowerInvariant()
            $rebootPrefixes = @(
                "device.",
                "position.",
                "power.",
                "network.",
                "display.",
                "lora.",
                "bluetooth.",
                "security.",
                "mqtt.",
                "serial.",
                "external_notification.",
                "store_forward.",
                "range_test.",
                "telemetry.",
                "canned_message.",
                "audio.",
                "remote_hardware.",
                "neighbor_info.",
                "ambient_lighting.",
                "detection_sensor.",
                "paxcounter."
            )
            foreach ($prefix in $rebootPrefixes) {
                if ($field.StartsWith($prefix)) {
                    return $true
                }
            }
        }
    }

    return $false
}

function Build-MeshtasticArgs {
    param([array]$Commands)

    $args = @()
    foreach ($cmd in $Commands) {
        if ($cmd.Type -eq "SetField") {
            $args += @("--set", $cmd.Field, $cmd.Value)
        } elseif ($cmd.Type -eq "SetCannedMessage") {
            $args += @("--set-canned-message", $cmd.Message)
        } elseif ($cmd.Type -eq "SetChannelUrl") {
            $args += @("--ch-set-url", $cmd.Url)
        } elseif ($cmd.Type -eq "AddChannelUrl") {
            $args += @("--ch-add-url", $cmd.Url)
        }
    }

    return $args
}

function Get-CommandAckPatterns {
    param([PSCustomObject]$Command)

    $patterns = @()
    switch ($Command.Type) {
        "SetField" {
            if ($Command.Field) {
                $variants = Get-FieldAckVariants -Field $Command.Field
                foreach ($variant in $variants) {
                    $escaped = [Regex]::Escape($variant)
                    $patterns += "(?i)\bset\s+$escaped\s+to\b"
                }
            }
        }
        "SetCannedMessage" {
            $patterns += "(?i)\bsetting\s+canned\s+plugin\s+message\b"
            $patterns += "(?i)\bset\s+canned\s+message\b"
            $patterns += "(?i)\bsetting\s+canned\s+message\b"
        }
        "SetChannelUrl" {
            # meshtastic CLI does not print a set/ack line for ch-set-url in v2.6.x
        }
        "AddChannelUrl" {
            # meshtastic CLI does not print a set/ack line for ch-add-url in v2.6.x
        }
    }

    return @($patterns)
}

function Invoke-MeshtasticCapture {
    param(
        [string]$Port,
        [array]$MeshtasticArgs,
        [string]$Raw
    )

    for ($attempt = 1; $attempt -le $MeshtasticRetryCount; $attempt++) {
        $outputLines = @()
        $prevErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & meshtastic --port $Port --timeout $MeshtasticTimeoutSeconds @MeshtasticArgs 2>&1 | ForEach-Object {
                $line = $_.ToString()
                if ($null -ne $line) {
                    $outputLines += $line
                }
            }
        } finally {
            $ErrorActionPreference = $prevErrorActionPreference
        }
        $output = ($outputLines -join "`n")
        if ($LASTEXITCODE -eq 0) {
            foreach ($line in $outputLines) {
                if ($line -ne "") {
                    Write-Log $line
                }
            }
            return $output
        }

        if ($attempt -lt $MeshtasticRetryCount) {
            Write-Log "meshtastic failed (attempt $attempt/$MeshtasticRetryCount). Retrying in ${MeshtasticRetryDelaySeconds}s..."
            Start-Sleep -Seconds $MeshtasticRetryDelaySeconds
            continue
        }
        throw "meshtastic command failed: $Raw`n$output"
    }
}

function Test-MeshtasticOutputForCommands {
    param(
        [string]$Output,
        [array]$Commands,
        [ref]$MissingRef
    )

    $missing = @()
    $text = $Output
    if (-not $text) {
        $missing += "No output from meshtastic"
        $MissingRef.Value = $missing
        return $false
    }

    if ($text -notmatch "(?i)connected to radio") {
        $missing += "Missing: Connected to radio"
    }

    foreach ($cmd in $Commands) {
        $patterns = @(Get-CommandAckPatterns -Command $cmd)
        if ($patterns.Count -eq 0) {
            continue
        }
        $matched = $false
        foreach ($pattern in $patterns) {
            if ($text -match $pattern) {
                $matched = $true
                break
            }
        }
        if (-not $matched) {
            $missing += "Missing ack for: $($cmd.Raw)"
        }
    }

    $MissingRef.Value = $missing
    return ($missing.Count -eq 0)
}

function Invoke-MeshtasticCommands {
    param(
        [string]$Port,
        [array]$Commands,
        [int]$RebootBatchSize,
        [int]$RebootWaitSeconds,
        [bool]$UseTransaction
    )

    $rebootCommands = @()
    $normalCommands = @()
    foreach ($cmd in $Commands) {
        if (Test-RebootCommand -Command $cmd) {
            $rebootCommands += $cmd
        } else {
            $normalCommands += $cmd
        }
    }

    if ($rebootCommands.Count -gt 0) {
        $batchSize = if ($RebootBatchSize -gt 0) { $RebootBatchSize } else { 1 }
        $batchIndex = 1
        for ($i = 0; $i -lt $rebootCommands.Count; $i += $batchSize) {
            $end = [Math]::Min($i + $batchSize - 1, $rebootCommands.Count - 1)
            $batch = @($rebootCommands[$i..$end])
            Write-Log ("Executing reboot batch #{0} ({1} commands)..." -f $batchIndex, $batch.Count)
            foreach ($cmd in $batch) {
                Write-Log ("Queued: {0}" -f $cmd.Raw)
            }
            $meshtasticArgs = Build-MeshtasticArgs -Commands $batch
            Write-Log ("Executing: meshtastic --port {0} {1}" -f $Port, ($meshtasticArgs -join " "))
            Invoke-MeshtasticWithRetry -Port $Port -MeshtasticArgs $meshtasticArgs -Raw ($batch | ForEach-Object { $_.Raw } | Out-String) -ExpectedCommands $batch -RequireAck $true
            $batchIndex++
            if ($RebootWaitSeconds -gt 0) {
                Write-Log "Waiting ${RebootWaitSeconds}s for reboot..."
                Start-Sleep -Seconds $RebootWaitSeconds
                $Port = Wait-ForSerialPort -PreferredPort $Port -TimeoutSeconds $PortDetectTimeoutSeconds -PollSeconds $PortDetectIntervalSeconds
                Write-Log "Port after reboot: $Port"
                Wait-ForMeshtasticReady -Port $Port -TimeoutSeconds $ReadyTimeoutSeconds -PollSeconds $ReadyPollSeconds -CommandTimeoutSeconds $ReadyCommandTimeoutSeconds
            }
        }
    }

    if ($normalCommands.Count -gt 0) {
        $needsTransaction = $UseTransaction -and ($normalCommands | Where-Object { $_.Type -eq "SetField" -or $_.Type -eq "SetCannedMessage" }).Count -gt 0
        if ($needsTransaction) {
            Write-Log "Opening settings transaction..."
            Invoke-MeshtasticWithRetry -Port $Port -MeshtasticArgs @("--begin-edit") -Raw "meshtastic --begin-edit"
        }

        Write-Log ("Executing non-reboot batch ({0} commands)..." -f $normalCommands.Count)
        foreach ($cmd in $normalCommands) {
            Write-Log ("Queued: {0}" -f $cmd.Raw)
        }
        $meshtasticArgs = Build-MeshtasticArgs -Commands $normalCommands
        Write-Log ("Executing: meshtastic --port {0} {1}" -f $Port, ($meshtasticArgs -join " "))
        Invoke-MeshtasticWithRetry -Port $Port -MeshtasticArgs $meshtasticArgs -Raw ($normalCommands | ForEach-Object { $_.Raw } | Out-String) -ExpectedCommands $normalCommands -RequireAck $true

        if ($needsTransaction) {
            Write-Log "Committing settings transaction..."
            Invoke-MeshtasticWithRetry -Port $Port -MeshtasticArgs @("--commit-edit") -Raw "meshtastic --commit-edit"
        }
    }

    return $Port
}

function Invoke-MeshtasticWithRetry {
    param(
        [string]$Port,
        [array]$MeshtasticArgs,
        [string]$Raw,
        [array]$ExpectedCommands = @(),
        [bool]$RequireAck = $false
    )

    for ($attempt = 1; $attempt -le $MeshtasticRetryCount; $attempt++) {
        $outputLines = @()
        $prevErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & meshtastic --port $Port --timeout $MeshtasticTimeoutSeconds @MeshtasticArgs 2>&1 | ForEach-Object {
                $line = $_.ToString()
                if ($null -ne $line) {
                    $outputLines += $line
                }
            }
        } finally {
            $ErrorActionPreference = $prevErrorActionPreference
        }
        $output = ($outputLines -join "`n")
        $success = ($LASTEXITCODE -eq 0)
        if ($success -and $outputLines.Count -gt 0) {
            foreach ($line in $outputLines) {
                if ($line -ne "") {
                    Write-Log $line
                }
            }
        }
        if ($success -and $RequireAck) {
            $missing = @()
            $ok = Test-MeshtasticOutputForCommands -Output $output -Commands $ExpectedCommands -MissingRef ([ref]$missing)
            if (-not $ok) {
                $success = $false
                if ($missing.Count -gt 0) {
                    Write-Log ("Missing expected CLI output:`n{0}" -f ($missing -join "`n"))
                }
            }
        }
        if ($success) {
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

function Get-ExpectedCannedMessage {
    param([array]$Commands)

    $messages = @($Commands | Where-Object { $_.Type -eq "SetCannedMessage" })
    if ($messages.Count -eq 0) {
        return $null
    }
    return $messages[-1].Message
}

function Get-CannedMessageFromOutput {
    param([string]$Output)

    if (-not $Output) {
        return $null
    }

    $match = [regex]::Match($Output, '(?im)^canned_plugin_message\s*:\s*(.+)$')
    if ($match.Success) {
        return $match.Groups[1].Value.Trim()
    }

    $lines = $Output -split "`r?`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -and $_ -notmatch '(?i)^connected to radio$' }
    if ($lines.Count -gt 0) {
        return $lines[-1]
    }
    return $null
}

function Get-CannedMessageFromDevice {
    param([string]$Port)

    $output = Invoke-MeshtasticCapture -Port $Port -MeshtasticArgs @("--get-canned-message") -Raw "meshtastic --get-canned-message"
    return Get-CannedMessageFromOutput -Output $output
}

function Get-MeshtasticInfo {
    param([string]$Port)

    return Invoke-MeshtasticCapture -Port $Port -MeshtasticArgs @("--info") -Raw "meshtastic --info"
}

function Get-InfoChannelUrl {
    param([string]$InfoText)

    $urls = Get-ChannelUrlsFromText -Text $InfoText
    return Get-PreferredChannelUrl -Urls $urls
}

function Get-ChannelUrlCommandIfMismatch {
    param(
        [string]$ExpectedUrl,
        [string]$ActualUrl
    )

    if (-not $ExpectedUrl) {
        return $null
    }

    $expectedNorm = Normalize-ChannelUrl -Url $ExpectedUrl
    $actualNorm = Normalize-ChannelUrl -Url $ActualUrl
    if ($expectedNorm -and $actualNorm -and $expectedNorm -ne $actualNorm) {
        return [PSCustomObject]@{
            Type = "SetChannelUrl"
            Url = $expectedNorm
            Raw = "meshtastic --ch-set-url $expectedNorm"
        }
    }

    return $null
}

function Write-MismatchDetails {
    param([array]$Details)

    foreach ($item in $Details) {
        Write-Log ("Mismatch: {0} (expected={1} actual={2})" -f $item.Command, $item.Expected, $item.Actual)
    }
}
function Convert-InfoToMap {
    param([string]$InfoText)

    $map = @{}
    $stack = @()
    $lines = $InfoText -split "`r?`n"

    foreach ($line in $lines) {
        $expanded = $line -replace "`t", "  "
        if ($expanded -match '^\s*"?([A-Za-z0-9_ ]+)"?\s*:\s*(.*)$') {
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
                    if ($entry.Key) {
                        $path += $entry.Key
                    }
                }
                $path += $key
                $normalizedValue = $value.Trim().Trim('"').TrimEnd(',')
                $fullPath = Normalize-InfoPath -Path ($path -join ".")
                if ($fullPath) {
                    $map[$fullPath] = $normalizedValue
                }
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
$expectedChannelUrls = Get-ChannelUrlsFromText -Text $cliText
$expectedChannelUrl = Get-PreferredChannelUrl -Urls $expectedChannelUrls

$initialPort = Select-SerialPort
Write-Log "Using port: $initialPort"

Invoke-EsptoolFlash -Port $initialPort -Firmware $FirmwarePath
Write-Log "Flash complete. Waiting ${PostFlashWaitSeconds}s for device to reboot..."
Start-Sleep -Seconds $PostFlashWaitSeconds

$portAfter = Wait-ForSerialPort -PreferredPort $initialPort -TimeoutSeconds $PortDetectTimeoutSeconds -PollSeconds $PortDetectIntervalSeconds
Write-Log "Using port after reboot: $portAfter"
Wait-ForMeshtasticReady -Port $portAfter -TimeoutSeconds $ReadyTimeoutSeconds -PollSeconds $ReadyPollSeconds -CommandTimeoutSeconds $ReadyCommandTimeoutSeconds

$commands = Get-MeshtasticCommands -Path $CliConfigPath
if (-not $commands -or $commands.Count -eq 0) {
    throw "No meshtastic --set commands found in $CliConfigPath"
}

Write-Log "Total commands queued: $($commands.Count)"

$portAfter = Invoke-MeshtasticCommands -Port $portAfter -Commands $commands -RebootBatchSize $RebootBatchSize -RebootWaitSeconds $RebootWaitSeconds -UseTransaction $UseTransaction

if ($RebootAfterConfig) {
    Write-Log "Rebooting device to apply config..."
    Invoke-MeshtasticWithRetry -Port $portAfter -MeshtasticArgs @("--reboot") -Raw "meshtastic --reboot"
    Write-Log "Waiting ${PostConfigRebootWaitSeconds}s for device to reboot..."
    Start-Sleep -Seconds $PostConfigRebootWaitSeconds
    $portAfter = Wait-ForSerialPort -PreferredPort $portAfter -TimeoutSeconds $PortDetectTimeoutSeconds -PollSeconds $PortDetectIntervalSeconds
    Write-Log "Port after config reboot: $portAfter"
    Wait-ForMeshtasticReady -Port $portAfter -TimeoutSeconds $ReadyTimeoutSeconds -PollSeconds $ReadyPollSeconds -CommandTimeoutSeconds $ReadyCommandTimeoutSeconds
}

$maxVerifyPasses = if ($ReapplyMaxPasses -gt 0) { $ReapplyMaxPasses } else { 1 }
$verifyOk = $false
for ($pass = 1; $pass -le $maxVerifyPasses; $pass++) {
    Write-Log "Verifying device settings (pass $pass/$maxVerifyPasses)..."
    $infoText = Get-MeshtasticInfo -Port $portAfter
    $infoMap = Convert-InfoToMap -InfoText $infoText
    $unverified = $null
    $missing = Get-MissingCommands -Commands $commands -InfoMap $infoMap -UnverifiedRef ([ref]$unverified)
    $details = Get-MismatchDetails -Missing $missing -InfoMap $infoMap

    $actualChannelUrl = Get-InfoChannelUrl -InfoText $infoText
    $channelCommand = Get-ChannelUrlCommandIfMismatch -ExpectedUrl $expectedChannelUrl -ActualUrl $actualChannelUrl

    if (($missing.Count -eq 0) -and (-not $channelCommand)) {
        Write-Log "Device settings verified."
        if ($unverified -and $unverified.Count -gt 0) {
            Write-Log ("Skipped verification for settings not present in --info: {0}" -f $unverified.Count)
        }
        $verifyOk = $true
        break
    }

    if ($details.Count -gt 0) {
        Write-MismatchDetails -Details $details
    }
    if ($channelCommand) {
        Write-Log ("Channel URL mismatch detected; will reapply: {0}" -f $channelCommand.Raw)
    }
    if ($unverified -and $unverified.Count -gt 0) {
        Write-Log ("Skipped verification for settings not present in --info: {0}" -f $unverified.Count)
    }

    $reapply = @()
    if ($missing.Count -gt 0) {
        $reapply += $missing
    }
    if ($channelCommand) {
        $reapply += $channelCommand
    }
    if ($reapply.Count -eq 0) {
        break
    }

    if ($pass -lt $maxVerifyPasses) {
        Write-Log ("Reapplying {0} mismatched settings..." -f $reapply.Count)
        $portAfter = Invoke-MeshtasticCommands -Port $portAfter -Commands $reapply -RebootBatchSize $RebootBatchSize -RebootWaitSeconds $RebootWaitSeconds -UseTransaction $UseTransaction
        Start-Sleep -Seconds 2
    }
}
if (-not $verifyOk) {
    throw "Verification failed after $maxVerifyPasses attempt(s)."
}

$expectedCannedMessage = Get-ExpectedCannedMessage -Commands $commands
if ($expectedCannedMessage) {
    $maxPasses = if ($ReapplyMaxPasses -gt 0) { $ReapplyMaxPasses } else { 1 }
    $verified = $false
    for ($pass = 1; $pass -le $maxPasses; $pass++) {
        Write-Log "Verifying canned message (pass $pass/$maxPasses)..."
        $actualMessage = Get-CannedMessageFromDevice -Port $portAfter
        if ($actualMessage -eq $expectedCannedMessage) {
            Write-Log "Canned message verified."
            $verified = $true
            break
        }
        $actualDisplay = if ($actualMessage) { $actualMessage } else { "<none>" }
        Write-Log ("Canned message mismatch: expected=""{0}"" actual=""{1}""" -f $expectedCannedMessage, $actualDisplay)
        if ($pass -lt $maxPasses) {
            $setCmd = [PSCustomObject]@{
                Type = "SetCannedMessage"
                Message = $expectedCannedMessage
                Raw = "meshtastic --set-canned-message $expectedCannedMessage"
            }
            Write-Log "Reapplying canned message..."
            Invoke-MeshtasticWithRetry -Port $portAfter -MeshtasticArgs @("--set-canned-message", $expectedCannedMessage) -Raw $setCmd.Raw -ExpectedCommands @($setCmd) -RequireAck $true
            Start-Sleep -Seconds 2
        }
    }
    if (-not $verified) {
        throw "Canned message verification failed after $maxPasses attempt(s)."
    }
}

Write-Log "Command execution complete."
