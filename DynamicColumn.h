#ifndef HEADER_DynamicColumn
#define HEADER_DynamicColumn
/*
htop - DynamicColumn.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Hashtable.h"
#include "Process.h"
#include "RichString.h"
#include "Table.h"


#define DYNAMIC_MAX_COLUMN_WIDTH 64
#define DYNAMIC_DEFAULT_COLUMN_WIDTH -5

typedef struct DynamicColumn_ {
   char name[32];           /* unique, internal-only name */
   char* heading;           /* displayed in main screen */
   char* caption;           /* displayed in setup menu (short name) */
   char* description;       /* displayed in setup menu (detail) */
   int width;               /* display width +/- for value alignment */
   bool enabled;            /* false == ignore this column (until enabled) */
   Table* table;            /* pointer to DynamicScreen or ProcessTable */
} DynamicColumn;

Hashtable* DynamicColumns_new(void);

void DynamicColumns_delete(Hashtable* dynamics);

const char* DynamicColumn_name(unsigned int key);

void DynamicColumn_done(DynamicColumn* this);

const DynamicColumn* DynamicColumn_lookup(Hashtable* dynamics, unsigned int key);

const DynamicColumn* DynamicColumn_search(Hashtable* dynamics, const char* name, unsigned int* key);

bool DynamicColumn_writeField(const Process* proc, RichString* str, unsigned int key);

#endif
