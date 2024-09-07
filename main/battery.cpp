// battery.cpp

#include "battery.h"
#include "utilities.h"  // Include your utilities.h to reuse pin definitions

#define VBAT_MV_PER_LSB   (0.73242188F)   // 3.0V ADC range and 12-bit ADC resolution = 3000mV/4096

#ifdef NRF52840_XXAA
#define VBAT_DIVIDER      (0.5F)          // 150K + 150K voltage divider on VBAT
#define VBAT_DIVIDER_COMP (2.0F)          // Compensation factor for the VBAT divider
#else
#define VBAT_DIVIDER      (0.71275837F)   // 2M + 0.806M voltage divider on VBAT
#define VBAT_DIVIDER_COMP (1.403F)        // Compensation factor for the VBAT divider
#endif

#define REAL_VBAT_MV_PER_LSB (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB)

uint32_t vbat_pin = Adc_Pin;  // Use Adc_Pin defined in utilities.h

float readVBAT(void) {
  float raw;

  // Set the analog reference to 3.0V (default = 3.6V)
  analogReference(AR_INTERNAL_3_0);

  // Set the resolution to 12-bit (0..4095)
  analogReadResolution(12); // Can be 8, 10, 12, or 14

  // Let the ADC settle
  delay(1);

  // Get the raw 12-bit, 0..3000mV ADC value
  raw = analogRead(vbat_pin);

  // Set the ADC back to the default settings
  analogReference(AR_DEFAULT);
  analogReadResolution(10);

  // Convert the raw value to compensated mv, taking the resistor-
  // divider into account (providing the actual LIPO voltage)
  return raw * REAL_VBAT_MV_PER_LSB;
}

uint8_t mvToPercent(float mvolts) {
  if(mvolts < 3300)
    return 0;

  if(mvolts < 3600) {
    mvolts -= 3300;
    return mvolts / 30;
  }

  mvolts -= 3600;
  return 10 + (mvolts * 0.15F);  // mvolts / 6.66666666
}

void checkBattery() {
  float vbat_mv = readVBAT();
  uint8_t vbat_per = mvToPercent(vbat_mv);

  SerialMon.print("Battery Voltage: ");
  SerialMon.print(vbat_mv);
  SerialMon.print(" mV (");
  SerialMon.print(vbat_per);
  SerialMon.println("%)");
}

// New function to return the battery percentage
uint8_t getBatteryPercentage() {
  float vbat_mv = readVBAT();
  return mvToPercent(vbat_mv);
}
