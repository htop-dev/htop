#ifndef HEADER_MetersPanel
#define HEADER_MetersPanel
/*
htop - MetersPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Panel.h"
#include "ScreenManager.h"
#include "Settings.h"
#include "Vector.h"


struct MetersPanel_;
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

void MetersPanel_cleanup(void);

void MetersPanel_setMoving(MetersPanel* this, bool moving);

extern const PanelClass MetersPanel_class;

MetersPanel* MetersPanel_new(Settings* settings, const char* header, Vector* meters, ScreenManager* scr);

#endif
