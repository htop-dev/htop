#ifndef HEADER_PCPDynamicColumn
#define HEADER_PCPDynamicColumn

#include <stddef.h>

#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Process.h"
#include "RichString.h"

#include "pcp/PCPProcess.h"


typedef struct PCPDynamicColumn_ {
   DynamicColumn super;
   char* metricName;
   size_t id;  /* identifier for metric array lookups */
} PCPDynamicColumn;

typedef struct PCPDynamicColumns_ {
   Hashtable* table;
   size_t count;  /* count of dynamic meters discovered by scan */
   size_t offset; /* start offset into the Platform metric array */
   size_t cursor; /* identifier allocator for each new metric used */
} PCPDynamicColumns;

void PCPDynamicColumns_init(PCPDynamicColumns* columns);

void PCPDynamicColumns_done(Hashtable* table);

void PCPDynamicColumn_writeField(PCPDynamicColumn* this, const Process* proc, RichString* str);

int PCPDynamicColumn_compareByKey(const PCPProcess* p1, const PCPProcess* p2, ProcessField key);

#endif
