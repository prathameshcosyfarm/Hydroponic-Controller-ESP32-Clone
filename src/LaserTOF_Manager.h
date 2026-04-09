#ifndef LASER_TOF_MANAGER_H
#define LASER_TOF_MANAGER_H

#include "define.h"

void laserInit();
void laserUpdate();
void laserReset();


extern float g_laserStdDev;
extern float g_laserHealthPct;
extern bool laserEnabled;
extern bool g_laserReflectionFlag;
extern float g_laserCorrectedDistCm;
extern uint32_t g_laserI2cTotal;  // Total I2C transaction attempts
extern uint32_t g_laserI2cErrors; // Total low-level I2C failures

// FreeRTOS task function
void laserTask(void *parameter);

#endif