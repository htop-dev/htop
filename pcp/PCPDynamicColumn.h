#ifndef HEADER_PCPDynamicColumn
#define HEADER_PCPDynamicColumn

#include "CRT.h"
#include "DynamicColumn.h"
#include "Process.h"
#include "RichString.h"
#include "PCPProcess.h"

typedef struct {
   unsigned int id;
   ColorElements color;
   char* name;
   char* label;
   char* suffix;
} PCPDynamicColumnMetric;

typedef struct {
   DynamicColumn super;
   PCPDynamicColumnMetric* metrics;
   unsigned int totalMetrics;
} PCPDynamicColumn;

typedef struct {
   Hashtable* table;
   unsigned int count;
   unsigned int offset;
   unsigned int cursor;
} PCPDynamicColumns;

void PCPDynamicColumns_init(PCPDynamicColumns* columns);

void PCPDynamicColumn_writeField(PCPDynamicColumn* this, const Process* proc, RichString* str, int param);

int PCPDynamicColumn_compareByKey(const PCPProcess* p1, const PCPProcess* p2, ProcessField key);

#endif
