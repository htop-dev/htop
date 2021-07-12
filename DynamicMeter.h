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

const char* DynamicMeter_lookup(Hashtable* dynamics, unsigned int param);

bool DynamicMeter_search(Hashtable* dynamics, const char* name, unsigned int* key);

extern const MeterClass DynamicMeter_class;

#endif
