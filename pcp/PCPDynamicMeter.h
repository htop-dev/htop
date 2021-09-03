#ifndef HEADER_PCPDynamicMeter
#define HEADER_PCPDynamicMeter

#include <stddef.h>

#include "CRT.h"
#include "DynamicMeter.h"
#include "Hashtable.h"
#include "Meter.h"
#include "RichString.h"


typedef struct PCPDynamicMetric_ {
   size_t id; /* index into metric array */
   ColorElements color;
   char* name; /* derived metric name */
   char* label;
   char* suffix;
} PCPDynamicMetric;

typedef struct PCPDynamicMeter_ {
   DynamicMeter super;
   PCPDynamicMetric* metrics;
   size_t totalMetrics;
} PCPDynamicMeter;

typedef struct PCPDynamicMeters_ {
   Hashtable* table;
   size_t count;  /* count of dynamic meters discovered by scan */
   size_t offset; /* start offset into the Platform metric array */
   size_t cursor; /* identifier allocator for each new metric used */
} PCPDynamicMeters;

void PCPDynamicMeters_init(PCPDynamicMeters* meters);

void PCPDynamicMeters_done(Hashtable* table);

void PCPDynamicMeter_enable(PCPDynamicMeter* this);

void PCPDynamicMeter_updateValues(PCPDynamicMeter* this, Meter* meter);

void PCPDynamicMeter_display(PCPDynamicMeter* this, const Meter* meter, RichString* out);

#endif
