#include "WiFi_Manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Update.h>
#include <freertos/queue.h> // Required for xQueueSend
#include "NTP_Manager.h"
#include "OTA_Manager.h"

// Global WiFi credentials (defined here, declared extern in define.h)
String g_ssid;
String g_password;

// Rollback protection variables
bool isPendingVerify = false;
unsigned long rollbackTimer = 0;
// Allow 5 minutes for the new firmware to successfully connect and sync.
#define ROLLBACK_TIMEOUT_MS 300000

// Flag to trigger NTP and OTA check outside of the WiFi event task context
static bool g_syncTriggered = false;

// Forward declaration or move the function definition up to ensure
// it is in scope for the event handlers.
bool ntpAttempt()
{
  // Set current state to NTP synchronization.
  int newState = STATE_NTP_SYNC;
  xQueueSend(stateQueue, &newState, 0);
  // Call the NTP update function which also fetches geo-location.
  ntpUpdateOnConnect();
  if (g_epochTime > 0 && g_lat.length() > 0)
  {
    int nextState = STATE_CONNECTED;
    xQueueSend(stateQueue, &nextState, 0);
    ntpRetryCount = 0;
    return true;
  }
  return false;
}

// Event handler for WiFi station connected to AP.
// This event signifies that the ESP32 has successfully connected to a WiFi access point,
// but it might not have received an IP address yet.
void WiFiEventConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.printf("WiFi connected to AP: %s\n", WiFi.SSID().c_str());
  // The currentState is not immediately set to STATE_CONNECTED here because
  // we need to wait for an IP address (handled by WiFiEventGotIP) and
  // complete NTP/OTA checks.
}

// Event handler for WiFi station obtaining an IP address.
// This event signifies that the ESP32 has a valid IP address and is fully connected to the network.
void WiFiEventGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  wifiConnected = true;
  Serial.printf("WiFi Connected! IP=%s, Signal=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  ntpRetryCount = 0; // Reset NTP retry counter on successful connection.

  // If this boot was a trial for new firmware, mark it as successful.
  if (isPendingVerify)
  {
    isPendingVerify = false;
    // Use a temporary Preferences object to avoid conflicts if prefs is used elsewhere
    Preferences tempPrefs;
    tempPrefs.begin("ota", false);
    tempPrefs.putBool("pending-verify", false);
    tempPrefs.end();
    Serial.println(F("Rollback Protection: WiFi verified! Success flag cleared."));
  }

  // Signal the wifiMonitorTask to perform NTP/OTA sync
  g_syncTriggered = true;
}

// Event handler for WiFi station disconnected from AP.
// This event is triggered when the ESP32 loses its connection to the WiFi access point.
void WiFiEventDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  wifiConnected = false;
  // In ESP32 Core 3.0+, 'disconnected' was renamed to 'wifi_sta_disconnected'
  Serial.printf("WiFi disconnected from AP, reason: %d. Retrying...\n", info.wifi_sta_disconnected.reason);

  // Attempt to reconnect using the stored global credentials.
  WiFi.begin(g_ssid.c_str(), g_password.c_str());
  int newState = STATE_CONNECTING;
  xQueueSend(stateQueue, &newState, 0); // Maintain connecting state during retry
}

// Initializes WiFi connection.
// It first attempts to load saved WiFi credentials from NVS.
// If no credentials are found, it uses default ones and saves them.
// Then, it tries to connect to the WiFi network.
void wifiInit()
{
  // Begin NVS preferences in "wifi-creds" namespace.
  prefs.begin("wifi-creds", false);
  g_ssid = prefs.getString("ssid", "");
  g_password = prefs.getString("pass", "");
  if (g_ssid == "")
  {
    Serial.printf("No saved WiFi creds. Using default: %s\n", DEFAULT_SSID);
    g_ssid = DEFAULT_SSID;
    g_password = DEFAULT_PASS;
    prefs.putString("ssid", g_ssid);
    prefs.putString("pass", g_password);
  }
  else
  {
    Serial.printf("Using saved WiFi creds from NVS: %s\n", g_ssid.c_str());
    // Diagnostic: Warn if saved SSID differs from the current hardcoded default
    if (g_ssid != DEFAULT_SSID)
    {
      Serial.printf("INFO: Saved SSID differs from DEFAULT_SSID ('%s').\n", DEFAULT_SSID);
      Serial.println(F("      To use the new default, send 'W' over Serial to wipe NVS."));
    }
  }
  prefs.end();

  // Check if we are in a "trial" boot following an OTA update.
  prefs.begin("ota", true);
  isPendingVerify = prefs.getBool("pending-verify", false);
  prefs.end();

  if (isPendingVerify)
    rollbackTimer = millis();

  // Set the WiFi hostname to the device ID for easier identification on the network
  WiFi.setHostname(g_deviceId.c_str());

  // Register WiFi event handlers to enable event-driven connection management.
  WiFi.onEvent(WiFiEventConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiEventGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiEventDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  Serial.printf("Attempting to connect to '%s'...\n", g_ssid.c_str());
  WiFi.begin(g_ssid.c_str(), g_password.c_str());
  int newState = STATE_CONNECTING;
  xQueueSend(stateQueue, &newState, 0); // Set initial state to connecting.
}

// FreeRTOS task to continuously monitor WiFi connection status.
// It handles reconnections, triggers NTP updates, and OTA checks when WiFi is connected.
// Also updates the LED status based on the current state.
void wifiMonitorTask(void *parameter)
{
  Serial.println(F("WiFi Monitor Task started"));

  for (;;)
  {
    // Safe mode removed - normal operation

    // LED update removed from here (handled by dedicated LED_Task)

    // Perform NTP and OTA check when triggered (and WiFi is still connected)
    if (g_syncTriggered && wifiConnected)
    {
      g_syncTriggered = false;
      Serial.println(F("WiFi Monitor: Starting post-connection sync (NTP/OTA)..."));

      if (!ntpAttempt())
      {
        ntpRetryCount++;
        if (ntpRetryCount >= 3)
        {
          Serial.println(F("NTP failed after 3 tries, setting state to CONNECTED"));
          int newState = STATE_CONNECTED;
          xQueueSend(stateQueue, &newState, 0);
        }
      }

      otaCheckAfterNtp();
    }

    // Rollback Logic: If WiFi fails to connect within the timeout after an update.
    if (isPendingVerify && (millis() - rollbackTimer > ROLLBACK_TIMEOUT_MS))
    {
      Serial.println(F("Rollback Protection: Connection timeout! Reverting to previous firmware..."));

      Preferences tempPrefs;
      tempPrefs.begin("ota", false);
      tempPrefs.putBool("pending-verify", false);
      // Revert the NVS version string so it reflects the version we are rolling back to.
      tempPrefs.putString("ota-version", FIRMWARE_VERSION);
      tempPrefs.end();

      if (Update.rollBack())
      {
        ESP.restart();
      }
      else
      {
        Serial.println(F("Rollback Protection: ERROR - Rollback failed (no previous image?)"));
        isPendingVerify = false; // Stop trying to rollback if it's impossible.
      }
    }

    // Delay the task for 100 milliseconds to allow other FreeRTOS tasks to run.
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
