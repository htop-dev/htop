#ifndef HEADER_LibSensors
#define HEADER_LibSensors

#include "config.h" // IWYU pragma: keep

#include <stdio.h>

#include "LinuxProcessList.h"


int LibSensors_init(FILE* input);
void LibSensors_cleanup(void);

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int cpuCount);

#endif /* HEADER_LibSensors */
