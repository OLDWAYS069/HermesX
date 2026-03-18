param(
    [string]$PythonExe = "C:\Users\OLDDWAYS\AppData\Local\Programs\Python\Python313\python.exe"
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

if (-not (Test-Path $PythonExe)) {
    throw "Python executable not found: $PythonExe"
}

Write-Host "Building standalone Windows bundle..."
& $PythonExe -m PyInstaller --noconfirm --clean --workpath $buildRoot --distpath $distRoot $specPath
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller build failed with exit code $LASTEXITCODE"
}

if (Test-Path $publishRoot) {
    Remove-Item -Recurse -Force $publishRoot
}
New-Item -ItemType Directory -Force -Path $publishRoot | Out-Null
Copy-Item (Join-Path $bundleRoot "Meshtastic_Auto_Flash.exe") (Join-Path $publishRoot "Meshtastic_Auto_Flash.exe") -Force
Copy-Item -Recurse -Force (Join-Path $bundleRoot "_internal") (Join-Path $publishRoot "_internal")
Write-Host "Windows bundle ready at: $publishRoot"
