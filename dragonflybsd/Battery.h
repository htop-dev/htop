#ifndef HEADER_Battery
#define HEADER_Battery
/*
htop - dragonflybsd/Battery.h
(C) 2015 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

void Battery_getData(double* level, ACPresence* isOnAC);

#endif
