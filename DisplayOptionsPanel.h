#ifndef HEADER_DisplayOptionsPanel
#define HEADER_DisplayOptionsPanel
/*
htop - DisplayOptionsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "ScreenManager.h"
#include "Settings.h"


typedef struct DisplayOptionsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} DisplayOptionsPanel;

extern const PanelClass DisplayOptionsPanel_class;

DisplayOptionsPanel* DisplayOptionsPanel_new(Settings* settings, ScreenManager* scr);

#endif
