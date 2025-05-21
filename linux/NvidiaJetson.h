#ifndef HEADER_NVIDIA_JETSON
#define HEADER_NVIDIA_JETSON

#include "config.h"
#ifdef NVIDIA_JETSON
#include "Meter.h"
#include "linux/LinuxMachine.h"

void NvidiaJetson_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs);
void NvidiaJetson_FindSensors(void);
extern const MeterClass JetsonGPUMeter_class;
#endif

#endif
