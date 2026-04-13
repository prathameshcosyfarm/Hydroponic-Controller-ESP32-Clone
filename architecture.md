# ESP32 Cosy Farm Controller - Architecture (Updated to match current codebase incl. CO2 v1.1.0)

## Overview
ESP32-S3 WiFi controller for Cosy Farm IoT system. Manages thermal monitoring (DHT22), water tank (HC-SR04 + DS18B20 + **VL53L0X**), AC condensate drainage (float + relay pump), **CO2 (MH-Z19E)**, with RGB LED status, NTP/RTC time sync, OTA from GitHub, serial CLI commands. Uses FreeRTOS tasks, LittleFS logging with RTC-persistent buffer, NVS prefs, auto sensor failure recovery. PlatformIO Arduino framework.

Repo: https://github.com/PrathameshMestry/CosyFarm-ESP32

## Hardware
- **Board**: ESP32-S3-DevKitC-1 (8MB Flash, 512KB SRAM, dual-core 240MHz)
- **UART**: USB CDC (115200 baud)

### Pin Mapping & Configs
| GPIO | Function | Sensor/Device | Notes/Calibration |
|------|----------|---------------|-------------------|
| 1 | RGB G | WS2812/LED | PWM ch1, 100kHz/8bit |
| 2 | RGB R | WS2812/LED | PWM ch0 |
| 3 | RGB B | WS2812/LED | PWM ch2 |
| 6 | DHT22 Data | Air T/RH | 4.7kΩ pull-up, 2s samples, EMA filter, SD check (threshold: 1.0C) |
| 7 | DS18B20 DQ | Water T (tank) | OneWire, 4.7kΩ pull-up, EMA filter |
| 10 | HC-SR04 Trig | Tank level ultrasonic | AJ-SR04M (R19 REMOVED). 11-sample median burst + Adaptive EMA. |
| 11 | HC-SR04 Echo | Tank level ultrasonic | Effective range 0-22cm. 3.3V Divider required on Echo. Burst spread < 4cm for validity. |
| 8 | Float Switch | AC Condensate Level | Pull-up, LOW=full, 3s debounce, 10s cooldown |
| 9 | Relay | AC Water Pump | Active HIGH, 90s cycles (~2L @1.5L/min) |
| 15 | Relay/Transistor | Circulation Pump | Active HIGH. 5min hourly cycle. Interlocked by Level & Temp. |
| 4 | VL53L0X SDA | Laser TOF | I2C. Long Range Mode. 7-sample median burst + Adaptive EMA. |
| 5 | VL53L0X SCL | Laser TOF | I2C. Burst spread < 2cm for validity. |
| 17 | MH-Z19 RX | CO2 Sensor UART2 | 9600 baud, Adaptive EMA, SD check (threshold: 150ppm). |
| 18 | MH-Z19 TX | CO2 Sensor UART2 | 9600 baud, internal temp monitor (overheat >55C). |

## Software Stack

### Core Components
- **main.cpp**: Serial/10s delay → g_deviceId=eFuse MAC→NVS → LittleFS/RTC log → hw info → ledInit/wifiInit → tasks → sensor inits. 100ms loop(commandUpdate). systemInfoTask (60s reports: uptime/sensors/CO2/WiFi RSSI/IP/heap/PSRAM/g_currentSystemState).
- **define.h**: Pins, globals (extern), states (0=NTP_SYNC ... 6=OTA_UPDATE), timings (AC_PUMP_RUN_TIME_MS=90s etc.), FIRMWARE_VERSION=\"1.0.0\". Defines `stateQueue` for inter-task state communication and `g_currentSystemState` as the authoritative system state. Includes new constants for Laser TOF (`TOF_SD_THRESHOLD`, `TOF_JUMP_THRESHOLD`, `TOF_MEDIAN_SAMPLES`, `TOF_MAX_SPREAD_CM`), AC Water (`AC_EMPTY_DEBOUNCE_MS`, `AC_PUMP_RUN_TIME_MS`), and `g_acWaterPumpedToday`.
- **Logging**: LittleFS `/system_log.txt` (128kB trunc), RTC_DATA_ATTR 2kB buffer (persist resets/brownouts, flush overflow/10min/critical/60s reports).
- **Storage**: NVS prefs (\"device\"/\"wifi-creds\"/\"ota\"), LittleFS mounted in setup.
- **ID**: g_deviceId = "COSYFARM-" + Decimal representation of eFuse MAC.

### FreeRTOS Tasks (all pri=1)

| Manager | Purpose | Update Freq | Stack Size | Globals/Key Logic |
|---------|---------|-------------|------------|-------------------|
| **Thermal_Manager** | DHT22 air T/RH (EMA filtered) | 2s | 16384 | avg_temp_c, avg_humid_pct, g_thermalStdDev (jitter), dhtEnabled (fail≥5→10min recovery). Uses `DHT_SD_SAMPLES` (10) and `THERMAL_EMA_ALPHA` (0.05f). |
| **Tank_Manager** | HC-SR04 median burst (5) + Adaptive EMA / DS18B20 (EMA), Volume/Health | 5s | 4096 | g_waterLevelPct/g_waterVolumeL, g_tankHealthPct, g_tankDryRunRisk (14cm threshold). Burst spread < 4cm for validity. Speed of sound compensated by DHT temp. |
| **LaserTOF_Manager** | VL53L0X Laser TOF distance (median burst + Adaptive EMA) | 5s | 4096 | g_laserDistanceCm, g_laserLevelPct, g_laserStdDev (jitter), laserEnabled (fail≥10→recovery). 7-sample median burst, burst spread < 2cm for validity. |
| **CO2_Manager** | MH-Z19E ppm (Adaptive EMA) / int T | 5s | 4096 | g_co2Ppm, g_co2StdDev, Adaptive EMA (Slow/Fast), overheat>55C protection. Uses `CO2_AVG_SAMPLES` (20), `CO2_JUMP_THRESHOLD` (250), `CO2_MAX_SLEW_PPM` (500). |
| **Circulation_Manager** | Hourly pump cycle | 1s | 2048 | g_circPumpRunning, 5min/hr window, interlocks: `!g_tankDryRunRisk` && `water_temp_c < 40C`. |
| **ACWater_Manager** | Float→pump FSM | 200ms | 2048 | g_acWaterPumpedToday (daily reset). States: IDLE/PUMPING/COOLDOWN/FAULT. Uses `AC_EMPTY_DEBOUNCE_MS` (1s) and `AC_PUMP_RUN_TIME_MS` (90s). |
| **WiFi_Manager** | STA (NVS/\"COSYFARM\") monitor+rollback | Async/100ms | 8192 | wifiConnected; AP SSID=g_deviceId. Sends state changes to `stateQueue`. Uses `g_syncTriggered` for NTP/OTA. |
| **systemInfoTask** | Aggregate Serial/LittleFS reports | 60s | 8192 | Uptime, sensors/CO2/Laser, WiFi, mem/heap/PSRAM, `g_currentSystemState`/OTA prog. |
| **wifiMonitorTask** | Events+NTP/OTA | 100ms | 8192 | Reconnect, ntpAttempt/otaCheckAfterNtp. Sends state changes to `stateQueue`. |
| **Backend_Manager** | HTTP POST JSON telemetry to CMS_SERVER_URL | 10s (POST_INTERVAL) | 8192 | deviceId/version/uptime/state/sensors{airTemp,humidity,waterTemp,co2,tankLevel,laserLevel}/diag{tankHealth,laserHealth,rssi,heap}. |

### Backend Telemetry JSON&#10;```json&#10;{&#10;  "deviceId": "COSYFARM-XXXX",&#10;  "version": "1.0.1",&#10;  "uptime": 12345,&#10;  "state": 3,&#10;  "sensors": {&#10;    "airTemp": 24.5,&#10;    "humidity": 65.2,&#10;    "waterTemp": 22.1,&#10;    "co2": 420,&#10;    "tankLevel": 75.0,&#10;    "laserLevel": 68.3&#10;  },&#10;  "diag": {&#10;    "tankHealth": 92.1,&#10;    "laserHealth": 88.5,&#10;    "rssi": -65,&#10;    "heap": 285000&#10;  }&#10;}&#10;```&#10;&#10;**Other Modules** (non-task/init-only):
 
- **NTP_Manager**: GeoIP/tz JSON sync on connect. Defines `g_lat`, `g_lon`, `g_timezone`, `g_utcTime`, `g_localTime`, `g_epochTime`. `rtcUpdate` daily.
- **Command_Manager**: Serial CLI (W/w/C/c/S/s/F/f/T/t/M/m). Includes `thermalReset()`, `tankReset()`, `triggerManualCirc()`.
- **LED_Manager**: Dedicated FreeRTOS task (`ledTask`) consuming state changes from `stateQueue`. Updates `g_currentSystemState`. PWM RGB blink patterns per `g_currentSystemState` (e.g. solid green=CONNECTED).
- **RTC_Manager**: ESP RTC + NTP drift correction, day-change→ntp+acWaterResetDaily. Uses `lastSyncDay` and `lastSyncTimestamp`.
- **OTA_Manager**: GitHub check post-NTP/24h, rollback if WiFi fail<5min. Uses `otaInProgress` and `targetOtaMd5`.

### UART Serial Commands (115200 baud, single char trigger in commandUpdate())

| Command | Case | Description | Effect |
|---------|------|-------------|--------|
| **W/w** | Both | Wipe WiFi Credentials | Clear NVS \"wifi-creds\", restart to DEFAULT_SSID/PASS |
| **C/c** | Both | Manual WiFi Config | Prompt SSID → PASS → save NVS → restart |
| **S/s** | Both | Sync WiFi Defaults | Copy define.h DEFAULT_SSID/PASS to NVS → restart |
| **F/f** | Both | Factory Reset | nvs_flash_erase() all → restart |
| **T/t** | Both | Reset Sensors | thermalReset() + tankReset() |
| **M/m** | Both | Manual Circulation | Triggers a manual 5-minute pump cycle if safe. |

### Key Globals (define.h extern)
- Sensors: avg_temp_c/avg_humid_pct, water_temp_c, g_waterLevelPct/g_waterVolumeL, g_tankStdDev/g_tankHealthPct, g_laserDistanceCm/g_laserLevelPct/g_laserStdDev, g_co2Ppm/g_co2Temp, g_co2StdDev, g_acWaterPumpedToday, g_circPumpRunning, g_circPumpEnabled.
- System: `g_currentSystemState` (0=NTP_SYNC..6=OTA_UPDATE), `stateQueue`, wifiConnected/ntpRetryCount, g_deviceId=\"COSYFARM-<ID>\", g_lat/g_lon/g_timezone/g_epochTime/g_utcTime/g_localTime, dhtEnabled/tankSensorEnabled/ds18b20Enabled/co2Enabled/laserEnabled/co2WarmedUp, otaInProgress, targetOtaMd5.

## Data Flow
1. setup(): Serial(115200)/10s delay, g_deviceId=eFuse MAC→NVS, LittleFS/RTC log buffer, hw info print, ledInit/wifiInit → creates `stateQueue` → tasks(wifiMonitor/systemInfo/co2/laser/acWater/tank/thermal) → sensor inits.
2. Sensor tasks update their respective globals. Manager tasks (WiFi, OTA) send state changes to `stateQueue`.
3. `ledTask` consumes from `stateQueue`, updates `g_currentSystemState` (which is the authoritative system state), and controls the RGB LED.
3. systemInfoTask aggregates → Serial + logStatusToFile (RTC-buffered).
4. RTC day-change → NTP resync + daily resets (acWater).
5. Loop: commandUpdate() → CLI actions (e.g., sensor reset).

## Build & OTA
**platformio.ini**:
```
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps = 
    bblanchon/ArduinoJson @ ^7.0.0
    adafruit/DHT sensor library @ ^1.4.6
    paulstoffregen/OneWire @ ^2.3.7
    milesburton/DallasTemperature @ ^3.9.0
    links2004/WebSockets @ ^2.4.1
    https://github.com/WifWaf/MH-Z19.git # MH-Z19E library
    https://github.com/adafruit/Adafruit_VL53L0X.git # VL53L0X Laser TOF library
build_flags = 
    -DCORE_DEBUG_LEVEL=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_ESP_TASK_WDT_INIT=1
    -DCONFIG_ESP_TASK_WDT_TIMEOUT_S=5
```

**OTA URLs**:
- Version: https://raw.githubusercontent.com/PrathameshMestry/CosyFarm-ESP32/main/version.txt
- Firmware: https://raw.githubusercontent.com/PrathameshMestry/CosyFarm-ESP32/main/firmware.bin

## Failure Handling
- **Sensors**: Consecutive fail threshold (5 reads) → disable + 10min auto-recovery/re-init (separate HC-SR04/DS18B20/CO2/DHT).
- **Logging**: RTC buffer survives brownout/reset, auto-flush.
- **WDT**: 5s task timeout.
- **OTA Rollback**: If new FW WiFi fail >5min.

Firmware fully matches this architecture (incl. CO2). Flash & monitor with `pio run -t upload -t monitor`.
