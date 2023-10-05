#ifndef HEADER_NetBSDProcessTable
#define HEADER_NetBSDProcessTable
/*
htop - NetBSDProcessTable.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2021 Santhosh Raju
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessTable.h"


typedef struct NetBSDProcessTable_ {
   ProcessTable super;
} NetBSDProcessTable;

#endif
