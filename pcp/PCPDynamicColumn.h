#ifndef HEADER_PCPDynamicColumn
#define HEADER_PCPDynamicColumn

#include <pcp/pmapi.h>
#include <stddef.h>

/* use htop config.h values for these macros, not pcp values */
#undef PACKAGE_URL
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT

#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Process.h"
#include "RichString.h"

#include "pcp/PCPProcess.h"


typedef struct PCPDynamicColumn_ {
   DynamicColumn super;
   char* metricName;
   char* format;
   size_t id;  /* identifier for metric array lookups */
   int width;  /* optional width from configuration file */
   bool percent;
   bool instances;
} PCPDynamicColumn;

typedef struct PCPDynamicColumns_ {
   Hashtable* table;
   size_t count;  /* count of dynamic meters discovered by scan */
   size_t offset; /* start offset into the Platform metric array */
   size_t cursor; /* identifier allocator for each new metric used */
} PCPDynamicColumns;

void PCPDynamicColumns_init(PCPDynamicColumns* columns);

void PCPDynamicColumns_done(Hashtable* table);

void PCPDynamicColumns_setupWidths(PCPDynamicColumns* columns);

void PCPDynamicColumn_writeField(PCPDynamicColumn* this, const Process* proc, RichString* str);

void PCPDynamicColumn_writeAtomValue(PCPDynamicColumn* column, RichString* str, const struct Settings_* settings, int metric, int instance, const pmDesc* desc, const pmAtomValue* atomvalue);

int PCPDynamicColumn_compareByKey(const PCPProcess* p1, const PCPProcess* p2, ProcessField key);

#endif
