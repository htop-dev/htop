#ifndef HEADER_IncSet
#define HEADER_IncSet
/*
htop - IncSet.h
(C) 2005-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "FunctionBar.h"
#include "Panel.h"
#include <stdbool.h>

#define INCMODE_MAX 40

typedef enum {
   INC_SEARCH = 0,
   INC_FILTER = 1
} IncType;

#define IncSet_filter(inc_) (inc_->filtering ? inc_->modes[INC_FILTER].buffer : NULL)

typedef struct IncMode_ {
   char buffer[INCMODE_MAX+1];
   int index;
   FunctionBar* bar;
   bool isFilter;
} IncMode;

typedef struct IncSet_ {
   IncMode modes[2];
   IncMode* active;
   FunctionBar* defaultBar;
   bool filtering;
   bool found;
} IncSet;

typedef const char* (*IncMode_GetPanelValue)(Panel*, int);

void IncSet_reset(IncSet* this, IncType type);

IncSet* IncSet_new(FunctionBar* bar);

void IncSet_delete(IncSet* this);

bool IncSet_next(IncSet* this, IncType type, Panel* panel, IncMode_GetPanelValue getPanelValue);

bool IncSet_prev(IncSet* this, IncType type, Panel* panel, IncMode_GetPanelValue getPanelValue);

bool IncSet_handleKey(IncSet* this, int ch, Panel* panel, IncMode_GetPanelValue getPanelValue, Vector* lines);

const char* IncSet_getListItemValue(Panel* panel, int i);

void IncSet_activate(IncSet* this, IncType type, Panel* panel);

void IncSet_drawBar(IncSet* this);

int IncSet_synthesizeEvent(IncSet* this, int x);

#endif
