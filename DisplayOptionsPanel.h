#ifndef HEADER_DisplayOptionsPanel
#define HEADER_DisplayOptionsPanel
/*
htop - DisplayOptionsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct DisplayOptionsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} DisplayOptionsPanel;

extern PanelClass DisplayOptionsPanel_class;

DisplayOptionsPanel* DisplayOptionsPanel_new(Settings* settings, ScreenManager* scr);

#endif
