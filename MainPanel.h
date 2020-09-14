#ifndef HEADER_MainPanel
#define HEADER_MainPanel
/*
htop - ColumnsPanel.h
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Action.h"
#include "IncSet.h"
#include "Panel.h"

typedef struct MainPanel_ {
   Panel super;
   State* state;
   IncSet* inc;
   Htop_Action *keys;
   pid_t pidSearch;
} MainPanel;

typedef bool(*MainPanel_ForeachProcessFn)(Process*, Arg);

#define MainPanel_getFunctionBar(this_) (((Panel*)(this_))->defaultBar)

void MainPanel_updateTreeFunctions(MainPanel* this, bool mode);

void MainPanel_pidSearch(MainPanel* this, int ch);

int MainPanel_selectedPid(MainPanel* this);

const char* MainPanel_getValue(MainPanel* this, int i);

bool MainPanel_foreachProcess(MainPanel* this, MainPanel_ForeachProcessFn fn, Arg arg, bool* wasAnyTagged);

extern PanelClass MainPanel_class;

MainPanel* MainPanel_new();

void MainPanel_setState(MainPanel* this, State* state);

void MainPanel_delete(Object* object);

#endif
