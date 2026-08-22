#ifndef HEADER_StatusBar
#define HEADER_StatusBar
/*
htop - StatusBar.h
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

#include "HardwareSensor.h"
#include "Machine.h"


void StatusBar_formatSensorName(char* buffer, size_t size, const char* chip, const char* label, const char* feature, HardwareSensorType type);
void StatusBar_draw(const Machine* host, int y);

#endif
