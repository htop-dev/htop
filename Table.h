#ifndef HEADER_Table
#define HEADER_Table
/*
htop - Table.h
(C) 2004,2005 Hisham H. Muhammad
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Hashtable.h"
#include "Object.h"
#include "RichString.h"
#include "Settings.h"
#include "Vector.h"


struct Machine_;  // IWYU pragma: keep
struct Panel_;    // IWYU pragma: keep
struct Row_;      // IWYU pragma: keep

typedef struct Table_ {
   /* Super object for emulated OOP */
   Object super;

   Vector* rows;          /* all known; sort order can vary and differ from display order */
   Vector* displayList;   /* row tree flattened in display order (borrowed);
                             updated in Table_updateDisplayList when rebuilding panel */
   Hashtable* table;      /* fast known row lookup by identifier */

   struct Machine_* host;
   const char* incFilter;
   bool needsSort;
   int following;         /* -1 or row being visually tracked in the user interface */

   struct Panel_* panel;
} Table;

typedef Table* (*Table_New)(const struct Machine_*);
typedef void (*Table_ScanPrepare)(Table* this);
typedef void (*Table_ScanIterate)(Table* this);
typedef void (*Table_ScanCleanup)(Table* this);

typedef struct TableClass_ {
   const ObjectClass super;
   const Table_ScanPrepare prepare;
   const Table_ScanIterate iterate;
   const Table_ScanCleanup cleanup;
} TableClass;

#define As_Table(this_)  ((const TableClass*)((this_)->super.klass))

#define Table_scanPrepare(t_)  (As_Table(t_)->prepare ? (As_Table(t_)->prepare(t_)) : Table_prepareEntries(t_))
#define Table_scanIterate(t_)  (As_Table(t_)->iterate(t_))  /* mandatory; must have a custom iterate method */
#define Table_scanCleanup(t_)  (As_Table(t_)->cleanup ? (As_Table(t_)->cleanup(t_)) : Table_cleanupEntries(t_))

Table* Table_init(Table* this, const ObjectClass* klass, struct Machine_* host);

void Table_done(Table* this);

extern const TableClass Table_class;

void Table_setPanel(Table* this, struct Panel_* panel);

void Table_printHeader(const Settings* settings, RichString* header);

void Table_add(Table* this, struct Row_* row);

void Table_removeIndex(Table* this, const struct Row_* row, int idx);

void Table_updateDisplayList(Table* this);

void Table_expandTree(Table* this);

void Table_collapseAllBranches(Table* this);

void Table_rebuildPanel(Table* this);

static inline struct Row_* Table_findRow(Table* this, int id) {
   return (struct Row_*) Hashtable_get(this->table, id);
}

void Table_prepareEntries(Table* this);

void Table_cleanupEntries(Table* this);

void Table_cleanupRow(Table* this, Row* row, int idx);

static inline void Table_compact(Table* this) {
   Vector_compact(this->rows);
}

#endif
