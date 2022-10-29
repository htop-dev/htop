#ifndef HEADER_PCPDynamicScreen
#define HEADER_PCPDynamicScreen

#include <stddef.h>

#include <stdbool.h>

#include "CRT.h"
#include "DynamicScreen.h"
#include "Hashtable.h"
#include "Settings.h"

#include "pcp/PCPDynamicColumn.h"


typedef struct PCPDynamicScreen_ {
   DynamicScreen super;
   PCPDynamicColumn* columns;
   size_t totalColumns;
   char* instances;
   bool enabled;
} PCPDynamicScreen;

typedef struct PCPDynamicScreens_ {
   Hashtable* table;
   size_t count;  /* count of dynamic screens discovered by scan */
} PCPDynamicScreens;

void PCPDynamicScreens_appendDynamicColumns(PCPDynamicScreens* screens, PCPDynamicColumns* columns);

void PCPDynamicScreens_init(PCPDynamicScreens* screens);

void PCPDynamicScreens_done(Hashtable* table);

void PCPDynamicScreen_appendScreens(PCPDynamicScreens* screens, Settings* settings);

void PCPDynamicScreens_availableColumns(Hashtable* screens, char* currentScreen);

#endif
