#ifndef HEADER_LinuxProcessList
#define HEADER_LinuxProcessList
/*
htop - LinuxProcessList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"


typedef struct TtyDriver_ {
   char* path;
   unsigned int major;
   unsigned int minorFrom;
   unsigned int minorTo;
} TtyDriver;

typedef struct LinuxProcessList_ {
   ProcessList super;

   TtyDriver* ttyDrivers;
   bool haveSmapsRollup;
   bool haveAutogroup;

   #ifdef HAVE_DELAYACCT
   struct nl_sock* netlink_socket;
   int netlink_family;
   #endif
} LinuxProcessList;

#endif
