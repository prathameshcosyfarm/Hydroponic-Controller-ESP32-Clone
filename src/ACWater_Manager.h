#ifndef AC_WATER_MANAGER_H
#define AC_WATER_MANAGER_H

#include "define.h"

void acWaterInit();
void acWaterUpdate();
void acWaterTask(void *parameter);
void acWaterResetDaily();

#endif