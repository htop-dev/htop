#ifndef HEADER_FreeBSDCRT
#define HEADER_FreeBSDCRT
/*
htop - FreeBSDCRT.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"

void CRT_handleSIGSEGV(int sgn) ATTR_NORETURN;

#endif
