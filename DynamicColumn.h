#ifndef HEADER_DynamicColumn
#define HEADER_DynamicColumn

#include <stdbool.h>

#include "Hashtable.h"
#include "Process.h"
#include "RichString.h"


#define DYNAMIC_MAX_COLUMN_WIDTH 28
#define DYNAMIC_DEFAULT_COLUMN_WIDTH -5

typedef struct DynamicColumn_ {
   char name[32];     /* unique, internal-only name */
   char* heading;     /* displayed in main screen */
   char* caption;     /* displayed in setup menu (short name) */
   char* description; /* displayed in setup menu (detail) */
   int width;         /* display width +/- for value alignment */
} DynamicColumn;

Hashtable* DynamicColumns_new(void);

void DynamicColumns_delete(Hashtable* dynamics);

const char* DynamicColumn_init(unsigned int key);

const DynamicColumn* DynamicColumn_lookup(Hashtable* dynamics, unsigned int key);

const DynamicColumn* DynamicColumn_search(Hashtable* dynamics, const char* name, unsigned int* key);

bool DynamicColumn_writeField(const Process* proc, RichString* str, unsigned int key);

#endif
