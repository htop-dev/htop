#ifndef HEADER_ScreenManager
#define HEADER_ScreenManager
/*
htop - ScreenManager.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Action.h"
#include "Header.h"
#include "Machine.h"
#include "Panel.h"
#include "Vector.h"


typedef struct ScreenManager_ {
   int x1;
   int y1;
   int x2;
   int y2;
   Vector* panels;
   const char* name;
   int panelCount;
   Header* header;
   Machine* host;
   State* state;
   bool allowFocusChange;
} ScreenManager;

ScreenManager* ScreenManager_new(Header* header, Machine* host, State* state, bool owner);

void ScreenManager_delete(ScreenManager* this);

int ScreenManager_size(const ScreenManager* this);

void ScreenManager_add(ScreenManager* this, Panel* item, int size);

void ScreenManager_insert(ScreenManager* this, Panel* item, int size, int idx);

Panel* ScreenManager_remove(ScreenManager* this, int idx);

void ScreenManager_resize(ScreenManager* this);

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey, const char* name);

#endif
