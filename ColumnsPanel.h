#ifndef HEADER_ColumnsPanel
#define HEADER_ColumnsPanel
/*
htop - ColumnsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Hashtable.h"
#include "Panel.h"
#include "Settings.h"


typedef struct ColumnsPanel_ {
   Panel super;
   ScreenSettings* ss;
   bool* changed;

   bool moving;
} ColumnsPanel;

extern const PanelClass ColumnsPanel_class;

ColumnsPanel* ColumnsPanel_new(ScreenSettings* ss, Hashtable* columns, bool* changed);

void ColumnsPanel_fill(ColumnsPanel* this, ScreenSettings* ss, Hashtable* columns);

void ColumnsPanel_update(Panel* super);

#endif
