#ifndef HEADER_CygwinMachine
#define HEADER_CygwinMachine
/*
htop - CygwinMachine.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2017,2018 Guy M. Broome
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Machine.h"


typedef struct CPUData_ {
   bool online;
} CPUData;

typedef struct CygwinMachine_ {
   Machine super;

   CPUData* cpuData;
} CygwinMachine;

#ifndef PROCDIR
#define PROCDIR "/proc"
#endif

#ifndef PROCMEMINFOFILE
#define PROCMEMINFOFILE PROCDIR "/meminfo"
#endif

#endif /* HEADER_CygwinMachine */
