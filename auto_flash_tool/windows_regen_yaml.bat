@echo off
setlocal
chcp 65001 >nul

set "SCRIPT_DIR=%~dp0"
set "EXE=%SCRIPT_DIR%tool_windows\Meshtastic_Auto_Flash.exe"
set "CLI=%SCRIPT_DIR%CLI.md"
set "YAML=%SCRIPT_DIR%config.yaml"

echo ============================================
echo HermesX AutoFlasher Regenerate YAML
echo ============================================
echo SCRIPT_DIR=%SCRIPT_DIR%
echo EXE=%EXE%
echo CLI=%CLI%
echo YAML=%YAML%
echo.

if not exist "%EXE%" (
  echo [ERROR] EXE not found.
  goto END
)

if not exist "%CLI%" (
  echo [ERROR] CLI.md not found.
  goto END
)

echo [STEP 1] Export config.yaml from CLI.md
echo --------------------------------------------
"%EXE%" --cli-config-path "%CLI%" --export-config-yaml "%YAML%"
echo EXIT_CODE=%ERRORLEVEL%
echo.

echo [STEP 2] Show first 120 lines of config.yaml
echo --------------------------------------------
if exist "%YAML%" (
  powershell -NoProfile -Command "Get-Content -Path '%YAML%' -TotalCount 120"
) else (
  echo [ERROR] config.yaml was not created.
)
echo.

:END
echo ============================================
echo Copy everything in this window and send it back.
echo Press any key to close.
pause >nul
