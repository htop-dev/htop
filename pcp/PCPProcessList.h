#ifndef HEADER_PCPProcessList
#define HEADER_PCPProcessList
/*
htop - PCPProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"

#include "pcp/Platform.h"


typedef struct PCPProcessList_ {
   ProcessList super;
} PCPProcessList;

#endif
