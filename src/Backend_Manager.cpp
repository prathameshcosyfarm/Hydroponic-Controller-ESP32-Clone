#include "Backend_Manager.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "define.h"
#include "WiFi_Manager.h"

// External globals from various managers for serialization
extern float water_temp_c;
extern int g_co2Ppm;
extern float g_waterLevelPct;
extern float g_laserLevelPct;
extern float g_tankHealthPct;
extern float g_laserHealthPct;

WebSocketsClient webSocket;
static bool backendConnected = false;
static unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 10000; // Stream every 10 seconds

bool isBackendConnected() {
    return backendConnected;
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            backendConnected = false;
            Serial.println("[BACKEND] WebSocket Disconnected.");
            break;
        case WStype_CONNECTED:
            backendConnected = true;
            Serial.printf("[BACKEND] Connected to: %s\n", payload);
            // Send initial handshake/status
            backendSendStatus();
            break;
        case WStype_TEXT:
            Serial.printf("[BACKEND] Incoming Command: %s\n", payload);
            // Future: Parse incoming JSON for remote commands (e.g., manual pump)
            break;
        case WStype_ERROR:
            Serial.println("[BACKEND] WebSocket Error observed.");
            break;
    }
}

void backendInit() {
    // NGROK tunnels use SSL (WSS) by default.
    webSocket.beginSSL(BACKEND_WS_HOST, BACKEND_WS_PORT, BACKEND_WS_PATH);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
    
    // NGROK requires 'ngrok-skip-browser-warning' to bypass the interstitial page for automated clients.
    String headers = "X-Device-ID: " + g_deviceId + "\r\n";
    headers += "ngrok-skip-browser-warning: true";
    webSocket.setExtraHeaders(headers.c_str());

    Serial.println("Backend Manager: WebSocket (SSL/WSS) initialized for NGROK.");
}

void backendSendStatus() {
    if (!wifiConnected || !backendConnected) return;

    JsonDocument doc;
    doc["deviceId"] = g_deviceId;
    doc["version"]  = FIRMWARE_VERSION;
    doc["uptime"]   = millis() / 1000;
    doc["state"]    = g_currentSystemState;

    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["airTemp"]    = avg_temp_c;
    sensors["humidity"]   = avg_humid_pct;
    sensors["waterTemp"]  = water_temp_c;
    sensors["co2"]        = g_co2Ppm;
    sensors["tankLevel"]  = g_waterLevelPct;
    sensors["laserLevel"] = g_laserLevelPct;

    JsonObject diagnostics = doc.createNestedObject("diag");
    diagnostics["tankHealth"]  = g_tankHealthPct;
    diagnostics["laserHealth"] = g_laserHealthPct;
    diagnostics["rssi"]        = WiFi.RSSI();
    diagnostics["heap"]        = ESP.getFreeHeap();

    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(output);
    lastUpdate = millis();
}

void backendTask(void *parameter) {
    backendInit();
    
    for (;;) {
        if (wifiConnected) {
            webSocket.loop();

            // Heartbeat/Periodic Update
            if (backendConnected && (millis() - lastUpdate >= UPDATE_INTERVAL)) {
                backendSendStatus();
            }
        } else {
            backendConnected = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to CPU
    }
}