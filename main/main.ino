#include "utilities.h"
#include <SPI.h>
#include <Wire.h>

#include <GxEPD.h>
//#include <GxGDEP015OC1/GxGDEP015OC1.h>    // 1.54" b/w
//#include <GxGDEH0154D67/GxGDEH0154D67.h>  // 1.54" b/w
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w 

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>


#include GxEPD_BitmapExamples
#include <Fonts/FreeMonoBold12pt7b.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#include "display.h"
#include "gps.h"
#include "settings.h"
#include "app_modes.h"
#include "lora.h"
#include "audio.h"
#include "ble.h"

// GPS SoftwareSerial on P1.8 (TX) / P1.9 (RX)
SoftwareSerial SerialGPS(Gps_Tx_Pin, Gps_Rx_Pin);  // RX pin, TX pin



void configVDD(void);
void boardInit();

uint32_t        blinkMillis = 0;
uint8_t rgb = 0;
int count=0;

void setup()
{
    SerialMon.println();
    SerialMon.print("========================================\n");
    SerialMon.print("[BOOT] Starting firmware boot sequence\n");
    SerialMon.print("[BOOT] millis(): ");
    SerialMon.println(millis());
    SerialMon.print("========================================\n");

    SerialMon.print("[BOOT] begin Serial at 115200 baud\n");
    Serial.begin(115200);
    
    // Drain USB buffer before blocking operations
    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    SerialMon.print("[BOOT] Serial.begin() done\n");
    
    //while (!Serial);

    SerialMon.print("[BOOT] delay(200) ms\n");
    delay(200);
    SerialMon.print("[BOOT] delay done, starting boardInit()\n");
    boardInit();
    
    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    SerialMon.print("[BOOT] boardInit() done\n");
    
    updDisp(4, "Booting...");

    // ---------- Frequency Map ----------
    SerialMon.println("[BOOT] === Step 1/7: initializeFrequencyMap()");
    updDisp(5, "Init frequencymap");
    initializeFrequencyMap();
    SerialMon.println("[BOOT] initializeFrequencyMap() done");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- LoRa ----------
    SerialMon.println("[BOOT] === Step 2/7: setupLoRa()");
    updDisp(5, "Init lora...");
    bool lora_ok = setupLoRa();
    SerialMon.print("[BOOT] setupLoRa() returned: ");
    SerialMon.println(lora_ok ? "OK" : "FAIL");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- Settings (RTC) ----------
    SerialMon.println("[BOOT] === Step 3/7: setupSettings()");
    updDisp(5, "Init settings..");
    setupSettings();
    SerialMon.println("[BOOT] setupSettings() done");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- GPS ----------
    SerialMon.println("[BOOT] === Step 4/7: setupGPS()");
    updDisp(5, "Init gps...");
    bool gps_ok = setupGPS();
    SerialMon.print("[BOOT] setupGPS() returned: ");
    SerialMon.println(gps_ok ? "OK" : "FAIL");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- App Modes ----------
    SerialMon.println("[BOOT] === Step 5/7: setupAppModes()");
    updDisp(5, "Setup app modes");
    setupAppModes();
    SerialMon.println("[BOOT] setupAppModes() done");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- BLE ----------
    SerialMon.println("[BOOT] === Step 6/7: setupBLE()");
    updDisp(5, "Setup bluetooth");
    setupBLE();
    SerialMon.println("[BOOT] setupBLE() done");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- Done ----------
    SerialMon.println("[BOOT] === All init steps complete ===");
    updDisp(5, "Setup ok!");

    clearScreen();
    updModeAndChannelDisplay();
    
    SerialMon.println();
    SerialMon.print("[BOOT] ========================================\n");
    SerialMon.print("[BOOT] Boot sequence finished - entering loop()\n");
    SerialMon.print("[BOOT] ========================================\n\n");
}

void loop()
{
    handleAppModes();
    //handleBLE();  // BLE handling
    if (millis() - blinkMillis > 1000) {
        blinkMillis = millis();
        switch (rgb) {
        case 0:
            digitalWrite(GreenLed_Pin, LOW);
            digitalWrite(RedLed_Pin, HIGH);
            digitalWrite(BlueLed_Pin, HIGH);
            break;
        case 1:
            digitalWrite(GreenLed_Pin, HIGH);
            digitalWrite(RedLed_Pin, LOW);
            digitalWrite(BlueLed_Pin, HIGH);
            break;
        case 2:
            digitalWrite(GreenLed_Pin, HIGH);
            digitalWrite(RedLed_Pin, HIGH);
            digitalWrite(BlueLed_Pin, LOW);
            break;
        default :
            break;
        }
        rgb++;
        rgb %= 3;

        count++;

        if(count==600000) {
          count=0;
        }
        char buf[50];
        snprintf(buf, sizeof(buf), "Idlecount: %d", count);
        //updDisp(11, buf,true);
    }
}



void configVDD(void)
{
    // Configure UICR_REGOUT0 register only if it is set to default value.
    if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) ==
            (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos)) {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}

        NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
                            (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}

        // System reset is needed to update UICR registers.
        NVIC_SystemReset();
    }
}

void boardInit()
{
    uint8_t rlst = 0;

#ifdef HIGH_VOLTAGE
    configVDD();
#endif

    SerialMon.println("[Board] >>> boardInit() START");

    SerialMon.print("[Board] begin SerialMon at ");
    SerialMon.print(MONITOR_SPEED);
    SerialMon.println(" baud");
    SerialMon.begin(MONITOR_SPEED);
    
    // Drain USB buffer before blocking operations
    while (Serial.available()) Serial.read();
    delay(10);

    // delay(5000);
    // while (!SerialMon);
    SerialMon.println("[Board] Reset reason decode:");

    uint32_t reset_reason;
    sd_power_reset_reason_get(&reset_reason);
    SerialMon.print("  value=0x");
    SerialMon.println(reset_reason, HEX);
    
    if (reset_reason & 0x01) SerialMon.println("  -> Power-up");
    if (reset_reason & 0x02) SerialMon.println("  -> External pin");
    if (reset_reason & 0x04) SerialMon.println("  -> Watchdog");
    if (reset_reason & 0x08) SerialMon.println("  -> SVD");
    if (reset_reason & 0x10) SerialMon.println("  -> CPU lockup");
    if (reset_reason & 0x40) SerialMon.println("  -> LRWICTO from System OFF");
    if (reset_reason & 0x80) SerialMon.println("  -> CRITERIASE from System OFF");

    while (Serial.available()) Serial.read();
    delay(10);

    SerialMon.print("[Board] Power_Enable_Pin=");
    SerialMon.print(Power_Enable_Pin);
    SerialMon.println(", OUTPUT HIGH");
    pinMode(Power_Enable_Pin, OUTPUT);
    digitalWrite(Power_Enable_Pin, HIGH);

    pinMode(ePaper_Backlight, OUTPUT);
    enableBacklight(false);

    SerialMon.println("[Board] configuring LEDs and buttons ...");
    pinMode(GreenLed_Pin, OUTPUT);
    pinMode(RedLed_Pin, OUTPUT);
    pinMode(BlueLed_Pin, OUTPUT);

    pinMode(UserButton_Pin, INPUT_PULLUP);
    pinMode(Touch_Pin, INPUT_PULLUP);

    // Blink pattern to confirm board is alive
    SerialMon.print("[Board] LED blink pattern (10 cycles) ... ");
    int i = 10;
    while (i--) {
        digitalWrite(GreenLed_Pin, !digitalRead(GreenLed_Pin));
        digitalWrite(RedLed_Pin, !digitalRead(RedLed_Pin));
        digitalWrite(BlueLed_Pin, !digitalRead(BlueLed_Pin));
        delay(300);
    }
    digitalWrite(GreenLed_Pin, HIGH);
    digitalWrite(RedLed_Pin, HIGH);
    digitalWrite(BlueLed_Pin, HIGH);
    SerialMon.println("done");

    // Drain USB before display init (display takes a long time)
    while (Serial.available()) Serial.read();
    delay(10);

    SerialMon.println("[Board] calling setupDisplay() ...");
    setupDisplay();
    
    // Drain USB after display (all done printing during display init)
    while (Serial.available()) Serial.read();
    delay(10);

    SerialMon.println("[Board] <<< boardInit() DONE");
}