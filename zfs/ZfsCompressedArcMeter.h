#ifndef HEADER_ZfsCompressedArcMeter
#define HEADER_ZfsCompressedArcMeter
/*
htop - ZfsCompressedArcMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ZfsArcStats.h"

#include "Meter.h"

extern int ZfsCompressedArcMeter_attributes[];

void ZfsCompressedArcMeter_readStats(Meter* this, ZfsArcStats* stats);

extern MeterClass ZfsCompressedArcMeter_class;

#endif
