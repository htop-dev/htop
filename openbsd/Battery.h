#ifndef HEADER_Battery
#define HEADER_Battery
/*
htop - openbsd/Battery.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "BatteryMeter.h"

void Battery_getData(double* level, ACPresence* isOnAC);

#endif
