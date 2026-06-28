@echo off
setlocal EnableDelayedExpansion

set "ARDUINO_CLI=D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set BOARD=adafruit:nrf52:pca10056
set PORT=%1
set "BOARDLIST_TMP=%TEMP%\t-echo_boardlist.tmp"
set "UPLOAD_COM_PORT="

REM Check for --monitor-only flag
set MONITOR_ONLY=
if /I "%PORT%"=="--monitor-only" set MONITOR_ONLY=1

if not exist "%ARDUINO_CLI%" (
    echo ERROR: arduino-cli not found at %ARDUINO_CLI%
    pause
    exit /b 1
)

REM Find firmware binary
set "FW_FILE="
set "FW_ELF="
set "FW_HEX="
set "FW_BIN="
for %%F in (.pio\t-echo-build\main.ino.elf .pio\t-echo-build\t-echo.elf .pio\t-echo-build\main.elf) do (
    if exist "%%F" set FW_ELF=%%F
)
for %%F in (.pio\t-echo-build\main.ino.hex .pio\t-echo-build\t-echo.hex .pio\t-echo-build\main.hex) do (
    if exist "%%F" set FW_HEX=%%F
)
for %%F in (.pio\t-echo-build\main.ino.bin .pio\t-echo-build\t-echo.bin .pio\t-echo-build\main.bin) do (
    if exist "%%F" set FW_BIN=%%F
)

REM Prefer hex first for standard serial DFU uploads, then bin, then elf
if "!FW_HEX!" neq "" set "FW_FILE=!FW_HEX!"
if "!FW_BIN!" neq "" if "!FW_FILE!"=="" set "FW_FILE=!FW_BIN!"
if "!FW_ELF!" neq "" if "!FW_FILE!"=="" set "FW_FILE=!FW_ELF!"

if "!FW_FILE!"=="" (
    echo ERROR: No firmware binary found. Run build first.
    echo   ^.\01_build_firmware.bat
    pause
    exit /b 1
)

echo ============================================
echo   Upload to LilyGO T-Echo (Standard USB Serial)
echo ============================================
echo.
echo Uploading firmware via standard USB Serial over COM port.
echo (If the upload fails to auto-reset, double-click the reset button).
echo.

if "!MONITOR_ONLY!"=="1" set PORT=

REM If port is already specified, skip auto-detection
if not "!PORT!"=="" goto :port_found

echo Scanning for connected T-ECHO boards...
echo.

REM T-Echo in normal mode does not appear as DFU device, so arduino-cli board list fails.
REM We use wmic to enumerate all COM ports and pick the first one — T-Echo is always the
REM only USB-serial device on a freshly-connected PC.
for /f "tokens=*" %%P in ('wmic path Win32_SerialPort get DeviceID 2^>nul ^| findstr /I "COM[0-9]*"') do (
    if not "%%P"=="" set PORT=%%P
)

if "!PORT!"=="" (
    echo ERROR: No serial port found.
    echo.
    echo Possible causes:
    echo   - T-Echo not connected via USB
    echo   - Device driver not installed
    pause
    exit /b 1
)

:port_found
echo Found T-Echo on !PORT!
echo.

REM Flattened monitor check to prevent parentheses from breaking the parser
if not "!MONITOR_ONLY!"=="1" goto :do_upload
echo [SKIP] Upload skipped (--monitor-only mode)
echo.
set UPLOAD_COM_PORT=!PORT!
goto :serial_monitor

:do_upload
REM ---- Pre-upload capture (captures pre-reset state, then restarts post-upload) ----
echo [1/4] Starting pre-upload serial capture...
set "UPLOAD_COM_PORT=!PORT!"

REM Run capture synchronously for the duration of upload (it will timeout after 300s if needed)
REM The capture captures everything until device reboots during upload
rem call powershell -ExecutionPolicy Bypass -Command "& { & '.\build_scripts\t-echo_capture_before_upload.ps1' -Port '!PORT!' }"

echo.

REM ---- Standard USB Serial Upload ----
echo [2/4] Uploading firmware to !PORT!...
set "UPLOAD_COM_PORT=!PORT!"

REM Standard adafruit nRF52 upload command in verbose mode (-v)
call "%ARDUINO_CLI%" upload -v -b %BOARD% --port !PORT! --input-file "!FW_FILE!"

if !errorlevel! neq 0 (
    echo.
    echo ERROR: Upload failed. 
    echo Please make sure the device is connected, or try double-clicking 
    echo the reset button to force bootloader mode manually.
    pause
    exit /b 1
)

echo [OK] Upload complete!
echo.

REM Wait for device to stabilize after DFU upload (nRF52840 reboot takes ~2-3s)
echo Waiting 5s for device to boot...
timeout /t 5 /nobreak >nul

REM Post-upload capture: listens for boot messages then starts formal monitor
echo [4/4] Capturing post-upload boot messages and monitoring (120s)...
start /b /wait powershell -ExecutionPolicy Bypass -Command "& 'build_scripts\t-echo_monitor.ps1' -Ports @('COM1','COM2','COM3','COM4','COM5','COM6','COM7','COM8','COM9','COM10','COM11','COM12') -DurationSeconds 120 -DiscoveryWaitSeconds 0"

echo --------------------------------------------
echo.
echo [DONE] Done -- collected complete boot + 120s serial output above.
echo.
