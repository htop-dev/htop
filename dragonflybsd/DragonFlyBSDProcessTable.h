#ifndef HEADER_DragonFlyBSDProcessTable
#define HEADER_DragonFlyBSDProcessTable
/*
htop - DragonFlyBSDProcessTable.h
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/param.h>

#include "ProcessTable.h"


typedef struct DragonFlyBSDProcessTable_ {
   ProcessTable super;
} DragonFlyBSDProcessTable;

#endif
