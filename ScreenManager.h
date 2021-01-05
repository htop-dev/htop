#ifndef HEADER_ScreenManager
#define HEADER_ScreenManager
/*
htop - ScreenManager.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Action.h"
#include "Header.h"
#include "Panel.h"
#include "Settings.h"
#include "Vector.h"


typedef struct ScreenManager_ {
   int x1;
   int y1;
   int x2;
   int y2;
   Vector* panels;
   int panelCount;
   Header* header;
   const Settings* settings;
   const State* state;
   bool owner;
   bool allowFocusChange;
} ScreenManager;

ScreenManager* ScreenManager_new(Header* header, const Settings* settings, const State* state, bool owner);

void ScreenManager_delete(ScreenManager* this);

int ScreenManager_size(const ScreenManager* this);

void ScreenManager_add(ScreenManager* this, Panel* item, int size);

Panel* ScreenManager_remove(ScreenManager* this, int idx);

void ScreenManager_resize(ScreenManager* this, int x1, int y1, int x2, int y2);

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey);

#endif
