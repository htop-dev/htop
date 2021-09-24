#ifndef HEADER_CategoriesPanel
#define HEADER_CategoriesPanel
/*
htop - CategoriesPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"
#include "Panel.h"
#include "ProcessList.h"
#include "ScreenManager.h"
#include "Settings.h"


typedef struct CategoriesPanel_ {
   Panel super;
   ScreenManager* scr;

   Settings* settings;
   Header* header;
   ProcessList* pl;
} CategoriesPanel;

extern const PanelClass CategoriesPanel_class;

CategoriesPanel* CategoriesPanel_new(ScreenManager* scr, Settings* settings, Header* header, ProcessList* pl);

#endif
