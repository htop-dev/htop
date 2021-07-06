#ifndef HEADER_LibSensors
#define HEADER_LibSensors

#include "config.h" // IWYU pragma: keep

#include <stdio.h>

#include "linux/LinuxProcessList.h"


int LibSensors_init(FILE* input);
void LibSensors_cleanup(void);

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int cpuCount);

char** LibSensors_getTempChoices(void);
void LibSensors_getTemp(const char* choice, double* currTemp, double* maxTemp);

char** LibSensors_getFanChoices(void);
void LibSensors_getFan(const char* choice, double* curr, double* min, double* max);

#endif /* HEADER_LibSensors */
