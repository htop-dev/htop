#ifndef HEADER_ScreenManager
#define HEADER_ScreenManager
/*
htop - ScreenManager.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Vector.h"
#include "Header.h"
#include "Settings.h"
#include "Panel.h"

typedef enum Orientation_ {
   VERTICAL,
   HORIZONTAL
} Orientation;

typedef struct ScreenManager_ {
   int x1;
   int y1;
   int x2;
   int y2;
   Orientation orientation;
   Vector* panels;
   int panelCount;
   const Header* header;
   const Settings* settings;
   bool owner;
   bool allowFocusChange;
} ScreenManager;

ScreenManager* ScreenManager_new(int x1, int y1, int x2, int y2, Orientation orientation, const Header* header, const Settings* settings, bool owner);

void ScreenManager_delete(ScreenManager* this);

int ScreenManager_size(ScreenManager* this);

void ScreenManager_add(ScreenManager* this, Panel* item, int size);

Panel* ScreenManager_remove(ScreenManager* this, int idx);

void ScreenManager_resize(ScreenManager* this, int x1, int y1, int x2, int y2);

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey);

#endif
