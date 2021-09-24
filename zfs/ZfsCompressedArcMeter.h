#ifndef HEADER_ZfsCompressedArcMeter
#define HEADER_ZfsCompressedArcMeter
/*
htop - ZfsCompressedArcMeter.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "zfs/ZfsArcStats.h"

#include "Meter.h"


void ZfsCompressedArcMeter_readStats(Meter* this, const ZfsArcStats* stats);

extern const MeterClass ZfsCompressedArcMeter_class;

#endif
