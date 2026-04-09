#ifndef TANK_MANAGER_H
#define TANK_MANAGER_H

#include "define.h"

void tankInit();
void tankUpdate();
void tankReset();
void tankTask(void *parameter);

extern float g_waterLevelPct;
extern float g_waterDistanceCm;
extern bool tankSensorEnabled;
extern float g_tankStdDev;
extern float g_tankHealthPct;

extern float water_temp_c;
extern bool ds18b20Enabled;

#endif