#ifndef HEADER_LibSensors
#define HEADER_LibSensors

#include "linux/LinuxProcessList.h"


int LibSensors_init(void);
void LibSensors_cleanup(void);
int LibSensors_reload(void);

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs, unsigned int activeCPUs);

#endif /* HEADER_LibSensors */
