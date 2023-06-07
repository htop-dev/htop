#ifndef HEADER_OpenBSDProcessList
#define HEADER_OpenBSDProcessList
/*
htop - OpenBSDProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "ProcessList.h"


typedef struct OpenBSDProcessList_ {
   ProcessList super;
} OpenBSDProcessList;

#endif
