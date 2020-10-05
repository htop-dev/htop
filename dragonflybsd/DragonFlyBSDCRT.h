#ifndef HEADER_DragonFlyBSDCRT
#define HEADER_DragonFlyBSDCRT
/*
htop - dragonflybsd/DragonFlyBSDCRT.h
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"

void CRT_handleSIGSEGV(int sgn) ATTR_NORETURN;

#endif
