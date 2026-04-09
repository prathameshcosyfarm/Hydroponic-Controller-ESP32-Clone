#include "Command_Manager.h"
#include "define.h"
#include "WiFi_Manager.h"
#include <nvs_flash.h>
#include <Preferences.h>
#include "Thermal_Manager.h"
#include "Tank_Manager.h"
#include "Circulation_Manager.h"

void commandUpdate()
{
    if (Serial.available() > 0)
    {
        char cmd = Serial.read();

        // Command 'W': Wipe WiFi Credentials
        if (cmd == 'W' || cmd == 'w')
        {
            Serial.println(F("\n[COMMAND] Wiping WiFi credentials..."));
            prefs.begin("wifi-creds", false);
            prefs.clear();
            prefs.end();
            Serial.println(F("[SUCCESS] WiFi settings cleared. Restarting to use defaults from define.h..."));
            delay(1000);
            ESP.restart();
        }

        // Command 'C': Manual WiFi Configuration via Serial
        if (cmd == 'C' || cmd == 'c')
        {
            Serial.println(F("\n[COMMAND] Manual WiFi Configuration Mode"));

            Serial.println(F("Step 1: Enter New SSID:"));
            while (Serial.available() == 0)
                vTaskDelay(pdMS_TO_TICKS(10));
            String newSsid = Serial.readStringUntil('\n');
            newSsid.trim();

            Serial.println(F("Step 2: Enter New Password:"));
            while (Serial.available() == 0)
                vTaskDelay(pdMS_TO_TICKS(10));
            String newPass = Serial.readStringUntil('\n');
            newPass.trim();

            if (newSsid.length() > 0)
            {
                Serial.printf("[INFO] Received SSID: %s\n", newSsid.c_str());

                prefs.begin("wifi-creds", false);
                prefs.putString("ssid", newSsid);
                prefs.putString("pass", newPass);
                prefs.end();

                Serial.println(F("[SUCCESS] Credentials saved to NVS. Restarting..."));
                delay(2000);
                ESP.restart();
            }
            else
            {
                Serial.println(F("[ERROR] SSID cannot be empty. Configuration aborted."));
            }
        }

        // Command 'S': Sync WiFi Credentials with define.h
        if (cmd == 'S' || cmd == 's')
        {
            Serial.println(F("\n[COMMAND] Syncing NVS with current define.h defaults..."));
            prefs.begin("wifi-creds", false);
            prefs.putString("ssid", DEFAULT_SSID);
            prefs.putString("pass", DEFAULT_PASS);
            prefs.end();
            Serial.println(F("[SUCCESS] Credentials updated in NVS. Restarting..."));
            delay(1000);
            ESP.restart();
        }

        // Command 'F': Factory Reset (Wipe All NVS)
        if (cmd == 'F' || cmd == 'f')
        {
            Serial.println(F("\n[COMMAND] Performing Factory Reset (Wiping all NVS)..."));
            // Low-level erase of the entire NVS partition
            esp_err_t err = nvs_flash_erase();
            if (err == ESP_OK)
            {
                nvs_flash_init();
                Serial.println(F("[SUCCESS] All settings cleared. Restarting..."));
                delay(1000);
                ESP.restart();
            }
            else
            {
                Serial.printf("[ERROR] NVS Erase failed: 0x%x\n", err);
            }
        }

        // Command 'T': Reset Thermal Sensor
        if (cmd == 'T' || cmd == 't')
        {
            Serial.println(F("\n[COMMAND] Resetting Thermal Sensor..."));
            thermalReset();
            tankReset();
        }

        // Command 'M': Manual Circulation Trigger
        if (cmd == 'M' || cmd == 'm')
        {
            Serial.println(F("\n[COMMAND] Triggering Manual Circulation..."));
            triggerManualCirc();
        }
    }
}