#ifndef HEADER_ZfsArcMeter
#define HEADER_ZfsArcMeter
/*
htop - ZfsArcMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ZfsArcStats.h"

#include "Meter.h"

extern int ZfsArcMeter_attributes[];

void ZfsArcMeter_readStats(Meter* this, ZfsArcStats* stats);

extern MeterClass ZfsArcMeter_class;

#endif
