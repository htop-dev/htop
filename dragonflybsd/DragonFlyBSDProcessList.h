#ifndef HEADER_DragonFlyBSDProcessList
#define HEADER_DragonFlyBSDProcessList
/*
htop - DragonFlyBSDProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/param.h>

#include "ProcessList.h"


typedef struct DragonFlyBSDProcessList_ {
   ProcessList super;
} DragonFlyBSDProcessList;

#endif
