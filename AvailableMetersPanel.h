#ifndef HEADER_AvailableMetersPanel
#define HEADER_AvailableMetersPanel
/*
htop - AvailableMetersPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

#include "Header.h"
#include "Machine.h"
#include "MetersPanel.h"
#include "Panel.h"
#include "ScreenManager.h"


typedef struct AvailableMetersPanel_ {
   Panel super;
   ScreenManager* scr;
   Machine* host;
   Header* header;
   size_t columns;
   MetersPanel** meterPanels;
} AvailableMetersPanel;

extern const PanelClass AvailableMetersPanel_class;

AvailableMetersPanel* AvailableMetersPanel_new(Machine* host, Header* header, size_t columns, MetersPanel** meterPanels, ScreenManager* scr);

#endif
