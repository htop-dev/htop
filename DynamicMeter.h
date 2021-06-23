#ifndef HEADER_DynamicMeter
#define HEADER_DynamicMeter

#include "Meter.h"


typedef struct DynamicMeter_ {
   char name[32];  /* unique name, cannot contain spaces */
   char* caption;
   char* description;
   unsigned int type;
   double maximum;

   void* dynamicData;  /* platform-specific meter data */
} DynamicMeter;

Hashtable* DynamicMeters_new(void);

const char* DynamicMeter_lookup(const ProcessList* pl, unsigned int param);

unsigned int DynamicMeter_search(const ProcessList* pl, const char* name);

extern const MeterClass DynamicMeter_class;

#endif
