#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <Arduino.h>

/**
 * Monitors the Serial interface for incoming user commands.
 * Handles WiFi wiping, manual configuration, syncing, and factory resets.
 * Should be called frequently in the main loop.
 */
void commandUpdate();

#endif