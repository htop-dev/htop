#ifndef HEADER_IncSet
#define HEADER_IncSet
/*
htop - IncSet.h
(C) 2005-2012 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>

#include "FunctionBar.h"
#include "Panel.h"
#include "Vector.h"


#define INCMODE_MAX 128

typedef enum {
   INC_SEARCH = 0,
   INC_FILTER = 1
} IncType;

typedef struct IncMode_ {
   char buffer[INCMODE_MAX + 1];
   int index;
   FunctionBar* bar;
   bool isFilter;
} IncMode;

typedef struct IncSet_ {
   IncMode modes[2];
   IncMode* active;
   Panel* panel;
   FunctionBar* defaultBar;
   bool filtering;
   bool found;
} IncSet;

static inline const char* IncSet_filter(const IncSet* this) {
   return this->filtering ? this->modes[INC_FILTER].buffer : NULL;
}

void IncSet_setFilter(IncSet* this, const char* filter);

typedef const char* (*IncMode_GetPanelValue)(Panel*, int);

void IncSet_reset(IncSet* this, IncType type);

IncSet* IncSet_new(FunctionBar* bar);

void IncSet_delete(IncSet* this);

bool IncSet_handleKey(IncSet* this, int ch, Panel* panel, IncMode_GetPanelValue getPanelValue, Vector* lines);

const char* IncSet_getListItemValue(Panel* panel, int i);

void IncSet_activate(IncSet* this, IncType type, Panel* panel);

void IncSet_drawBar(const IncSet* this, int attr);

int IncSet_synthesizeEvent(IncSet* this, int x);

#endif
