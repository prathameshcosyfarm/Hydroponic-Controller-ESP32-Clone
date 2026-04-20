#include "Backend_Manager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "define.h"
#include "WiFi_Manager.h"
#include "OTA_Manager.h"

static bool backendActive = false;
static int consecutiveFailures = 0;
static unsigned long lastPost = 0;
const unsigned long POST_INTERVAL = 10000UL; // 10 seconds base
const int BACKOFF_MAX_SHIFT = 5;             // cap: 10s * 2^5 = 320s (~5 min)

bool isBackendConnected()
{
    return backendActive;
}

// Exponential backoff: doubles interval per failure, capped so a down backend is polled at most every ~5 min.
static unsigned long currentInterval()
{
    int shift = consecutiveFailures;
    if (shift > BACKOFF_MAX_SHIFT) shift = BACKOFF_MAX_SHIFT;
    return POST_INTERVAL * (1UL << shift);
}

void backendSendStatus()
{
    lastPost = millis();

    if (!wifiConnected || isOtaInProgress())
        return;

    WiFiClient client;
    HTTPClient http;
    String url = CMS_SERVER_URL;
    Serial.printf("[BACKEND] HTTP POST to: %s\n", url.c_str());

    http.useHTTP10(true); // Disable chunked encoding and GZIP to ensure plain text response
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", CMS_API_KEY);
    http.addHeader("ngrok-skip-browser-warning", "true");
    http.addHeader("Accept", "application/json");
    http.addHeader("Accept-Encoding", "identity;q=1, *;q=0"); // Strictly request uncompressed data
    http.setUserAgent("ESP32-Hydro-Controller/" FIRMWARE_VERSION);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

    JsonDocument doc;

    // 1. System & Metadata
    JsonObject system = doc["system"].to<JsonObject>();
    system["deviceId"] = g_deviceId;
    system["version"] = FIRMWARE_VERSION;
    system["model"] = ESP.getChipModel();
    system["revision"] = ESP.getChipRevision();
    system["uptime"] = millis() / 1000;
    system["heapFree"] = ESP.getFreeHeap();
    system["psramFree"] = ESP.getFreePsram();
    system["state"] = g_currentSystemState;

    // 2. Network Telemetry
    JsonObject network = doc["network"].to<JsonObject>();
    network["ip"] = WiFi.localIP().toString();
    network["rssi"] = WiFi.RSSI();
    network["ssid"] = WiFi.SSID();
    network["mac"] = WiFi.macAddress();

    // 3. Time & Location (NTP/GeoIP)
    JsonObject timeLoc = doc["time"].to<JsonObject>();
    timeLoc["local"] = g_localTime;
    timeLoc["utc"] = g_utcTime;
    timeLoc["timezone"] = g_timezone;
    timeLoc["lat"] = g_lat;
    timeLoc["lon"] = g_lon;

    // 4. Sensor Readings & Signal Quality (Jitter)
    JsonObject sensors = doc["sensors"].to<JsonObject>();
    sensors["airTemp"] = avg_temp_c;
    sensors["airHumid"] = avg_humid_pct;
    sensors["heatIndex"] = g_heatIndex;
    sensors["airJitter"] = g_thermalStdDev;
    sensors["waterTemp"] = water_temp_c;
    sensors["co2Ppm"] = g_co2Ppm;
    sensors["co2Temp"] = g_co2Temp;
    sensors["co2Jitter"] = g_co2StdDev;
    sensors["tankLevel"] = g_waterLevelPct;
    sensors["tankVolumeL"] = g_waterVolumeL;
    sensors["tankJitter"] = g_tankStdDev;
    sensors["laserLevel"] = g_laserLevelPct;
    sensors["laserJitter"] = g_laserStdDev;

    // 5. Actuator States
    JsonObject actuators = doc["actuators"].to<JsonObject>();
    actuators["circEnabled"] = g_circPumpEnabled;
    actuators["circPump"] = g_circPumpRunning;
    actuators["acPump"] = g_acPumpRunning;
    actuators["acVolToday"] = g_acWaterPumpedToday;

    // 6. Component Health Flags
    JsonObject health = doc["health"].to<JsonObject>();
    health["dhtOk"] = dhtEnabled;
    health["ds18b20Ok"] = ds18b20Enabled;
    health["co2Ok"] = co2Enabled;
    health["co2Warm"] = co2WarmedUp;
    health["laserOk"] = laserEnabled;
    health["tankOk"] = tankSensorEnabled;
    health["tankHealth"] = g_tankHealthPct;
    health["laserHealth"] = g_laserHealthPct;

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);
    String response = (httpCode > 0) ? http.getString() : http.errorToString(httpCode);

    JsonDocument responseDoc;
    DeserializationError jsonError = deserializeJson(responseDoc, response);
    bool statusVerified = (!jsonError && responseDoc["status"] == "ok");

    if (httpCode >= 200 && httpCode < 400 && statusVerified)
    {
        Serial.printf("[BACKEND] OK HTTP %d (%u bytes)\n", httpCode, response.length());
        backendActive = true;
        consecutiveFailures = 0;
    }
    else
    {
        bool printable = response.length() > 0 && (unsigned char)response[0] < 128;
        Serial.printf("[BACKEND] FAIL HTTP %d: %s%s\n",
                      httpCode,
                      jsonError ? "bad-json " : "",
                      printable ? response.substring(0, 100).c_str() : "<binary>");

        backendActive = false;
        if (consecutiveFailures < 100) consecutiveFailures++;
    }
    http.end();
}

void backendTask(void *parameter)
{
    // Wait for WiFi_Manager to finish NTP sync and OTA checks
    while (g_currentSystemState != STATE_CONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    Serial.println("Backend: Core services (NTP/OTA) ready. Performing initial availability check...");

    // Perform an immediate initial check to verify backend presence
    backendSendStatus();

    for (;;)
    {
        if (wifiConnected && !isOtaInProgress())
        {
            if (millis() - lastPost > currentInterval())
            {
                backendSendStatus();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void backendInit() {} // Stateless
