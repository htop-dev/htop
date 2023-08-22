#ifndef HEADER_MainPanel
#define HEADER_MainPanel
/*
htop - ColumnsPanel.h
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <sys/types.h>

#include "Action.h"
#include "IncSet.h"
#include "Object.h"
#include "Panel.h"
#include "Row.h"


typedef struct MainPanel_ {
   Panel super;
   State* state;
   IncSet* inc;
   Htop_Action* keys;
   unsigned int idSearch;
} MainPanel;

typedef bool(*MainPanel_foreachRowFn)(Row*, Arg);

#define MainPanel_getFunctionBar(this_) (((Panel*)(this_))->defaultBar)

// update the Label Keys in the MainPanel bar, list: list / tree mode, filter: filter (inc) active / inactive
void MainPanel_updateLabels(MainPanel* this, bool list, bool filter);

int MainPanel_selectedRow(MainPanel* this);

bool MainPanel_foreachRow(MainPanel* this, MainPanel_foreachRowFn fn, Arg arg, bool* wasAnyTagged);

extern const PanelClass MainPanel_class;

MainPanel* MainPanel_new(void);

void MainPanel_setState(MainPanel* this, State* state);

void MainPanel_delete(Object* object);

#endif
