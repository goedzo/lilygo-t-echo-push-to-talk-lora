@echo off
setlocal EnableDelayedExpansion

set "ARDUINO_CLI=D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set BOARD=adafruit:nrf52:feather52840
set PORT=%1
set "BOARDLIST_TMP=%TEMP%\t-echo_boardlist.tmp"
set "UPLOAD_COM_PORT="

REM Check for --monitor-only flag (no upload, just monitor serial output)
set MONITOR_ONLY=
if /I "%PORT%"=="--monitor-only" set MONITOR_ONLY=1

if not exist "%ARDUINO_CLI%" (
    echo ERROR: arduino-cli not found at %ARDUINO_CLI%
    pause
    exit /b 1
)

REM Find firmware binary (Arduino CLI for nRF52 produces .elf/.hex)
set FW_FILE=
for %%F in (.pio\t-echo-build\main.bin .pio\t-echo-build\main.uf2 .pio\t-echo-build\t-echo.elf .pio\t-echo-build\t-echo.bin .pio\t-echo-build\t-echo.uf2 .pio\t-echo-build\main.ino.elf .pio\t-echo-build\main.ino.hex .pio\t-echo-build\main.ino.bin) do (
    if exist "%%F" set FW_FILE=%%F
)

if "!FW_FILE!"=="" (
    echo ERROR: No firmware binary found. Run build first.
    echo   ^.\01_build_firmware.bat
    pause
    exit /b 1
)

echo ============================================
echo   Upload to LilyGO T-Echo
echo ============================================
echo.
echo The T-Echo uses MCU-BOOT serial DFU which accepts firmware
echo over its running COM port (no USB drive mode needed).
echo.

if "!MONITOR_ONLY!"=="1" (
    set PORT=
)

if "!PORT!"=="" (
    echo Scanning for connected T-ECHO boards...
    echo.
    
    REM Save board list to temp file (avoids piping issues with spaces in paths)
    "%ARDUINO_CLI%" board list --log-level info 2>nul > "%BOARDLIST_TMP%"
    
    findstr "feather52840" "%BOARDLIST_TMP%" >nul 2>&1
    if !errorlevel! neq 0 (
        del "%BOARDLIST_TMP%" 2>nul
        echo ERROR: No nRF52 board detected.
        echo.
        echo Possible causes:
        echo   - T-Echo not connected
        echo   - Device driver not installed
        echo   - Double-click the reset button to enter DFU mode
        pause
        exit /b 1
    )

    REM Extract COM port from board list (first whitespace-delimited token)
    for /f "tokens=1 delims= " %%C in ('findstr "feather52840" "%BOARDLIST_TMP%"') do (
        set CANDIDATE=%%C
        echo !CANDIDATE! | findstr /I "COM[0-9]*" >nul 2>&1
        if !errorlevel! equ 0 (
            set PORT=!CANDIDATE!
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
)

:port_found
echo Found T-Echo on !PORT!
echo.

if "!MONITOR_ONLY!"=="1" (
    echo [SKIP] Upload skipped (--monitor-only mode)
    echo.
    set UPLOAD_COM_PORT=!PORT!
    goto :serial_monitor
)

REM ---- Upload firmware ----
echo [1/4] Uploading firmware...
set "UPLOAD_COM_PORT=!PORT!"
set "UPLOAD_OUTPUT_FILE=%TEMP%\t-echo_upload_out.txt"
call "%ARDUINO_CLI%" upload -b %BOARD% --port !PORT! --input-file "!FW_FILE!" > "!UPLOAD_OUTPUT_FILE!" 2>&1

REM Extract the new upload port that arduino-cli reports after DFU
for /f "tokens=1,2,3,* delims=: " %%A in ('findstr /I "New upload port" "!UPLOAD_OUTPUT_FILE!"') do if not "%%D"=="" (
    set UPLOAD_COM_PORT=%%D
)

REM Trim whitespace from UPLOAD_COM_PORT (e.g. "COM3 (serial)" -> "COM3")
for /f "tokens=1 delims=( " %%A in ("!UPLOAD_COM_PORT!") do set UPLOAD_COM_PORT=%%A

del "!UPLOAD_OUTPUT_FILE!" 2>nul

if "!UPLOAD_COM_PORT!"=="" set UPLOAD_COM_PORT=!PORT!

echo [OK] Upload complete (device is on !UPLOAD_COM_PORT!)
echo.

REM After DFU, the bootloader is still running on COM4 (no Serial output).
REM Press the T-Echo reset button to start the app firmware which sends messages.
if "!MONITOR_ONLY!"=="1" goto :serial_monitor

:serial_monitor

REM ---- Serial monitor: read messages for 120 seconds ----
echo [2/4] Reading serial output for 120 seconds...
echo --------------------------------------------

REM Wait longer for device to fully re-enumerate after DFU + app boot
echo   Waiting for device to re-initialize (USB re-enumeration + app boot)...
start /b /wait powershell -Command "Start-Sleep -Seconds 15"

REM Detect all COM ports on this system via registry (more reliable than wmic)
set "ALL_PORTS="
for /f "tokens=1,2,*" %%A in ('reg query "HKLM\HARDWARE\DEVICEMAP\SERIALCOMM" 2^>nul') do (
    if /I "%%B"=="!UPLOAD_COM_PORT!" set ALL_PORTS=%%C
)

REM If registry didn't find it, use the detected port directly
if "!ALL_PORTS!"=="" set ALL_PORTS=!UPLOAD_COM_PORT!

REM Also add any additional COM ports from wmic (the DFU bootloader may have opened a different one)
for /f "tokens=*" %%C in ('wmic path Win32_SerialPort get DeviceID 2^>nul ^| findstr /I "COM[0-9]*"') do (
    if not "%%C"=="" (
        echo !ALL_PORTS! | findstr /I "!%%C!" >nul 2>&1 || set ALL_PORTS=!ALL_PORTS! %%C
    )
)

if "!ALL_PORTS!"=="" set ALL_PORTS=COM3 COM4 COM5 COM6 COM7 COM8

echo   Monitoring: !ALL_PORTS!
echo.

REM Write ports to a temp text file (one per line) to avoid batch quoting issues
set PORTSLIST=%TEMP%\t-echo_ports.txt
(echo !ALL_PORTS!) > "!PORTSLIST!"

REM Run the serial monitor PowerShell script
start /b /wait powershell -ExecutionPolicy Bypass -Command "$p = Get-Content '!PORTSLIST!' | where { $_.Trim().Length -gt 0 }; & 'build_scripts\t-echo_monitor.ps1' -Ports $p -DurationSeconds 120"
del "!PORTSLIST!" 2>nul

echo --------------------------------------------
echo.
echo [3/4] Done -- collected 120 seconds of serial output above.
echo.
pause
