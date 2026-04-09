#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include "define.h"

/**
 * Initializes the internal RTC logic and attempts to load cached time settings.
 */
void rtcInit();

/**
 * Updates the RTC sync status. Should be called after successful NTP synchronization.
 */
void rtcSyncWithNTP();

/**
 * Periodically checks for day-change events to trigger drift correction.
 */
void rtcUpdate();

/**
 * Returns the current live local time as a formatted string.
 */
String rtcGetLocalTimeStr();

#endif