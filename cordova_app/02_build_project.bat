SET ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk
SET PATH=%PATH%;%ANDROID_HOME%\platform-tools;%ANDROID_HOME%\tools;%ANDROID_HOME%\tools\bin
cd PTTLora
call cordova build android
del ..\pttlora.apk
copy platforms\android\app\build\outputs\apk\debug\app-debug.apk ..\pttlora.apk
echo open android studio project to test --- cordova_app\PTTLora\platforms\android
pause