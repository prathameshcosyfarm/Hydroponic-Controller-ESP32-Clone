# ESP32 Hydroponic Controller - Task Tracking

## NEW TASK: Optimize task stacks + crash prevention [PLANNING]

**Status:** Repo clean except TODO.md. main.cpp has task creates.

**Information Gathered:**
- Tasks in main.cpp: wifiMonitor(4096), systemInfo(4096), CO2/laser/tank/thermal(3072), acWater/circ(2048), backend(16384), OTA temp(12288), LED(2048)
- Architecture.md table outdated (e.g. Thermal 16384 vs actual 3072)
- WDT: 5s (platformio.ini). Heap/PSRAM logged.
- Git clean.

**Plan (file-level):**
1. **main.cpp**: Further optimize stacks:
   - backendTask: 12288 (from 16384, enough for JSON/HTTP)
   - Thermal/Tank/etc: confirm 3072 OK
   - Add uxTaskGetStackHighWaterMark logging for monitoring
2. **architecture.md**: Sync table stacks to actual
3. **All managers**: Add stack canary checks, vTaskDelay bounds
4. **define.h**: Add STACK_MIN_THRESHOLD (e.g. 512B)
5. **main.cpp loop**: Add uxTaskGetSystemState heap/stack monitor, WDT feed

**Dependent Files:** src/*.h (externs), platformio.ini (WDT)

**Followup:** Commit/push, `pio run -t monitor` check stack HWM, stress test.

Approve plan? Specific stacks or crash scenarios?

