#ifndef HEADER_PCPDynamicColumn
#define HEADER_PCPDynamicColumn
/*
htop - PCPDynamicColumn.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Process.h"
#include "RichString.h"

#include "pcp/PCPProcess.h"


struct pmDesc;

typedef struct PCPDynamicColumn_ {
   DynamicColumn super;
   char* metricName;
   char* format;
   size_t id;  /* identifier for metric array lookups */
   int width;  /* optional width from configuration file */
   bool defaultEnabled;  /* default enabled in dynamic screen */
   bool percent;
   bool instances;  /* an instance *names* column, not values */
} PCPDynamicColumn;

typedef struct PCPDynamicColumns_ {
   Hashtable* table;
   size_t count;  /* count of dynamic columns discovered by scan */
   size_t offset; /* start offset into the Platform metric array */
   size_t cursor; /* identifier allocator for each new metric used */
} PCPDynamicColumns;

void PCPDynamicColumns_init(PCPDynamicColumns* columns);

void PCPDynamicColumns_done(Hashtable* table);

void PCPDynamicColumns_setupWidths(PCPDynamicColumns* columns);

void PCPDynamicColumn_writeField(PCPDynamicColumn* this, const Process* proc, RichString* str);

void PCPDynamicColumn_writeAtomValue(PCPDynamicColumn* column, RichString* str, const struct Settings_* settings, int metric, int instance, const struct pmDesc* desc, const void* atomvalue);

int PCPDynamicColumn_compareByKey(const PCPProcess* p1, const PCPProcess* p2, ProcessField key);

void PCPDynamicColumn_done(PCPDynamicColumn* this);

#endif
