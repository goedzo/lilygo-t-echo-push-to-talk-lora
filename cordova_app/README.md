# Cordova BLE App for Lilygo T-Echo

## Prerequisites:
- **Node.js**
- **Cordova**
- **Arduino IDE** with nRF52840 board support
- **Android SDK** (for building the Android platform in Cordova)
- **LilyGO T-Echo** device

---

### 1. **Installing Node.js and Cordova:**
Ensure that **Node.js** is installed on your system. You can download it from [nodejs.org](https://nodejs.org/).

1. Install Cordova globally using npm:
    ```bash
    npm install -g cordova
    ```

---

### 2. **Installing the Android SDK:**

If you don't already have the Android SDK installed, follow these steps to download and configure it:

1. **Download Android Studio** (includes Android SDK):
    - Go to [Android Studio](https://developer.android.com/studio) and download it.
    - During installation, select **Android SDK** and **Android SDK Platform Tools**.
    - Alternatively, you can download only the **command-line tools** if you don’t need Android Studio.

2. **Configure Environment Variables** (Windows):
    - Set up the `ANDROID_HOME` (or `ANDROID_SDK_ROOT`) environment variable:
        - Open **System Properties** → **Advanced** → **Environment Variables**.
        - Under **System variables**, click **New** and add:
            - **Variable name:** `ANDROID_HOME`
            - **Variable value:** `C:\Users\YourUsername\AppData\Local\Android\Sdk` (or wherever your SDK is installed).
        - Add the following to your `Path` under **System variables**:
            - `%ANDROID_HOME%\platform-tools`
            - `%ANDROID_HOME%\tools`
            - `%ANDROID_HOME%\tools\bin`

    On **macOS** and **Linux**, add these lines to your `~/.bashrc`, `~/.zshrc`, or equivalent shell configuration file:

    ```bash
    export ANDROID_HOME=$HOME/Library/Android/sdk
    export PATH=$PATH:$ANDROID_HOME/tools
    export PATH=$PATH:$ANDROID_HOME/platform-tools
    ```

3. **Verify Installation**:
    - Open a terminal or command prompt and verify the installation by running:
    ```bash
    adb --version
    ```

    If successful, this will display the installed version of `adb`.

---

### 3. **Setting up the Cordova App:**

1. **Run the setup script**: 
   If you're using the provided batch file, simply run `01_create_project.bat`.

    Alternatively, follow the manual steps below:

#### Manually creating the Cordova app:

1. Create a Cordova project:
    ```bash
    cordova create PTTLora com.example.PTTLora PTTLora
    cd PTTLora
    ```

2. Add the Android platform:
    ```bash
    cordova platform add android
    ```

3. Add the BLE plugin:
    ```bash
    cordova plugin add cordova-plugin-ble-central
    ```

---

### 4. **Building and Running the Cordova App:**

1. Build the Cordova app:
    ```bash
    cordova build android
    ```

2. Run the Cordova app on an Android device:
    ```bash
    cordova run android
    ```

Make sure you have an Android device connected to your system with **Developer Mode** and **USB Debugging** enabled.

---

### 5. **Communication:**

- The Cordova app will scan for the BLE device named `LilygoT-Echo-XXXXXXXX`.
- Once connected, the app can
