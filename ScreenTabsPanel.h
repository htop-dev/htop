#ifndef HEADER_ScreenTabsPanel
#define HEADER_ScreenTabsPanel
/*
htop - ScreenTabsPanel.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "DynamicScreen.h"
#include "ListItem.h"
#include "Object.h"
#include "Panel.h"
#include "ScreensPanel.h"
#include "ScreenManager.h"
#include "Settings.h"


typedef struct ScreenNamesPanel_ {
   Panel super;

   ScreenManager* scr;
   Settings* settings;
   char buffer[SCREEN_NAME_LEN + 1];
   DynamicScreen* ds;
   char* saved;
   int cursor;
   ListItem* renamingItem;
} ScreenNamesPanel;

typedef struct ScreenNameListItem_ {
   ListItem super;
   ScreenSettings* ss;
} ScreenNameListItem;

typedef struct ScreenTabsPanel_ {
   Panel super;

   ScreenManager* scr;
   Settings* settings;
   ScreenNamesPanel* names;
   int cursor;
} ScreenTabsPanel;

typedef struct ScreenTabListItem_ {
   ListItem super;
   DynamicScreen* ds;
} ScreenTabListItem;


ScreenTabsPanel* ScreenTabsPanel_new(Settings* settings);

extern ObjectClass ScreenNameListItem_class;

ScreenNameListItem* ScreenNameListItem_new(const char* value, ScreenSettings* ss);

extern PanelClass ScreenNamesPanel_class;

ScreenNamesPanel* ScreenNamesPanel_new(Settings* settings);

#endif
