#ifndef CIRCULATION_MANAGER_H
#define CIRCULATION_MANAGER_H

#include "define.h"

extern bool g_circPumpRunning;
extern bool g_circPumpEnabled;

void circInit();
void circUpdate();
void triggerManualCirc();
void circTask(void *parameter);

#endif // CIRCULATION_MANAGER_H