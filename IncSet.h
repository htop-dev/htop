#ifndef HEADER_IncSet
#define HEADER_IncSet
/*
htop - IncSet.h
(C) 2005-2012 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>

#include "FunctionBar.h"
#include "History.h"
#include "LineEditor.h"
#include "Panel.h"
#include "Vector.h"

typedef enum {
   INC_SEARCH = 0,
   INC_FILTER = 1
} IncType;

typedef struct IncMode_ {
   LineEditor editor;
   FunctionBar* bar;
   bool isFilter;
} IncMode;

typedef struct IncSet_ {
   IncMode modes[2];
   IncMode* active;
   Panel* panel;
   FunctionBar* defaultBar;
   bool filtering;
   bool found;
   History* history;  /* shared history for search and filter; may be NULL */
} IncSet;

static inline const char* IncSet_filter(IncSet* this) {
   return this->filtering ? LineEditor_getText(&this->modes[INC_FILTER].editor) : NULL;
}

void IncSet_setFilter(IncSet* this, const char* filter);

typedef const char* (*IncMode_GetPanelValue)(Panel*, int);

void IncSet_reset(IncSet* this, IncType type);

IncSet* IncSet_new(FunctionBar* bar);

void IncSet_delete(IncSet* this);

/* Set the history file path (creates/loads history from the given file).
   Call this after IncSet_new when the settings path is available. */
void IncSet_setHistoryFile(IncSet* this, const char* filename);

/* Save the history to disk (noop if no history file was set) */
void IncSet_saveHistory(const IncSet* this);

bool IncSet_handleKey(IncSet* this, int ch, Panel* panel, IncMode_GetPanelValue getPanelValue, Vector* lines);

const char* IncSet_getListItemValue(Panel* panel, int i);

void IncSet_activate(IncSet* this, IncType type, Panel* panel);

void IncSet_drawBar(const IncSet* this, int attr);

int IncSet_synthesizeEvent(IncSet* this, int x);

#endif
