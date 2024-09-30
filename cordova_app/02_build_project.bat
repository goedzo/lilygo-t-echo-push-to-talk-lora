SET ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk
SET PATH=%PATH%;%ANDROID_HOME%\platform-tools;%ANDROID_HOME%\tools;%ANDROID_HOME%\tools\bin
cd PTTLora
call cordova build android
echo open android studio project to test --- cordova_app\PTTLora\platforms\android
pause