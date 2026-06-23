@echo off
REM ============================================
REM   T-Echo Firmware - Build with Arduino CLI
REM   No PlatformIO — uses arduino-cli from Arduino IDE installation
REM ============================================
setlocal EnableDelayedExpansion

set "ARDUINO_CLI=D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set "BOARD=adafruit:nrf52:feather52840"
set "BUILD_DIR=.pio\t-echo-build"
set "SKETCH_DIR=main"

echo ============================================
echo   LilyGO T-Echo - Arduino CLI Build
echo ============================================
echo.

REM ---- Check arduino-cli exists ----
if not exist "%ARDUINO_CLI%" (
    echo ERROR: arduino-cli not found at %ARDUINO_CLI%
    echo Install Arduino IDE or download arduino-cli from: https://github.com/arduino/arduino-cli/releases
    pause
    exit /b 1
)

"%ARDUINO_CLI%" version
echo.

REM ---- Ensure board core is installed ----
echo [1/3] Ensuring Adafruit nRF52 core...
call "%ARDUINO_CLI%" core update-index --log-level error 2>nul
call "%ARDUINO_CLI%" core install %BOARD%@1.7.0 --log-level error 2>nul
echo   OK: Core verified
echo.

REM ---- Install libraries (from Library Manager) ----
echo [2/3] Installing required libraries from Library Manager...
set "LIBS=GxEPD2 GxEPD AceButton Codec2 TinyGPSPlus PCF8563_Library SdFat Adafruit_GFX_Library Adafruit_BusIO Adafruit_Sensor Adafruit_EPD SerialFlash Button2 SoftSPI SoftSPIB MPU9250"
for %%L in (%LIBS%) do (
    call "%ARDUINO_CLI%" lib install "%%L" --log-level error 2>nul
)
REM Pin RadioLib to 6.6.0 — 7.x removed getIrqStatus() from SX126x
call "%ARDUINO_CLI%" lib uninstall "RadioLib" --log-level error 2>nul
call "%ARDUINO_CLI%" lib install "RadioLib@6.6.0" --log-level error 2>nul
echo   OK: Libraries verified
echo.

REM ---- Compile firmware ----
echo [3/3] Building firmware...
rd /s /q %BUILD_DIR% 2>nul
if exist "%BUILD_DIR%" (
    echo [WARN] Build dir still locked (T-Echo USB active), waiting 5s ...
ping -n 6 127.0.0.1 >nul
rd /s /q %BUILD_DIR% 2>nul
)
call "%ARDUINO_CLI%" compile -b %BOARD% --build-path %BUILD_DIR% %SKETCH_DIR% 2>&1

if !errorlevel! neq 0 (
    echo.
    echo ============================================
    echo   BUILD FAILED
    echo ============================================
    echo.
    echo Troubleshooting:
    echo   1. Libraries must be installed via Library Manager:
    echo      arduino-cli lib install "Library Name"
    echo   2. Or copy from libraries/ to %USERPROFILE%\Arduino\libraries\
    pause
    exit /b !errorlevel!
)

REM Find the compiled binary (Arduino CLI for nRF52 produces .elf/.hex)
set FW_FILE=
for %%F in (%BUILD_DIR%\%SKETCH_DIR%.bin %BUILD_DIR%\%SKETCH_DIR%.uf2 %BUILD_DIR%\%SKETCH_DIR%.elf %BUILD_DIR%\%SKETCH_DIR%.bin %BUILD_DIR%\main.ino.elf %BUILD_DIR%\main.bin) do (
    if exist "%%F" set FW_FILE=%%F
)
if "!FW_FILE!"=="" (
    echo ERROR: No firmware output found
    pause
    exit /b 1
)

echo.
echo ============================================
echo   BUILD SUCCESSFUL
echo ============================================
echo.
echo Firmware binary: !FW_FILE!
echo.
echo To upload to T-Echo:
echo   1. Double-click the reset button on T-Echo (DFU mode)
echo   2. Run: .\02_upload_firmware.bat [COM port]
echo.
pause
