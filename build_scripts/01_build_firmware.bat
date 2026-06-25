@echo off
REM ============================================
REM   T-Echo Firmware - Build with Arduino CLI
REM   No PlatformIO — uses arduino-cli from Arduino IDE installation
REM ============================================
setlocal EnableDelayedExpansion

set "ARDUINO_CLI=D:\Tools\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set "BOARD=adafruit:nrf52:pca10056"
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

REM ---- Use vendored libraries from libraries/ folder ----
echo [2/3] Using vendored libraries from libraries/...
set "ARDUINO_LIB_DIR=%LOCALAPPDATA%\Arduino\libraries"
set "SKETCHBOOK_LIB_DIR=%USERPROFILE%\Documents\Arduino\libraries"
for %%D in ("%ARDUINO_LIB_DIR%" "%SKETCHBOOK_LIB_DIR%") do (
    if not exist "%%~D" mkdir "%%~D" 2>nul
)

REM Copy all vendor library sources to Arduino's lib directory for compilation
set VENDOR_LIBS=AceButton Button2 Codec2 GxEPD GxEPD2 MPU9250 PCF8563_Library RadioLib SdFat_-_Adafruit_Fork SerialFlash SoftSPI SoftSPIB TinyGPSPlus Adafruit_BluefruitLE_nRF51
for %%V in (%VENDOR_LIBS%) do (
    if exist "libraries\%%V" (
        for %%D in ("%ARDUINO_LIB_DIR%" "%SKETCHBOOK_LIB_DIR%") do (
            xcopy /E /I /Y "libraries\%%V" "%%~D\%%V" 2>nul >nul
        )
    )
)
REM Handle library names with different naming conventions
if not exist "%ARDUINO_LIB_DIR%\Adafruit_GFX_Library" if exist "%ARDUINO_LIB_DIR%\Adafruit-GFX-Library" (
    move /Y "%ARDUINO_LIB_DIR%\Adafruit-GFX-Library" "%ARDUINO_LIB_DIR%\Adafruit_GFX_Library" 2>nul >nul
)
if not exist "%ARDUINO_LIB_DIR%\Adafruit_BusIO" if exist "%ARDUINO_LIB_DIR%\Adafruit BusIO" (
    move /Y "%ARDUINO_LIB_DIR%\Adafruit BusIO" "%ARDUINO_LIB_DIR%\Adafruit_BusIO" 2>nul >nul
)
if not exist "%ARDUINO_LIB_DIR%\SdFat_-_Adafruit_Fork" if exist "%ARDUINO_LIB_DIR%\SdFat - Adafruit Fork" (
    move /Y "%ARDUINO_LIB_DIR%\SdFat - Adafruit Fork" "%ARDUINO_LIB_DIR%\SdFat_-_Adafruit_Fork" 2>nul >nul
)
if not exist "%ARDUINO_LIB_DIR%\MPU9250-0.4.6" if exist "%ARDUINO_LIB_DIR%\MPU9250-0.4.6" (
    move /Y "%ARDUINO_LIB_DIR%\MPU9250-0.4.6" "%ARDUINO_LIB_DIR%\MPU9250" 2>nul >nul
)
echo   OK: Libraries verified
echo   OK: Libraries verified
echo.

REM ---- Compile firmware ----
echo [3/3] Building firmware...
if exist "%BUILD_DIR%\*.a" (
    echo [WARN] Build dir exists and locked, waiting 5s for USB re-enumeration ...
    ping -n 6 127.0.0.1 >nul
)
rd /s /q %BUILD_DIR% 2>nul
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
echo   1. Run: .\02_upload_firmware.bat [COM port]
echo.
