#ifndef HEADER_LibSensors
#define HEADER_LibSensors

#include "config.h" // IWYU pragma: keep

#include <stdio.h>

#include "LinuxProcessList.h"


int LibSensors_init(FILE* input);
void LibSensors_cleanup(void);

int LibSensors_getCPUTemperatures(CPUData* cpus, int cpuCount);

#endif /* HEADER_LibSensors */
