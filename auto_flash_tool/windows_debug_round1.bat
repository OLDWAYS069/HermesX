@echo off
setlocal
chcp 65001 >nul

set "ROOT=%~dp0"
set "EXE=%ROOT%tool_windows\Meshtastic_Auto_Flash.exe"
set "CLI=%ROOT%CLI.md"
set "YAML=%ROOT%config.yaml"

echo ============================================
echo HermesX AutoFlasher Windows Debug Round 1
echo ============================================
echo ROOT=%ROOT%
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

echo [STEP 1] Show EXE help
echo --------------------------------------------
"%EXE%" --help
echo.

echo [STEP 2] Show first 80 lines of CLI.md
echo --------------------------------------------
powershell -NoProfile -Command "Get-Content -Path '%CLI%' -TotalCount 80"
echo.

echo [STEP 3] Export YAML from CLI.md
echo --------------------------------------------
"%EXE%" --cli-config-path "%CLI%" --export-config-yaml "%YAML%"
echo EXIT_CODE=%ERRORLEVEL%
echo.

echo [STEP 4] Show first 120 lines of config.yaml
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
