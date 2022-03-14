#ifndef HEADER_ScreensPanel
#define HEADER_ScreensPanel
/*
htop - ScreensPanel.h
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2022 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "ColumnsPanel.h"
#include "ListItem.h"
#include "Object.h"
#include "Panel.h"
#include "ScreenManager.h"
#include "Settings.h"

#ifndef SCREEN_NAME_LEN
#define SCREEN_NAME_LEN 20
#endif

typedef struct ScreensPanel_ {
   Panel super;

   ScreenManager* scr;
   Settings* settings;
   ColumnsPanel* columns;
   char buffer[SCREEN_NAME_LEN + 1];
   char* saved;
   int cursor;
   bool moving;
   bool renaming;
} ScreensPanel;

typedef struct ScreenListItem_ {
   ListItem super;
   ScreenSettings* ss;
} ScreenListItem;


extern ObjectClass ScreenListItem_class;

ScreenListItem* ScreenListItem_new(const char* value, ScreenSettings* ss);

extern PanelClass ScreensPanel_class;

ScreensPanel* ScreensPanel_new(Settings* settings);

void ScreensPanel_update(Panel* super);

#endif
