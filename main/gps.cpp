#include <TinyGPS++.h>
#include "utilities.h"  // Include pin definitions here
#include "settings.h"   // Include for 'time_set' and 'rtc' definitions
#include "gps.h"
#include "display.h"
#include "app_modes.h"

TinyGPSPlus     *gps;

uint32_t lastGPSUpdate;
GPSStatus gps_status = NO_GPS;  // Initialize the GPS status

// GPS-related extern variables
int gps_satellites = 0;          // Initialize the satellite count
double gps_latitude = 0;
double gps_longitude = 0;
float gps_altitude = 0;
unsigned long gps_location_age = 0;
unsigned long gps_date_age = 0;
uint32_t gps_date_value = 0;
unsigned long gps_time_age = 0;
uint32_t gps_time_value = 0;
float gps_speed_kmph = 0;
float gps_speed_mps = 0;
float gps_course = 0;
float gps_hdop = 0;

// External variables from settings.h
extern bool time_set;  // Ensure 'time_set' is declared in settings.h
//extern PCF8563_Class rtc;  // RTC instance from your RTC code

bool setupGPS() {
    SerialMon.println("[GPS] Initializing ... ");
    SerialMon.flush();

#ifndef PCA10056
    SerialGPS.setPins(Gps_Rx_Pin, Gps_Tx_Pin);  // Use pin definitions from utilities.h
#endif
    SerialGPS.begin(9600);
    SerialGPS.flush();

    pinMode(Gps_pps_Pin, INPUT);

    pinMode(Gps_Wakeup_Pin, OUTPUT);
    digitalWrite(Gps_Wakeup_Pin, HIGH);

    delay(10);
    pinMode(Gps_Reset_Pin, OUTPUT);
    digitalWrite(Gps_Reset_Pin, HIGH); delay(10);
    digitalWrite(Gps_Reset_Pin, LOW); delay(10);
    digitalWrite(Gps_Reset_Pin, HIGH);

    gps_status = GPS_INIT;  // GPS module has been initialized
    gps = new TinyGPSPlus();  // Create a new instance of TinyGPSPlus

    return true;
}

void loopGPS() {
    while (SerialGPS.available() > 0) {
        gps->encode(SerialGPS.read());
    }

    // Error check for insufficient GPS data
    if (gps->charsProcessed() < 10) {
        Serial.println(F("WARNING: No GPS data. Check wiring."));
        gps_status = GPS_ERROR;  // Set status to GPS_ERROR
        return;  // Exit the function to avoid further processing
    }

    // Only attempt to update RTC if GPS time has been updated and not already synced
    if (gps_satellites >= 5 && gps->time.isUpdated() && gps->date.isUpdated() && digitalRead(Gps_pps_Pin) == HIGH) {
        // Get the GPS time and date
        int gpsHour = gps->time.hour();
        int gpsMinute = gps->time.minute();
        int gpsSecond = gps->time.second();
        int gpsYear = gps->date.year();
        int gpsMonth = gps->date.month();
        int gpsDay = gps->date.day();
        //Only correct it when it's off

        RTC_Date currentTime = rtc.getDateTime();  // Get the current time from RTC

        if(currentTime.second != gpsSecond) {
            //Update time if we are longer then a minute out of synch
            rtc.setDateTime(gpsYear, gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond);
            time_set = true;
            //SerialMon.println("RTC set from GPS time");
        }
        if(gps_status==GPS_INIT) {
            gps_status = GPS_TIME;  // Set status to GPS_TIME since we found the time
        }
    }

    if (millis() - lastGPSUpdate > 5000) {
        if (gps->altitude.isUpdated()) {
            gps_altitude = gps->altitude.meters();
            //SerialMon.print(F("altitude: "));
            //SerialMon.println(gps_altitude);
        }

        if (gps->satellites.isUpdated()) {
            if(gps_satellites != gps->satellites.value() ) {
                //Something has changed, update the display
                printStatusIcons();
                display->displayWindow(0,0,disp_width,disp_height);
            }

            gps_satellites = gps->satellites.value();  // Update the satellite count
            //SerialMon.print(F("SATELLITES: "));
            //SerialMon.println(gps_satellites);
        }

        if (gps->date.isUpdated()) {
            gps_date_age = gps->date.age();
            gps_date_value = gps->date.value();
        }

        if (gps->time.isUpdated()) {
            gps_time_age = gps->time.age();
            gps_time_value = gps->time.value();
            //SerialMon.print(F("time: "));
            //SerialMon.println(gps_time_value);
        }

        if (gps->speed.isUpdated()) {
            gps_speed_kmph = gps->speed.kmph();
            gps_speed_mps = gps->speed.mps();
            //SerialMon.print(F("speed: "));
            //SerialMon.println(gps_speed_kmph);
        }

        if (gps->course.isUpdated()) {
            gps_course = gps->course.deg();
            //SerialMon.print(F("deg: "));
            //SerialMon.println(gps_course);
        }

        if (gps->hdop.isUpdated()) {
            gps_hdop = gps->hdop.value();
            //SerialMon.print(F("hdop: "));
            //SerialMon.println(gps_hdop);
        }

        if (gps->location.isUpdated()) {
            if(gps->location.isValid()) {
                if (gps->satellites.value() >= 4) {
                    // Location is more stable
                    if (gps->hdop.isValid() && gps->hdop.value() <= 100.0) {
                        // HDOP is within a good range, likely stable
                        if (gps->location.age() < 4000) {
                            // The fix is recent, likely stable
                            gps_latitude = gps->location.lat();
                            gps_longitude = gps->location.lng();
                            gps_location_age = gps->location.age();
                            gps_status = GPS_LOC;  // GPS location found, update status
                        }
                        else {
                            //Location is too old
                            gps_status=GPS_TIME;
                            SerialMon.print("GPS Loc age too high");
                            SerialMon.print(gps->location.age());
                            SerialMon.println("");
                        }
                    } else {
                        // HDOP is too high, position might be unstable
                        gps_status=GPS_TIME;
                        SerialMon.print("GPS Loc hdop too low ");
                        SerialMon.print(gps->hdop.value());
                        SerialMon.println("");

                        //Still store it, in case someone still wants to use it.
                        gps_latitude = gps->location.lat();
                        gps_longitude = gps->location.lng();
                        gps_location_age = gps->location.age();


                    }
                } else {
                    // Location might not be stable
                    gps_status=GPS_TIME;
                    /*SerialMon.print("GPS Loc sattelites too low ");
                    SerialMon.print(gps->satellites.value());
                    SerialMon.println("");
                    */

                }

                //SerialMon.print(F("LOCATION   Lat="));
                //SerialMon.print(gps_latitude, 6);
                //SerialMon.print(F(" Long="));
                //SerialMon.println(gps_longitude, 6);
            }
            else {
              gps_status=GPS_TIME;
              SerialMon.println("GPS Location invalid");
            }

        }
        printGPSIcon();
        lastGPSUpdate = millis();
    }
}

