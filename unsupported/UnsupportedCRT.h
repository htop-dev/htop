#ifndef HEADER_UnsupportedCRT
#define HEADER_UnsupportedCRT
/*
htop - UnsupportedCRT.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"

void CRT_handleSIGSEGV(int sgn) ATTR_NORETURN;

#endif
