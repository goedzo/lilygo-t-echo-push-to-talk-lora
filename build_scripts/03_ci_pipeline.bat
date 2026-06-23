@echo off
REM ============================================
REM   T-Echo Firmware - CI Pipeline (Build ^> Verify ^> Flash)
REM ============================================
setlocal EnableDelayedExpansion

set "ARDUINO_CLI=D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set "BOARD=adafruit:nrf52:feather52840"
set "BUILD_DIR=.pio\t-echo-build"
set "SKETCH_DIR=main"
set "FAILED=0"
set "BOARDLIST_TMP=%TEMP%\t-echo_boardlist.tmp"

echo ============================================
echo   T-Echo Firmware - CI Pipeline
echo ============================================
echo.

REM Step 1: Check tooling
if not exist "%ARDUINO_CLI%" (
    echo [FAIL] arduino-cli not found at %ARDUINO_CLI%
    exit /b 1
)
echo [OK] arduino-cli available
echo.

REM Step 2: Ensure core installed
echo [1/4] Ensuring Adafruit nRF52 core...
call "%ARDUINO_CLI%" core update-index --log-level error 2>nul
call "%ARDUINO_CLI%" core install %BOARD%@1.7.0 --log-level error 2>nul
if !errorlevel! neq 0 (
    echo [WARN] Core install failed, continuing...
)
echo [OK] Core verified
echo.

REM Step 3: Install libraries
echo [2/4] Installing libraries from Library Manager...
set "LIBS=GxEPD2 GxEPD AceButton Codec2 TinyGPSPlus PCF8563_Library SdFat Adafruit_GFX_Library Adafruit_BusIO Adafruit_Sensor Adafruit_EPD SerialFlash Button2 SoftSPI SoftSPIB MPU9250"
for %%L in (%LIBS%) do (
    call "%ARDUINO_CLI%" lib install "%%L" --log-level error 2>nul
)
REM Pin RadioLib to 6.6.0 — 7.x removed getIrqStatus() from SX126x
call "%ARDUINO_CLI%" lib uninstall "RadioLib" --log-level error 2>nul
call "%ARDUINO_CLI%" lib install "RadioLib@6.6.0" --log-level error 2>nul
echo [OK] Libraries verified
echo.

REM Step 4: Build firmware
echo [3/4] Building firmware...
rd /s /q %BUILD_DIR% 2>nul
call "%ARDUINO_CLI%" compile -b %BOARD% --build-path %BUILD_DIR% %SKETCH_DIR% 2>&1
if !errorlevel! neq 0 (
    echo [FAIL] Compilation failed. See errors above.
    set FAILED=1
    goto :summary
)

REM Find binary (Arduino CLI for nRF52 produces .elf/.hex)
set FW_FILE=
for %%F in (%BUILD_DIR%\%SKETCH_DIR%.bin %BUILD_DIR%\%SKETCH_DIR%.uf2 %BUILD_DIR%\%SKETCH_DIR%.elf %BUILD_DIR%\main.bin %BUILD_DIR%\main.ino.elf %BUILD_DIR%\main.ino.hex) do (
    if exist "%%F" set FW_FILE=%%F
)
if "!FW_FILE!"=="" (
    echo [FAIL] No firmware output found in %BUILD_DIR%
    set FAILED=1
    goto :summary
)

echo [OK] Build successful
echo     Binary: !FW_FILE!
echo.

REM Step 5: Detect upload target with auto-detection
echo [4/4] Checking for upload target...
"%ARDUINO_CLI%" board list --log-level info 2>nul > "%BOARDLIST_TMP%"
findstr "feather52840" "%BOARDLIST_TMP%" >nul 2>&1
if !errorlevel! equ 0 (
    REM Extract the detected COM port
    set DETECTED_PORT=
    for /f "tokens=1 delims= " %%C in ('findstr "feather52840" "%BOARDLIST_TMP%"') do (
        set CANDIDATE=%%C
        echo !CANDIDATE! | findstr /I "COM[0-9]*" >nul 2>&1
        if !errorlevel! equ 0 set DETECTED_PORT=!CANDIDATE!
    )
    if not "!DETECTED_PORT!"=="" (
        echo   T-Echo detected on !DETECTED_PORT!
        echo.
        echo To upload: build_scripts\02_upload_firmware.bat !DETECTED_PORT!
    ) else (
        echo   T-Echo detected.
        echo.
        echo To upload: build_scripts\02_upload_firmware.bat COM3
    )
) else (
    echo   No T-Echo detected via USB.
    echo   Connect your T-Echo and run: build_scripts\02_upload_firmware.bat COM3
)

del "%BOARDLIST_TMP%" 2>nul
echo.
goto :summary

:summary
if !FAILED! equ 0 (
    echo ============================================
    echo   ALL CHECKS PASSED
    echo ============================================
) else (
    echo ============================================
    echo   PIPELINE FAILED
    echo ============================================
)
exit /b !FAILED!
