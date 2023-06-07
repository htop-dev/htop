#ifndef HEADER_FreeBSDProcessList
#define HEADER_FreeBSDProcessList
/*
htop - FreeBSDProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"

typedef struct FreeBSDProcessList_ {
   ProcessList super;
} FreeBSDProcessList;

#endif
