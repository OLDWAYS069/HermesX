param(
    [string]$PythonExe = ""
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$builderRoot = (Resolve-Path (Join-Path $scriptDir "..\.." )).Path
$repoRoot = (Resolve-Path (Join-Path $builderRoot "..\.." )).Path
$runtimeRoot = Join-Path $repoRoot "auto_flash_tool"
$specPath = Join-Path $builderRoot "platform\shared\meshtastic_auto_flash.spec"
$buildRoot = Join-Path $builderRoot "build\windows"
$distRoot = Join-Path $builderRoot "dist\windows"
$bundleRoot = Join-Path $distRoot "Meshtastic_Auto_Flash"
$publishRoot = Join-Path $runtimeRoot "tool_windows"
$audioSourceRoot = Join-Path $runtimeRoot "audio"
$configSourcePath = Join-Path $runtimeRoot "config.yaml"
$cliSourcePath = Join-Path $runtimeRoot "CLI.md"
$targetSourceRoot = Join-Path $runtimeRoot "Target"

function Copy-PublishPayload {
    param(
        [Parameter(Mandatory = $true)]
        [string]$DestinationRoot
    )

    if (Test-Path $DestinationRoot) {
        Remove-Item -Recurse -Force $DestinationRoot -ErrorAction Stop
    }

    New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null
    Copy-Item (Join-Path $bundleRoot "Meshtastic_Auto_Flash.exe") (Join-Path $DestinationRoot "Meshtastic_Auto_Flash.exe") -Force
    Copy-Item -Recurse -Force (Join-Path $bundleRoot "_internal") (Join-Path $DestinationRoot "_internal")

    if (Test-Path $audioSourceRoot) {
        Copy-Item -Recurse -Force $audioSourceRoot (Join-Path $DestinationRoot "audio")
    }
    if (Test-Path $configSourcePath) {
        Copy-Item -Force $configSourcePath (Join-Path $DestinationRoot "config.yaml")
    }
    if (Test-Path $cliSourcePath) {
        Copy-Item -Force $cliSourcePath (Join-Path $DestinationRoot "CLI.md")
    }
    if (Test-Path $targetSourceRoot) {
        Copy-Item -Recurse -Force $targetSourceRoot (Join-Path $DestinationRoot "Target")
    }
}

function Test-PublishPayload {
    param(
        [Parameter(Mandatory = $true)]
        [string]$DestinationRoot
    )

    $requiredPaths = @(
        (Join-Path $DestinationRoot "Meshtastic_Auto_Flash.exe"),
        (Join-Path $DestinationRoot "_internal\meshtastic\util.py"),
        (Join-Path $DestinationRoot "_internal\requests\adapters.py"),
        (Join-Path $DestinationRoot "_internal\certifi\cacert.pem")
    )

    foreach ($requiredPath in $requiredPaths) {
        if (-not (Test-Path $requiredPath)) {
            throw "Publish validation failed. Missing required file: $requiredPath"
        }
    }
}

if (-not $PythonExe) {
    $venvPython = Join-Path $repoRoot ".venv\Scripts\python.exe"
    if (Test-Path $venvPython) {
        $PythonExe = $venvPython
    }
}

if (-not (Test-Path $PythonExe)) {
    throw "Python executable not found. Pass -PythonExe or create .venv at: $repoRoot\.venv"
}

Write-Host "Building standalone Windows bundle..."
& $PythonExe -m PyInstaller --noconfirm --clean --workpath $buildRoot --distpath $distRoot $specPath
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller build failed with exit code $LASTEXITCODE"
}

Copy-PublishPayload -DestinationRoot $publishRoot
Test-PublishPayload -DestinationRoot $publishRoot

Write-Host "Windows bundle ready at: $publishRoot"