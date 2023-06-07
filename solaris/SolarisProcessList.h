#ifndef HEADER_SolarisProcessList
#define HEADER_SolarisProcessList
/*
htop - SolarisProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <kstat.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>

#include "Hashtable.h"
#include "ProcessList.h"
#include "UsersTable.h"

#include "solaris/SolarisProcess.h"


typedef struct SolarisProcessList_ {
   ProcessList super;
} SolarisProcessList;

#endif
