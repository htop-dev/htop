#ifndef HEADER_PCPDynamicMeter
#define HEADER_PCPDynamicMeter

#include "CRT.h"
#include "DynamicMeter.h"

typedef struct {
   unsigned int id; /* index into metric array */
   ColorElements color;
   char* name; /* derived metric name */
   char* label;
   char* suffix;
} PCPDynamicMetric;

typedef struct {
   DynamicMeter super;
   PCPDynamicMetric* metrics;
   unsigned int totalMetrics;
} PCPDynamicMeter;

typedef struct {
   Hashtable* table;
   unsigned int count; /* count of dynamic meters discovered by scan */
   unsigned int offset; /* start offset into the Platform metric array */
   unsigned int cursor; /* identifier allocator for each new metric used */
} PCPDynamicMeters;

void PCPDynamicMeters_init(PCPDynamicMeters* meters);

void PCPDynamicMeter_enable(PCPDynamicMeter* this);

void PCPDynamicMeter_updateValues(PCPDynamicMeter* this, Meter* meter);

void PCPDynamicMeter_display(PCPDynamicMeter* this, const Meter* meter, RichString* out);

#endif
