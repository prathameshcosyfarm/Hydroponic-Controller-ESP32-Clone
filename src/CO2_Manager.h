#ifndef CO2_MANAGER_H
#define CO2_MANAGER_H

#include "define.h"
#include <MHZ19.h>

extern int g_co2Ppm;
extern int g_co2Temp;
extern bool co2Enabled;
extern bool co2WarmedUp;

void co2Init();
void co2Update();
void co2Reset();

// FreeRTOS Task
void co2Task(void *parameter);

#endif