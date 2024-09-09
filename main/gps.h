#ifndef GPS_H
#define GPS_H

#include <TinyGPS++.h>

// Enum to represent GPS status
enum GPSStatus {
    NO_GPS = 0,
    GPS_INIT,
    GPS_ERROR,
    GPS_TIME,
    GPS_LOC
};

extern GPSStatus gps_status;  // Holds the current GPS status

// GPS-related extern variables for use in the application
extern int gps_satellites;          // Holds the number of satellites
extern double gps_latitude;
extern double gps_longitude;
extern float gps_altitude;
extern unsigned long gps_location_age;
extern unsigned long gps_date_age;
extern uint32_t gps_date_value;
extern unsigned long gps_time_age;
extern uint32_t gps_time_value;
extern float gps_speed_kmph;
extern float gps_speed_mps;
extern float gps_course;
extern float gps_hdop;

bool setupGPS();
void loopGPS();

#endif
