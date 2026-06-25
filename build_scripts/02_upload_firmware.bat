@echo off
setlocal EnableDelayedExpansion

set "ARDUINO_CLI=D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set BOARD=adafruit:nrf52:feather52840
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

"%ARDUINO_CLI%" board list --log-level info 2>nul > "%BOARDLIST_TMP%"

findstr "feather52840" "%BOARDLIST_TMP%" >nul 2>&1
if !errorlevel! equ 0 goto :extract_port

del "%BOARDLIST_TMP%" 2>nul
echo ERROR: No nRF52 board detected.
echo.
echo Possible causes:
echo   - T-Echo not connected
echo   - Device driver not installed
echo   - Double-click the reset button to enter bootloader mode
pause
exit /b 1

:extract_port
for /f "tokens=1 delims= " %%C in ('findstr "feather52840" "%BOARDLIST_TMP%"') do (
    set CANDIDATE=%%C
    echo !CANDIDATE! | findstr /I "COM[0-9]*" >nul 2>&1
    if !errorlevel! equ 0 (
        set PORT=!CANDIDATE!
        del "%BOARDLIST_TMP%" 2>nul
        goto :port_found
    )
)
del "%BOARDLIST_TMP%" 2>nul

REM Fallback: wmic SerialPort lookup
for /f "tokens=*" %%C in ('wmic path Win32_SerialPort get DeviceID 2^>nul ^| findstr /I "COM[0-9]*"') do (
    if not "%%C"=="" set PORT=%%C
)

if "!PORT!"=="" (
    echo ERROR: Could not auto-detect COM port.
    echo Please specify manually: .\02_upload_firmware.bat COM3
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
REM ---- Standard USB Serial Upload ----
echo [1/3] Uploading firmware to !PORT!...
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

:serial_monitor
REM ---- Serial monitor: read messages for 240 seconds ----
echo [2/3] Reading serial output for 240 seconds...
echo --------------------------------------------
echo   Monitoring (auto-discovery) ...
echo.

start /b /wait powershell -ExecutionPolicy Bypass -Command "& 'build_scripts\t-echo_monitor.ps1' -Ports @('COM1','COM2','COM3','COM4','COM5','COM6','COM7','COM8','COM9','COM10','COM11','COM12') -DurationSeconds 240 -DiscoveryWaitSeconds 0"

echo --------------------------------------------
echo.
echo [3/3] Done -- collected 240 seconds of serial output above.
echo.
pause