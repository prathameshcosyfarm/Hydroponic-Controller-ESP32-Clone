#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "define.h"


// enum moved if needed

void ledInit();
void ledSetColor(uint8_t r, uint8_t g, uint8_t b);
void ledBlink(int state, unsigned long now);

#endif
