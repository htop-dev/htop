#ifndef HEADER_HeaderOptionsPanel
#define HEADER_HeaderOptionsPanel
/*
htop - ColorsPanel.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "ScreenManager.h"
#include "Settings.h"


typedef struct HeaderOptionsPanel_ {
   Panel super;

   ScreenManager* scr;
   Settings* settings;
} HeaderOptionsPanel;

extern const PanelClass HeaderOptionsPanel_class;

HeaderOptionsPanel* HeaderOptionsPanel_new(Settings* settings, ScreenManager* scr);

#endif /* HEADER_HeaderOptionsPanel */
