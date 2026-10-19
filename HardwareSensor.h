#ifndef HEADER_HardwareSensor
#define HEADER_HardwareSensor
/*
htop - HardwareSensor.h
(C) 2026 Massimo Mazzariol
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdint.h>


typedef enum HardwareSensorType_ {
   SENSOR_TEMPERATURE,
   SENSOR_FAN,
} HardwareSensorType;

typedef enum HardwareSensorLabelSource_ {
   SENSOR_LABEL_FALLBACK,
   SENSOR_LABEL_LIBSENSORS,
   SENSOR_LABEL_HTOP,
} HardwareSensorLabelSource;

typedef struct HardwareSensor_ {
   char* id;
   char* label;
   HardwareSensorLabelSource labelSource;
   HardwareSensorType type;
   double value;
   double min;
   double average;
   double max;
   uint64_t sampleCount;
} HardwareSensor;

#endif
