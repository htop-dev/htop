#ifndef HEADER_Battery
#define HEADER_Battery
/*
htop - openbsd/Battery.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

void Battery_getData(double* level, ACPresence* isOnAC);

#endif
