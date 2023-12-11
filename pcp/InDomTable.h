#ifndef HEADER_InDomTable
#define HEADER_InDomTable
/*
htop - InDomTable.h
(C) 2023 htop dev team
(C) 2022-2023 Sohaib Mohammed
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Platform.h"
#include "Table.h"


typedef struct InDomTable_ {
   Table super;
   pmInDom id;  /* shared by metrics in the table */
   unsigned int metricKey;  /* representative metric using this indom */
} InDomTable;

extern const TableClass InDomTable_class;

InDomTable* InDomTable_new(Machine* host, pmInDom indom, int metricKey);

void InDomTable_done(InDomTable* this);

RowField RowField_keyAt(const Settings* settings, int at);

void InDomTable_scan(Table* super);

#endif
