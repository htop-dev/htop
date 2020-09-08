#ifndef HEADER_AvailableColumnsPanel
#define HEADER_AvailableColumnsPanel
/*
htop - AvailableColumnsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"

typedef struct AvailableColumnsPanel_ {
   Panel super;
   Panel* columns;
} AvailableColumnsPanel;

extern PanelClass AvailableColumnsPanel_class;

AvailableColumnsPanel* AvailableColumnsPanel_new(Panel* columns);

#endif
