#ifndef HEADER_Action
#define HEADER_Action
/*
htop - Action.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <sys/types.h>

#include "Header.h"
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "ProcessList.h"
#include "Settings.h"
#include "UsersTable.h"


typedef enum {
   HTOP_OK              = 0x00,
   HTOP_REFRESH         = 0x01,
   HTOP_RECALCULATE     = 0x02 | HTOP_REFRESH,
   HTOP_SAVE_SETTINGS   = 0x04,
   HTOP_KEEP_FOLLOWING  = 0x08,
   HTOP_QUIT            = 0x10,
   HTOP_REDRAW_BAR      = 0x20,
   HTOP_UPDATE_PANELHDR = 0x40 | HTOP_REFRESH,
   HTOP_RESIZE          = 0x80 | HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR,
} Htop_Reaction;

struct MainPanel_; // IWYU pragma: keep

typedef struct State_ {
   Settings* settings;
   UsersTable* ut;
   ProcessList* pl;
   struct MainPanel_* mainPanel;
   Header* header;
   bool pauseProcessUpdate;
   bool hideProcessSelection;
} State;

static inline bool State_hideFunctionBar(const State* st) {
   return st->settings->hideFunctionBar == 2 || (st->settings->hideFunctionBar == 1 && st->hideProcessSelection);
}

typedef Htop_Reaction (*Htop_Action)(State* st);

Object* Action_pickFromVector(State* st, Panel* list, int x, bool followProcess);

bool Action_setUserOnly(const char* userName, uid_t* userId);

Htop_Reaction Action_setSortKey(Settings* settings, ProcessField sortKey);

Htop_Reaction Action_setScreenTab(Settings* settings, int x);

Htop_Reaction Action_follow(State* st);

void Action_setBindings(Htop_Action* keys);

#endif
