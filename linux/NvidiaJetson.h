#ifndef HEADER_NVIDIA_JETSON
#define HEADER_NVIDIA_JETSON

#include "Hashtable.h"
#include "Meter.h"

#include "linux/LinuxMachine.h"

void NvidiaJetson_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs);
void NvidiaJetson_FindSensors(void);

void NvidiaJetson_LoadGpuProcessTable(Hashtable *pidHash);
Hashtable *NvidiaJetson_GetPidMatchList(void);

extern const MeterClass JetsonGPUMeter_class;

#endif
