#ifndef HEADER_ColumnsPanel
#define HEADER_ColumnsPanel
/*
htop - ColumnsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Panel.h"
#include "Settings.h"


typedef struct ColumnsPanel_ {
   Panel super;

   Settings* settings;
   bool moving;
} ColumnsPanel;

extern const PanelClass ColumnsPanel_class;

ColumnsPanel* ColumnsPanel_new(Settings* settings);

void ColumnsPanel_update(Panel* super);

#endif
