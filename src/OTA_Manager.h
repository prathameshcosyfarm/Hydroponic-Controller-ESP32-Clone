#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "define.h"
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

// Function to check for new OTA firmware version after NTP sync.
// It initiates the version comparison and potentially the update process.
void otaCheckAfterNtp();

// FreeRTOS task function for handling the actual firmware download and update.
// This runs in a separate thread to avoid blocking other operations.
void otaUpdateTask(void *parameter);

// Function to query if an OTA update is currently in progress.
bool isOtaInProgress();

#endif
