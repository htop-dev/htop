#ifndef HEADER_PCPProcessTable
#define HEADER_PCPProcessTable
/*
htop - PCPProcessTable.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessTable.h"
#include "UsersTable.h"

#include "pcp/Platform.h"


typedef struct PCPProcessTable_ {
   ProcessTable super;
} PCPProcessTable;

#endif
