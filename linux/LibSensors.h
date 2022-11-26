#ifndef HEADER_LibSensors
#define HEADER_LibSensors

#include "Meter.h"

#include "linux/LinuxProcessList.h"


int LibSensors_init(void);
void LibSensors_cleanup(void);
int LibSensors_reload(void);

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs, unsigned int activeCPUs);

char** LibSensors_getTempChoices(ATTR_UNUSED Meter* meter);
void LibSensors_getTemp(const char* choice, double* currTemp, double* maxTemp);

char** LibSensors_getFanChoices(ATTR_UNUSED Meter* meter);
void LibSensors_getFan(const char* choice, double* curr, double* min, double* max);

#endif /* HEADER_LibSensors */
