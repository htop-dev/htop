#ifndef HEADER_AvailableMetersPanel
#define HEADER_AvailableMetersPanel
/*
htop - AvailableMetersPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"
#include "Panel.h"
#include "ScreenManager.h"
#include "ProcessList.h"

typedef struct AvailableMetersPanel_ {
   Panel super;
   ScreenManager* scr;

   Settings* settings;
   Header* header;
   Panel* leftPanel;
   Panel* rightPanel;
} AvailableMetersPanel;

extern PanelClass AvailableMetersPanel_class;

AvailableMetersPanel* AvailableMetersPanel_new(Settings* settings, Header* header, Panel* leftMeters, Panel* rightMeters, ScreenManager* scr, ProcessList* pl);

#endif
