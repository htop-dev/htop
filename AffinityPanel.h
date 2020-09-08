#ifndef HEADER_AffinityPanel
#define HEADER_AffinityPanel
/*
htop - AffinityPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "Affinity.h"
#include "ProcessList.h"

extern PanelClass AffinityPanel_class;

Panel* AffinityPanel_new(ProcessList* pl, Affinity* affinity, int* width);

Affinity* AffinityPanel_getAffinity(Panel* super, ProcessList* pl);

#endif
