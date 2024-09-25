#include "display.h"
#include "utility.h"

// Initialize the display (adjust as per your display type)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

void initDisplay() {
  u8g2.begin();
}

// Display function to update the screen, showing name first, then properties
void displayInfo(const String& deviceName, const String& deviceAddr, const String& rawCod, const String& deviceClass, const char* incomingChar) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);  // Choose a font

  if (!deviceName.isEmpty() && !deviceAddr.isEmpty()) {
    u8g2.setCursor(0, 10);
    u8g2.print("Name: ");
    u8g2.print(deviceName.length() > 25 ? deviceName.substring(0, 25) : deviceName);

    u8g2.setCursor(0, 20);
    u8g2.print("Addr: ");
    u8g2.print(deviceAddr);

    u8g2.setCursor(0, 30);
    u8g2.print("Raw CoD: ");
    u8g2.print(rawCod);

    u8g2.setCursor(0, 40);
    u8g2.print("Class: ");
    u8g2.print(deviceClass.length() > 25 ? deviceClass.substring(0, 25) : deviceClass);
  } else if (incomingChar) {
    u8g2.setCursor(0, 10);
    u8g2.print("Received Key: ");
    u8g2.print(incomingChar);
  } else {
    u8g2.setCursor(0, 10);
    u8g2.print("Scanning...");
  }

  u8g2.sendBuffer();  // Transfer internal memory to the display
}
