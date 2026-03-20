@echo off
setlocal
chcp 65001 >nul

set "ROOT=%~dp0"
set "EXE=%ROOT%tool_windows\Meshtastic_Auto_Flash.exe"
set "CFG=%ROOT%CLI.md"

if not exist "%EXE%" (
  echo [ERROR] EXE not found:
  echo %EXE%
  pause
  exit /b 1
)

if not exist "%CFG%" (
  echo [ERROR] CLI.md not found:
  echo %CFG%
  pause
  exit /b 1
)

pushd "%ROOT%"
"%EXE%" --config-path "%CFG%"
set "EXIT_CODE=%ERRORLEVEL%"
popd

echo.
echo EXIT_CODE=%EXIT_CODE%
pause
