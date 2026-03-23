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

$publishTargets = @($publishRoot)
$fallbackPublishRoot = Join-Path $runtimeRoot "tool_windows_fresh"
if ($fallbackPublishRoot -ne $publishRoot) {
    $publishTargets += $fallbackPublishRoot
}
$activePublishRoot = $publishRoot
try {
    if (Test-Path $publishRoot) {
        Remove-Item -Recurse -Force $publishRoot -ErrorAction Stop
    }
} catch {
    Write-Warning "Primary publish directory is locked: $publishRoot"
    $activePublishRoot = $fallbackPublishRoot
}
if (Test-Path $activePublishRoot) {
    Remove-Item -Recurse -Force $activePublishRoot
}
New-Item -ItemType Directory -Force -Path $activePublishRoot | Out-Null
Copy-Item (Join-Path $bundleRoot "Meshtastic_Auto_Flash.exe") (Join-Path $activePublishRoot "Meshtastic_Auto_Flash.exe") -Force
Copy-Item -Recurse -Force (Join-Path $bundleRoot "_internal") (Join-Path $activePublishRoot "_internal")
if (Test-Path $audioSourceRoot) {
    Copy-Item -Recurse -Force $audioSourceRoot (Join-Path $activePublishRoot "audio")
}
if (Test-Path $configSourcePath) {
    Copy-Item -Force $configSourcePath (Join-Path $activePublishRoot "config.yaml")
}
if (Test-Path $cliSourcePath) {
    Copy-Item -Force $cliSourcePath (Join-Path $activePublishRoot "CLI.md")
}
if (Test-Path $targetSourceRoot) {
    Copy-Item -Recurse -Force $targetSourceRoot (Join-Path $activePublishRoot "Target")
}
Write-Host "Windows bundle ready at: $activePublishRoot"
