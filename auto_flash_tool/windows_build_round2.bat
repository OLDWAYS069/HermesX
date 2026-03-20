@echo off
setlocal
chcp 65001 >nul

set "ROOT=%~dp0.."
set "VENV=%ROOT%\.venv"
set "PYTHON=%VENV%\Scripts\python.exe"
set "ACTIVATE=%VENV%\Scripts\activate.bat"
set "BUILD_PS1=%ROOT%\tooling\auto_flash_builder\platform\windows\build.ps1"
set "OUT_EXE=%ROOT%\auto_flash_tool\tool_windows\Meshtastic_Auto_Flash.exe"

echo ============================================
echo HermesX AutoFlasher Windows Build Round 2
echo ============================================
echo ROOT=%ROOT%
echo VENV=%VENV%
echo BUILD_PS1=%BUILD_PS1%
echo.

echo [STEP 1] Check py launcher
echo --------------------------------------------
py -0p
echo.

echo [STEP 2] Recreate clean Python 3.13 venv
echo --------------------------------------------
if exist "%VENV%" (
  rmdir /s /q "%VENV%"
)
py -3.13 -m venv "%VENV%"
if errorlevel 1 goto END
echo.

echo [STEP 3] Activate venv
echo --------------------------------------------
call "%ACTIVATE%"
if errorlevel 1 goto END
echo.

echo [STEP 4] Verify Python executable
echo --------------------------------------------
python -c "import sys; print(sys.executable)"
python -V
echo.

echo [STEP 5] Install build dependencies
echo --------------------------------------------
python -m pip install --upgrade pip
if errorlevel 1 goto END
python -m pip install pyinstaller meshtastic esptool pyserial pyyaml
if errorlevel 1 goto END
echo.

echo [STEP 6] Build Windows executable
echo --------------------------------------------
powershell -ExecutionPolicy Bypass -File "%BUILD_PS1%" -PythonExe "%PYTHON%"
echo BUILD_EXIT_CODE=%ERRORLEVEL%
echo.

echo [STEP 7] Test built executable help
echo --------------------------------------------
if exist "%OUT_EXE%" (
  "%OUT_EXE%" --help
  echo EXE_HELP_EXIT_CODE=%ERRORLEVEL%
) else (
  echo [ERROR] Built EXE not found:
  echo %OUT_EXE%
)
echo.

:END
echo ============================================
echo Copy everything in this window and send it back.
echo Press any key to close.
pause >nul
