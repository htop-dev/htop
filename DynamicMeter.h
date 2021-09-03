#ifndef HEADER_DynamicMeter
#define HEADER_DynamicMeter

#include <stdbool.h>

#include "Hashtable.h"
#include "Meter.h"


typedef struct DynamicMeter_ {
   char name[32];  /* unique name, cannot contain spaces */
   char* caption;
   char* description;
   unsigned int type;
   double maximum;
} DynamicMeter;

Hashtable* DynamicMeters_new(void);

void DynamicMeters_delete(Hashtable* dynamics);

const char* DynamicMeter_lookup(Hashtable* dynamics, unsigned int key);

bool DynamicMeter_search(Hashtable* dynamics, const char* name, unsigned int* key);

extern const MeterClass DynamicMeter_class;

#endif
