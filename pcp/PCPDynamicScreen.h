#ifndef HEADER_PCPDynamicScreen
#define HEADER_PCPDynamicScreen
/*
htop - PCPDynamicScreen.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>
#include <stdbool.h>

#include "CRT.h"
#include "DynamicScreen.h"
#include "Hashtable.h"
#include "Machine.h"
#include "Panel.h"
#include "Settings.h"


struct InDomTable_;
struct PCPDynamicColumn_;
struct PCPDynamicColumns_;

typedef struct PCPDynamicScreen_ {
   DynamicScreen super;

   struct InDomTable_* table;
   struct PCPDynamicColumn_** columns;
   size_t totalColumns;

   unsigned int indom;  /* instance domain number */
   unsigned int key;  /* PCPMetric identifier */

   bool defaultEnabled; /* enabled setting from configuration file */
   /* at runtime enabled screens have entries in settings->screens */
} PCPDynamicScreen;

typedef struct PCPDynamicScreens_ {
   Hashtable* table;
   size_t count;  /* count of dynamic screens discovered from scan */
} PCPDynamicScreens;

void PCPDynamicScreens_init(PCPDynamicScreens* screens, struct PCPDynamicColumns_* columns);

void PCPDynamicScreens_done(Hashtable* table);

void PCPDynamicScreen_appendTables(PCPDynamicScreens* screens, Machine* host);

void PCPDynamicScreen_appendScreens(PCPDynamicScreens* screens, Settings* settings);

void PCPDynamicScreen_addDynamicScreen(PCPDynamicScreens* screens, ScreenSettings* ss);

void PCPDynamicScreens_addAvailableColumns(Panel* availableColumns, Hashtable* screens, const char* screen);

#endif
