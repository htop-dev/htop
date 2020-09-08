#ifndef HEADER_IOPriorityPanel
#define HEADER_IOPriorityPanel
/*
htop - IOPriorityPanel.h
(C) 2004-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "IOPriority.h"
#include "ListItem.h"

Panel* IOPriorityPanel_new(IOPriority currPrio);

IOPriority IOPriorityPanel_getIOPriority(Panel* this);

#endif
