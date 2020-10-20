#ifndef HEADER_Battery
#define HEADER_Battery
/*
htop - freebsd/Battery.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "BatteryMeter.h"

void Battery_getData(double* level, ACPresence* isOnAC);

#endif
