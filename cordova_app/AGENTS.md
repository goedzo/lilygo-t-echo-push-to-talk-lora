# Agent Instructions — Companion App (Cordova Android)

## Purpose

Takes care of the Android companion app: `cordova_app/PTTLora/`. A Cordova-based Android app that scans for `LilygoT-Echo-XXXXXXXX` BLE devices and sends/receives LoRa messages. Output APK is copied to `cordova_app/pttlora.apk`.

## Ownership

- **Build scripts:** `cordova_app/01_create_project.bat`, `cordova_app/02_build_project.bat`
- **App source:** `cordova_app/PTTLora/` — Cordova project root
  - `PTTLora/www/index.html` — entry page (mode selector with pills: RAW, TXT, RANGE, TST, PONG, SCAN, PTT; BEACON and WP are firmware-only modes not exposed in the app UI)
  - `PTTLora/www/js/index.js` — BLE + LoRa message logic. Sends `"SENDTXT:{message}"` for TXT mode, `switchMode()` sends mode name via BLE GATT. Device name filter: `/^LilygoT-Echo-[A-F0-9]{8}$/`. Mode pills in index.html map 1:1 to firmware modes. Named contacts: alias set via `SETNAME:{name}` GATT, buddy list sync via `GETBUDDY`/`SETBUDDY`, stored as CSV "CN{name}|DI{id},". Device settings panel: toggle via status bar gear button, fetches via `GETSETTINGS`, saves via `SETSETTINGS:KEY=val,...` (keyed format, partial updates supported).
  - `PTTLora/www/css/index.css` — styling
  - `PTTLora/www/img/logo.png` — app icon
  - `PTTLora/config.xml` — Cordova config
  - `PTTLora/package.json` — npm dependencies (`cordova-android ^13.0.0`, `cordova-plugin-ble-central ^1.7.8`)

## Local Contracts

- Build requires running `01_create_project.bat` first (skip if PTTLora exists)
- Then run `02_build_project.bat` or `cordova build android`
- BLE plugin only (no other native features)
- Device discovery: scans for `LilygoT-Echo-XXXXXXXX` MAC-prefixed BLE devices
- GATT service UUID: `"1235"`, characteristic UUID: `"ABCE"` (non-standard simple string identifiers)

## Work Guidance

### Build process
1. Run `01_create_project.bat` to scaffold Cordova project (one-time)
2. Edit files under `PTTLora/www/` as needed
3. Run `02_build_project.bat` to produce APK
4. APK output: `cordova_app/pttlora.apk`

### Development notes
- Android-only target (cordova-android 13)
- BLE communication with the T-Echo uses custom GATT service UUIDs (`"1235"` / `"ABCE"`)
- LoRa message relay between phone and device
- All firmware text notifications arrive wrapped as `LINE:NOTIF|DATA:{payload}~~` — the app strips the prefix before regex matching. Payloads use `~~` terminators for multi-message framing on a single notification.
- Device serial monitoring arrives as `LINE:SERIAL|DATA:{log message}` — the companion app displays these as italic gray log lines in the console tab, labeled with the device's short ID. Firmware uses `sendSerialToAppLn()` to send these (appends `\n` automatically). The format is `LINE:SERIAL|DATA:` prefix + raw text payload (no `SERIAL:` wrapper needed anymore).
- Device settings GATT actions:
  - `GETSETTINGS` — no value. Response: `OK{SETTINGS:SF=12,BITRATE=2,CHAN=A,VOL=5,BL=1,BW=250000,CR=6,FH=1,HOUR=14,MIN=32,SEC=0}`
  - `SETSETTINGS:{key=val,...}` — partial updates supported. Response: `OK{SETTINGS:saved}` or `ERR{SETTINGS:...}`
  - Only changed fields need to be included in SETSETTINGS (order-independent keyed format). Unknown keys are ignored by both sides for forward compatibility.

## Verification

- `02_build_project.bat` must produce a non-empty `pttlora.apk`
- APK must install and pair with LilyGO T-Echo hardware over BLE
- No automated tests exist

## Child DOX Index

None yet. Create when subfolders acquire their own durable rules or workflows.
