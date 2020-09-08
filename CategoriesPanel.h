#ifndef HEADER_CategoriesPanel
#define HEADER_CategoriesPanel
/*
htop - CategoriesPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"
#include "ProcessList.h"

typedef struct CategoriesPanel_ {
   Panel super;
   ScreenManager* scr;

   Settings* settings;
   Header* header;
   ProcessList* pl;
} CategoriesPanel;

void CategoriesPanel_makeMetersPage(CategoriesPanel* this);

extern PanelClass CategoriesPanel_class;

CategoriesPanel* CategoriesPanel_new(ScreenManager* scr, Settings* settings, Header* header, ProcessList* pl);

#endif
