#ifndef HEADER_SolarisProcessTable
#define HEADER_SolarisProcessTable
/*
htop - SolarisProcessTable.h
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <kstat.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>

#include "Hashtable.h"
#include "ProcessTable.h"
#include "UsersTable.h"

#include "solaris/SolarisProcess.h"


typedef struct SolarisProcessTable_ {
   ProcessTable super;
} SolarisProcessTable;

#endif
