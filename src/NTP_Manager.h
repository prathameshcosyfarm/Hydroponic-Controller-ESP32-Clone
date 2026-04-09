#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include "define.h"


// Initializes the NTP client, typically by loading cached geo-location data.
void ntpInit();

// Updates NTP and geo-location data, called on fresh WiFi connect or periodically.
void ntpUpdateOnConnect(); 

// Saves geo-location and timezone offset to NVS for persistence.
void saveGeoCache(long offset);

// Loads geo-location and timezone offset from NVS.
bool loadGeoCache();

// External declarations for global variables related to geo-location and time.
extern String g_lat;
extern String g_lon;
extern time_t g_epochTime;

#endif
