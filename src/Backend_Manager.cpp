#include "Backend_Manager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "define.h"
#include "WiFi_Manager.h"

// External globals from various managers for serialization
extern float water_temp_c;
extern int g_co2Ppm;
extern int g_co2Temp;
extern float g_co2StdDev;
extern float g_waterLevelPct;
extern float g_waterVolumeL;
extern float g_waterDistanceCm;
extern float g_tankStdDev;
extern float g_laserLevelPct;
extern float g_laserDistanceCm;
extern float g_laserStdDev;
extern float g_tankHealthPct;
extern float g_laserHealthPct;
extern float avg_temp_c;
extern float avg_humid_pct;
extern float g_heatIndex;
extern float g_thermalStdDev;
extern float g_acWaterPumpedToday;
extern bool g_circPumpRunning;
extern bool g_acPumpRunning;
extern String g_lat;
extern String g_lon;
extern String g_localTime;
extern String g_timezone;

static bool backendActive = true;
static bool backendSuccessfullyPosted = false; // New flag to track successful posts
static unsigned long lastPost = 0; // Initialized to 0 to trigger immediate first post
const unsigned long POST_INTERVAL = 30000; // 30s

bool isBackendConnected()
{
    // Return true only if backend communication is active AND the last post was successful
    return backendActive && backendSuccessfullyPosted;
}

void backendSendStatus()
{
    if (!wifiConnected || !backendActive)
        return;

    WiFiClient client;
    client.setTimeout(4000); // Timeout must be shorter than the 5s Watchdog (WDT)

    HTTPClient http;
    String url = CMS_SERVER_URL;
    Serial.printf("[BACKEND] HTTP POST to: %s\n", url.c_str());

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", CMS_API_KEY);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

    JsonDocument doc;
    doc["deviceId"] = g_deviceId;
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["state"] = g_currentSystemState;
    doc["localTime"] = g_localTime;
    doc["timezone"] = g_timezone;

    JsonObject location = doc["location"].to<JsonObject>();
    location["lat"] = g_lat;
    location["lon"] = g_lon;

    JsonObject sensors = doc["sensors"].to<JsonObject>();
    
    JsonObject air = sensors["air"].to<JsonObject>();
    air["temp"] = avg_temp_c;
    air["humidity"] = avg_humid_pct;
    air["heatIndex"] = g_heatIndex;
    air["jitter"] = g_thermalStdDev;

    JsonObject water = sensors["water"].to<JsonObject>();
    water["temp"] = water_temp_c;
    water["levelPct"] = g_waterLevelPct;
    water["volumeL"] = g_waterVolumeL;
    water["distCm"] = g_waterDistanceCm;
    water["jitter"] = g_tankStdDev;

    JsonObject co2 = sensors["co2"].to<JsonObject>();
    co2["ppm"] = g_co2Ppm;
    co2["intTemp"] = g_co2Temp;
    co2["jitter"] = g_co2StdDev;

    JsonObject laser = sensors["laser"].to<JsonObject>();
    laser["levelPct"] = g_laserLevelPct;
    laser["distCm"] = g_laserDistanceCm;
    laser["jitter"] = g_laserStdDev;

    JsonObject pumps = doc["pumps"].to<JsonObject>();
    pumps["circulation"] = g_circPumpRunning;
    pumps["acDrain"] = g_acPumpRunning;
    pumps["acTotalTodayL"] = g_acWaterPumpedToday;

    JsonObject diagnostics = doc["diag"].to<JsonObject>();
    diagnostics["tankHealth"] = g_tankHealthPct;
    diagnostics["laserHealth"] = g_laserHealthPct;
    diagnostics["ip"] = WiFi.localIP().toString();
    diagnostics["rssi"] = WiFi.RSSI();
    diagnostics["heap"] = ESP.getFreeHeap();

    String payload;
    serializeJson(doc, payload);
    Serial.println("[JSON] " + payload);

    int httpCode = http.POST(payload);
    String response = http.getString();

    Serial.printf("[BACKEND] HTTP %d body:%d bytes\n", httpCode, response.length());
    
    // Refined check: success requires a 2xx/3xx code AND a non-empty response body
    if (httpCode >= 200 && httpCode < 400 && response.length() > 0)
    {
        JsonDocument responseDoc;
        DeserializationError error = deserializeJson(responseDoc, response);

        // Verify the JSON structure and look for the success key
        if (!error && responseDoc["status"] == "success")
        {
            Serial.printf("[SUCCESS] HTTP %d | Verified Logic: %s\n", httpCode, response.c_str());
            backendSuccessfullyPosted = true;
            lastPost = millis();
        }
        else
        {
            Serial.printf("[WARN] HTTP %d but JSON Verification Failed. Error: %s\n", httpCode, error.c_str());
            backendSuccessfullyPosted = false;
        }
    }
    else if (httpCode >= 200 && httpCode < 400 && response.length() == 0)
    {
        Serial.printf("[WARN] HTTP %d but EMPTY response. Backend logic might be down.\n", httpCode);
        backendSuccessfullyPosted = false;
    }
    else
    {
        Serial.printf("[FAIL] HTTP %d: %s\n", httpCode, response.c_str());
        backendSuccessfullyPosted = false;
    }
    http.end();
}

void backendTask(void *parameter)
{
    while (g_currentSystemState != STATE_CONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    Serial.println("Backend: NGROK HTTP POST (-5/EOF solved).");

    for (;;)
    {
        if (wifiConnected && backendActive)
        {
            // Logic: Start sending immediately upon entering this loop, then every 30s
            unsigned long now = millis();
            if (lastPost == 0 || (now - lastPost > POST_INTERVAL))
            {
                lastPost = now; // Update lastPost before sending to prevent re-triggering
                backendSendStatus();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void backendInit() {} // Stateless
