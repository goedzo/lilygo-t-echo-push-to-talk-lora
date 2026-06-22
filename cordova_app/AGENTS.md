# Agent Instructions — Companion App (Cordova Android)

## Purpose

Takes care of the Android companion app: `cordova_app/PTTLora/`. A Cordova-based Android app that scans for `LilygoT-Echo-XXXXXXXX` BLE devices and sends/receives LoRa messages. Output APK is copied to `cordova_app/pttlora.apk`.

## Ownership

- **Build scripts:** `cordova_app/01_create_project.bat`, `cordova_app/02_build_project.bat`
- **App source:** `cordova_app/PTTLora/` — Cordova project root
  - `PTTLora/www/index.html` — entry page
  - `PTTLora/www/js/index.js` — BLE + LoRa message logic
  - `PTTLora/www/css/index.css` — styling
  - `PTTLora/www/img/logo.png` — app icon
  - `PTTLora/config.xml` — Cordova config
  - `PTTLora/package.json` — npm dependencies

## Local Contracts

- Build requires running `01_create_project.bat` first (skip if PTTLora exists)
- Then run `02_build_project.bat` or `cordova build android`
- BLE plugin only (no other native features)
- Device discovery: scans for `LilygoT-Echo-XXXXXXXX` MAC-prefixed BLE devices

## Work Guidance

### Build process
1. Run `01_create_project.bat` to scaffold Cordova project (one-time)
2. Edit files under `PTTLora/www/` as needed
3. Run `02_build_project.bat` to produce APK
4. APK output: `cordova_app/pttlora.apk`

### Development notes
- Android-only target (cordova-android 13)
- BLE communication with the T-Echo uses custom GATT service UUIDs
- LoRa message relay between phone and device

## Verification

- `02_build_project.bat` must produce a non-empty `pttlora.apk`
- APK must install and pair with LilyGO T-Echo hardware over BLE
- No automated tests exist

## Child DOX Index

None yet. Create when subfolders acquire their own durable rules or workflows.
