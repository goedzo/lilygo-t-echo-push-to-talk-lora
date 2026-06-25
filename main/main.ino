#include "utilities.h"
#include <SPI.h>
#include <Wire.h>

#define BOOT_LOG(step) do { \
    SerialMon.print(F("[BOOT] === ")); \
    SerialMon.print(step); \
    unsigned long _s = millis(); \
} while(0)
#define BOOT_LOG_DONE(step) do { \
    SerialMon.print(F(") [")); \
    SerialMon.print(millis() - _s); \
    SerialMon.println(F("ms) ")); \
} while(0)

#include <GxEPD.h>
//#include <GxGDEP015OC1/GxGDEP015OC1.h>    // 1.54" b/w
//#include <GxGDEH0154D67/GxGDEH0154D67.h>  // 1.54" b/w
#include <GxDEPG0150BN/GxDEPG0150BN.h>  // 1.54" b/w 

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>


#include GxEPD_BitmapExamples
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
bool probeSX1262_direct();

// ---- HardFault / crash detection ----
volatile uint32_t fault_cfsr = 0;
volatile uint32_t fault_bfar = 0;
volatile uint32_t fault_lr = 0;
bool fatal_crash_detected = false;
volatile uint32_t crashCount = 0x5A5A5A5A;  // survives reset in RAM (BSS preserved by NVIC_SystemReset)

extern "C" void HardFault_Handler(void) {
    // Save state first (these are safe — direct register reads)
    fault_cfsr = SCB->CFSR;
    fault_bfar = SCB->BFAR;
    
    uint32_t lr;
    __asm volatile("mov %0, lr" : "=r"(lr));
    fault_lr = lr;
    
    fatal_crash_detected = true;
    crashCount++;  // track how many crashes in a row
    
    // Clear pending faults before resetting
    SCB->CFSR = 0xFFFFFFFF;
    SCB->HFSR = 0xFFFFFFFF;
    
    // Immediately trigger system reset — NO Serial calls here (they can fault)
    // Use the reset register which is safe to call from any context
    NVIC_SystemReset();
}

void checkCrashState() {
    if (fatal_crash_detected) {
        SerialMon.println("\n\n*** FIRMWARE CRASHED ***");
        SerialMon.print("  HardFault LR=0x"); SerialMon.println(fault_lr, HEX);
        SerialMon.print("  CFSR=0x"); SerialMon.println(fault_cfsr, HEX);
        if (fault_cfsr & 0x8000) { // MMARVALID
            SerialMon.print("  BFAR=0x"); SerialMon.println(fault_bfar, HEX);
        }
        if (fault_cfsr & 0x07) { // IACCVIOL
            SerialMon.println("  -> Illegal Instruction Access Violation");
        }
        if (fault_cfsr & 0x70) { // IACCVIOL
            SerialMon.println("  -> Unmapped / forbidden instruction access");
        }
        if (fault_cfsr & 0x200) { // DACPLB
            SerialMon.println("  -> Data PLB address fault");
        }
        if (fault_cfsr & 0x8000) { // MMARVALID
            SerialMon.print("  -> Faulting data address: 0x"); SerialMon.println(fault_bfar, HEX);
        }
        fatal_crash_detected = false;
    }
}

uint32_t        blinkMillis = 0;
uint8_t rgb = 0;
int count=0;

#define DB(x) do { SerialMon.print(F("[DB] line ")); SerialMon.println(__LINE__); x; } while(0)

// Stack guard pattern — write known value at stack bottom to detect overflow
void initStackGuard(uint32_t* guard, uint32_t pattern) {
    *guard = pattern;
}

bool checkStackGuard(uint32_t* guard, uint32_t pattern) {
    return (*guard == pattern);
}

void setup()
{
    Serial.begin(115200);
    
    // Crash-proof safe mode flag — BSS persists across NVIC_SystemReset
    static bool safe_mode_active = false;
    
    if (safe_mode_active) {
        // Any init step caused a crash — enter safe mode, skip everything
        SerialMon.println("\n\n=== SAFE MODE ===");
        SerialMon.println("Init failed. Hardware is resetting too fast.");
        
        // Blink Green LED = alive in safe mode
        pinMode(GreenLed_Pin, OUTPUT);
        pinMode(RedLed_Pin, OUTPUT);
        pinMode(BlueLed_Pin, OUTPUT);
        for (;;) {
            digitalWrite(GreenLed_Pin, HIGH);
            delay(500);
            digitalWrite(GreenLed_Pin, LOW);
            delay(500);
        }
    }
    
    // Log crash count on boot (RAM preserved across NVIC_SystemReset)
    if (crashCount != 0x5A5A5A5A) {
        SerialMon.print(F("[BOOT] Crash reboot #"));
        SerialMon.println(crashCount);
        
        // Dump RTC RAM crash info from last fault (if available)
        #define CRASH_MAGIC 0x43524153
        typedef struct { uint32_t magic; uint32_t lr_at_crash; uint32_t cfsr; uint32_t bfar; char function[16]; uint32_t line_number; } CrashRecord;
        CrashRecord* g_crash = (CrashRecord*)0x20007FC0;  // RTC RAM crash area
        
        if (g_crash->magic == CRASH_MAGIC) {
            SerialMon.println("\n--- Last crash from RTC RAM ---");
            SerialMon.print(F("  LR=0x"));   SerialMon.println(g_crash->lr_at_crash, HEX);
            SerialMon.print(F("  CFSR=0x"));  SerialMon.println(g_crash->cfsr, HEX);
            SerialMon.print(F("  BFAR=0x"));  SerialMon.println(g_crash->bfar, HEX);
            SerialMon.print(F("  Func: "));   SerialMon.println(g_crash->function);
            SerialMon.print(F("  Line: "));   SerialMon.println(g_crash->line_number);
            SerialMon.println("---");
        }
    }
    
    // Drain USB buffer before blocking operations
    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    SerialMon.print("[BOOT] begin Serial at 115200 baud\n");
    
    delay(200);
    SerialMon.print("[BOOT] delay done, starting boardInit()\n");
    boardInit();
    
    checkCrashState();
    DB("after boardInit");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    SerialMon.print("[BOOT] boardInit() done\n");
    
    unsigned long bootStart = millis();
    
    // Step 0a: check stack pointer before heavy init
    uint32_t sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    SerialMon.print("[BOOT] MSP=0x"); SerialMon.println(sp, HEX);
    DB("MSP read");

    updDisp(4, "Booting...");
    checkCrashState();
    DB("after first updDisp");

    // ---------- Frequency Map ----------
    static uint32_t crash_step = 0;  // BSS persists: survives crash resets
    unsigned long step1Start = millis();
    SerialMon.print("[BOOT] === Step 1/7: initializeFrequencyMap() (");
    updDisp(5, "Init frequencymap");
    checkCrashState();
    DB("about to call initializeFrequencyMap");
    
    crash_step = 1;  // We got here — step 1 is safe so far
    
    initializeFrequencyMap();
    checkCrashState();
    DB("after initializeFrequencyMap");

    SerialMon.print(F(") took "));
    SerialMon.print(millis() - step1Start);
    SerialMon.println("ms");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- LoRa ----------
    unsigned long step2Start = millis();
    SerialMon.print("[BOOT] === Step 2/7: setupLoRa() (");
    checkCrashState();
    DB("about to call setupLoRa");
    
    crash_step = 2;
    
    bool lora_ok = setupLoRa();
    checkCrashState();
    DB("after setupLoRa");

    SerialMon.print(F(") took "));
    SerialMon.print(millis() - step2Start);
    SerialMon.print(F("ms, returned: "));
    SerialMon.println(lora_ok ? "OK" : "FAIL");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- Settings (RTC) ----------
    unsigned long step3Start = millis();
    crash_step = 3;
    SerialMon.print("[BOOT] === Step 3/7: setupSettings() (");
    checkCrashState();
    DB("about to call setupSettings");
    
    setupSettings();
    checkCrashState();
    DB("after setupSettings");

    SerialMon.print(F(") took "));
    SerialMon.print(millis() - step3Start);
    SerialMon.println("ms");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- GPS ----------
    unsigned long step4Start = millis();
    crash_step = 4;
    SerialMon.print("[BOOT] === Step 4/7: setupGPS() (");
    checkCrashState();
    DB("about to call setupGPS");
    
    bool gps_ok = setupGPS();
    checkCrashState();
    DB("after setupGPS");

    SerialMon.print(F(") took "));
    SerialMon.print(millis() - step4Start);
    SerialMon.print(F("ms, returned: "));
    SerialMon.println(gps_ok ? "OK" : "FAIL");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- App Modes ----------
    unsigned long step5Start = millis();
    crash_step = 5;
    SerialMon.print("[BOOT] === Step 5/7: setupAppModes() (");
    checkCrashState();
    DB("about to call setupAppModes");
    
    setupAppModes();
    checkCrashState();
    DB("after setupAppModes");

    SerialMon.print(F(") took "));
    SerialMon.print(millis() - step5Start);
    SerialMon.println("ms");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- BLE ----------
    unsigned long step6Start = millis();
    crash_step = 6;
    SerialMon.print("[BOOT] === Step 6/7: setupBLE() (");
    checkCrashState();
    DB("about to call setupBLE");
    
    setupBLE();
    checkCrashState();
    DB("after setupBLE");

    SerialMon.print(F(") took "));
    SerialMon.print(millis() - step6Start);
    SerialMon.println("ms");

    while (Serial.available()) Serial.read();
    delay(10);
    while (Serial.available()) Serial.read();

    // ---------- Done ----------
    crash_step = 7;  // All init steps completed successfully
    SerialMon.println("[BOOT] === All init steps complete ===");
    SerialMon.print(F("[BOOT] Total boot time: "));
    SerialMon.print(millis() - bootStart);
    SerialMon.println(F("ms"));
    updDisp(5, "Setup ok!");

    checkCrashState();
    DB("about to clearScreen");

    clearScreen();
    checkCrashState();
    DB("after clearScreen");

    updModeAndChannelDisplay();
    checkCrashState();
    DB("after updModeAndChannelDisplay");
    
    SerialMon.println();
    SerialMon.print("[BOOT] ========================================\n");
    SerialMon.print("[BOOT] Boot sequence finished - entering loop()\n");
    SerialMon.print("[BOOT] ========================================\n\n");
}

void loop()
{
    checkCrashState();
    
    handleAppModes();
    handleBLE();  // BLE handling + drain notification queue
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

bool probeSX1262_direct() {
    bool found = false;
    
    pinMode(LoRa_Cs, OUTPUT);
    digitalWrite(LoRa_Cs, HIGH);
    
    SerialMon.println("  LoRa_CS pin configured (HIGH)");
    
    for (int attempt = 0; attempt < 5; attempt++) {
        // Toggle reset
        pinMode(LoRa_Rst, OUTPUT);
        digitalWrite(LoRa_Rst, LOW);
        delay(10);
        digitalWrite(LoRa_Rst, HIGH);
        delay(10);
        
        SerialMon.print("  Attempt ");
        SerialMon.print(attempt + 1);
        SerialMon.println(": probing SPI...");
        
        // Read busy state
        pinMode(LoRa_Busy, INPUT);
        int busy = digitalRead(LoRa_Busy);
        SerialMon.print("    Busy pin: ");
        SerialMon.println(busy ? "HIGH" : "LOW");
        
        // Create SPI port for LoRa
        SPIClass *loraSPI = new SPIClass(NRF_SPIM3, LoRa_Miso, LoRa_Sclk, LoRa_Mosi);
        loraSPI->begin();
        
        pinMode(LoRa_Cs, OUTPUT);
        digitalWrite(LoRa_Cs, HIGH);
        delay(1);
        
        // Manual SPI read of version register 0x0320 (16 bytes)
        uint8_t cmd = 0x1D;  // READ_REGISTER command
        uint32_t regAddr = 0x0320;  // Version string register
        uint8_t addrBytes[3] = {
            (uint8_t)((regAddr >> 8) & 0xFF),
            (uint8_t)(regAddr & 0xFF),
            0x00  // dummy byte for read
        };
        
        digitalWrite(LoRa_Cs, LOW);
        loraSPI->transfer(&cmd, 1);
        loraSPI->transfer(addrBytes, 3);
        
        uint8_t version[16];
        for (int i = 0; i < 16; i++) {
            version[i] = loraSPI->transfer(0x00);
        }
        
        digitalWrite(LoRa_Cs, HIGH);
        
        // Print raw hex
        SerialMon.print("    Version register: ");
        for (int i = 0; i < 16; i++) {
            if (version[i] < 0x10) SerialMon.print("0");
            SerialMon.print(version[i], HEX);
            SerialMon.print(" ");
        }
        SerialMon.println();
        
        // Check for SX1262 signature "SX126" at start of version string
        if (version[0] == 'S' && version[1] == 'X' && version[2] == '1' && 
            version[3] == '2' && version[4] == '6' && version[5] == '2') {
            found = true;
            SerialMon.println("    *** SX1262 FOUND! ***");
            break;
        }
        
        loraSPI->end();
        delete loraSPI;
        delay(10);
    }
    
    if (!found) {
        SerialMon.println("  SX1262 NOT found after 5 attempts");
    }
    
    return found;
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

    // T-Echo specific: configure NFC pins as regular GPIO inputs.
    // P0.8 (NFC1) and P0.9 (NFC2) share the CST816 touch controller I2C bus.
    // Left floating or in low-power mode they pull SDA/SCL down causing hard faults
    // during/after display init when SPI+I2C peripherals are active together.
    pinMode(NFC1_Pin, INPUT);
    pinMode(NFC2_Pin, INPUT);
    SerialMon.println("[Board] NFC pins P0.8/P0.9 configured as inputs");

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
