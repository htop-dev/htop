#ifndef HEADER_LibSensors
#define HEADER_LibSensors
/*
htop - linux/LibSensors.h
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/LinuxMachine.h"


int LibSensors_init(void);
void LibSensors_cleanup(void);
int LibSensors_reload(void);

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs, unsigned int activeCPUs);

#endif /* HEADER_LibSensors */
