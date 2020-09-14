#ifndef HEADER_MetersPanel
#define HEADER_MetersPanel
/*
htop - MetersPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct MetersPanel_ MetersPanel;

struct MetersPanel_ {
   Panel super;

   Settings* settings;
   Vector* meters;
   ScreenManager* scr;
   MetersPanel* leftNeighbor;
   MetersPanel* rightNeighbor;
   bool moving;
};

void MetersPanel_setMoving(MetersPanel* this, bool moving);

extern PanelClass MetersPanel_class;

MetersPanel* MetersPanel_new(Settings* settings, const char* header, Vector* meters, ScreenManager* scr);

#endif
