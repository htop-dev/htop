#ifndef HEADER_DynamicScreen
#define HEADER_DynamicScreen
/*
htop - DynamicColumn.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Hashtable.h"
#include "Panel.h"


typedef struct DynamicScreen_ {
   char name[32];  /* unique name cannot contain any spaces */
   char* heading;  /* user-settable more readable name */
   char* caption;  /* explanatory text for screen */
   char* fields;
   char* sortKey;
   char* columnKeys;
   int direction;
} DynamicScreen;

Hashtable* DynamicScreens_new(void);

void DynamicScreens_delete(Hashtable* dynamics);

void DynamicScreen_done(DynamicScreen* this);

void DynamicScreens_addAvailableColumns(Panel* availableColumns, char* screen);

const char* DynamicScreen_lookup(Hashtable* screens, unsigned int key);

bool DynamicScreen_search(Hashtable* screens, const char* name, unsigned int* key);

#endif
