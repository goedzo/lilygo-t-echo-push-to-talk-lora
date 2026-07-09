@echo off
SET ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk
SET PATH=%PATH%;%ANDROID_HOME%\platform-tools;%ANDROID_HOME%\tools;%ANDROID_HOME%\tools\bin

REM === Pre-build JS syntax check ===
echo [prebuild] Checking JS files for syntax errors...
cd PTTLora
node --check "www\js\index.js" || (echo [prebuild] ERROR: index.js has syntax error && cd .. & exit /b 1)
node --check "www\js\OpusEncoder.js" || (echo [prebuild] ERROR: OpusEncoder.js has syntax error && cd .. & exit /b 1)
echo [prebuild] JS syntax check passed.
REM ====================================

call cordova build android
del ..\pttlora.apk
copy platforms\android\app\build\outputs\apk\debug\app-debug.apk ..\pttlora.apk
echo open android studio project to test --- cordova_app\PTTLora\platforms\android