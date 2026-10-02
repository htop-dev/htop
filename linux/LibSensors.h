#ifndef HEADER_LibSensors
#define HEADER_LibSensors
/*
htop - linux/LibSensors.h
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>

#include "HardwareSensor.h"
#include "linux/LinuxMachine.h"


int LibSensors_init(void);
void LibSensors_cleanup(void);
int LibSensors_reload(void);

HardwareSensor* LibSensors_getHardwareSensors(size_t* count);
bool LibSensors_updateHardwareSensors(HardwareSensor* sensors, size_t count);
void LibSensors_freeHardwareSensors(HardwareSensor* sensors, size_t count);

int LibSensors_countCCDs(void);
void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs, unsigned int activeCPUs);

#endif /* HEADER_LibSensors */
