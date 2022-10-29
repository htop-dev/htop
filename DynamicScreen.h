#ifndef HEADER_DynamicScreen
#define HEADER_DynamicScreen

#include <stdbool.h>

#include "Hashtable.h"
#include "Settings.h"


typedef struct DynamicScreen_ {
   char name[32];  /* unique name, cannot contain spaces */
   char* caption;
   char* fields;
   char* sortKey;
   int direction;
} DynamicScreen;

Hashtable* DynamicScreens_new(Settings* settings);

void DynamicScreens_delete(Hashtable* dynamics);

const char* DynamicScreen_lookup(Hashtable* dynamics, unsigned int key);

bool DynamicScreen_search(Hashtable* dynamics, const char* name, unsigned int* key);

void DynamicScreen_availableColumns(char* currentScreen);

#endif
