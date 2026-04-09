#include "RTC_Manager.h"
#include <time.h>
#include "NTP_Manager.h"
#include "ACWater_Manager.h"

static int lastSyncDay = -1;
static char lastSyncTimestamp[20] = "Never"; // Fixed buffer: YYYY-MM-DD HH:MM:SS

void rtcInit() {
    // Internal RTC is managed by the ESP32 system clock.
    // We initialize by loading the geo-cache to ensure timezone offsets are ready.
    ntpInit();
}

void rtcSyncWithNTP() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        lastSyncDay = timeinfo.tm_mday;

        strftime(lastSyncTimestamp, sizeof(lastSyncTimestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        
        Serial.println("Internal RTC: Synchronized with NTP. Local day tracking started.");
    }
}

void rtcUpdate() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    // Requirement: Correct drift on day change event.
    if (lastSyncDay != -1 && timeinfo.tm_mday != lastSyncDay) {
        Serial.println("Internal RTC: Day change detected. Triggering NTP re-sync to correct drift...");
        acWaterResetDaily();
        ntpUpdateOnConnect(); // This will trigger rtcSyncWithNTP() internally
    }
}

String rtcGetLocalTimeStr() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "Time Not Set";
    
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // Efficiently format the output string
    char outputBuf[100];
    snprintf(outputBuf, sizeof(outputBuf), "%s (Last Sync: %s)", buf, lastSyncTimestamp);
    
    return String(outputBuf);
}