#ifndef HEADER_ColorsPanel
#define HEADER_ColorsPanel
/*
htop - ColorsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct ColorsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} ColorsPanel;

extern PanelClass ColorsPanel_class;

ColorsPanel* ColorsPanel_new(Settings* settings, ScreenManager* scr);

#endif
