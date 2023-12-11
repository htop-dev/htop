#ifndef HEADER_LinuxProcessTable
#define HEADER_LinuxProcessTable
/*
htop - LinuxProcessTable.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "ProcessTable.h"


typedef struct TtyDriver_ {
   char* path;
   unsigned int major;
   unsigned int minorFrom;
   unsigned int minorTo;
} TtyDriver;

typedef struct LinuxProcessTable_ {
   ProcessTable super;

   TtyDriver* ttyDrivers;
   bool haveSmapsRollup;
   bool haveAutogroup;

   #ifdef HAVE_DELAYACCT
   struct nl_sock* netlink_socket;
   int netlink_family;
   #endif
} LinuxProcessTable;

#endif
