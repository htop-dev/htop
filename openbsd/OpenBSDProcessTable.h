#ifndef HEADER_OpenBSDProcessTable
#define HEADER_OpenBSDProcessTable
/*
htop - OpenBSDProcessTable.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "ProcessTable.h"


typedef struct OpenBSDProcessTable_ {
   ProcessTable super;
} OpenBSDProcessTable;

#endif
