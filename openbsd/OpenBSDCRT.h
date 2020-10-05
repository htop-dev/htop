#ifndef HEADER_OpenBSDCRT
#define HEADER_OpenBSDCRT
/*
htop - OpenBSDCRT.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"

void CRT_handleSIGSEGV(int sgn) ATTR_NORETURN;

#endif
