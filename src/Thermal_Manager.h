#ifndef THERMAL_MANAGER_H
#define THERMAL_MANAGER_H

#include "define.h"

void thermalInit();
void thermalUpdate();
void thermalReset();
void thermalTask(void *parameter);

extern float avg_temp_c;
extern float avg_humid_pct;
extern float g_heatIndex;
extern bool dhtEnabled;

#endif
