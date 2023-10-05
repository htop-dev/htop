#ifndef HEADER_AffinityPanel
#define HEADER_AffinityPanel
/*
htop - AffinityPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Affinity.h"
#include "Machine.h"
#include "Panel.h"


extern const PanelClass AffinityPanel_class;

Panel* AffinityPanel_new(Machine* host, const Affinity* affinity, int* width);

Affinity* AffinityPanel_getAffinity(Panel* super, Machine* host);

#endif
