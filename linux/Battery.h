#ifndef HEADER_Battery
#define HEADER_Battery
/*
htop - linux/Battery.h
(C) 2004-2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.

Linux battery readings written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "BatteryMeter.h"


void Battery_getData(double* level, ACPresence* isOnAC);

#endif
