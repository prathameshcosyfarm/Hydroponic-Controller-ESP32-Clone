#ifndef LASER_TOF_MANAGER_H
#define LASER_TOF_MANAGER_H

#include "define.h"

void laserInit();
void laserUpdate();
void laserReset();

extern float g_laserDistanceCm;
extern float g_laserLevelPct;
extern float g_laserStdDev;
extern float g_laserHealthPct;
extern bool laserEnabled;

// FreeRTOS task function
void laserTask(void *parameter);

#endif