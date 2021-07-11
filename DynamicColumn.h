#ifndef HEADER_DynamicColumn
#define HEADER_DynamicColumn

#include "Hashtable.h"
#include "ProcessList.h"


typedef struct DynamicColumn_ {
   char name[32];  /* unique name, cannot contain spaces */
   char* caption;
   char* description;
   unsigned int type;
   double maximum;
   unsigned int width;

   void* dynamicData;  /* platform-specific Column data */
} DynamicColumn;

Hashtable* DynamicColumns_new(void);

const char* DynamicColumn_lookup(const ProcessList* pl, unsigned int param);

unsigned int DynamicColumn_search(const ProcessList* pl, const char* name);

void DynamicColumn_writeField(const Process* proc, RichString* str, int field);

#endif
