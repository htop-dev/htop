#ifndef HEADER_FreeBSDProcessTable
#define HEADER_FreeBSDProcessTable
/*
htop - FreeBSDProcessTable.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessTable.h"
#include "UsersTable.h"

typedef struct FreeBSDProcessTable_ {
   ProcessTable super;
} FreeBSDProcessTable;

#endif
