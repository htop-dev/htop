/*
htop - UnsupportedProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"

#include <stdlib.h>
#include <sys/sysctl.h>

/*{

}*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList) {
   ProcessList* this = calloc(1, sizeof(ProcessList));
   ProcessList_init(this, usersTable, pidWhiteList);

   int cpus = 1;
   size_t sizeof_cpus = sizeof(cpus);
   int err = sysctlbyname("hw.ncpu", &cpus, &sizeof_cpus, NULL, 0);
   if (err) cpus = 1;
   this->cpuCount = MAX(cpus, 1);
   this->cpus = realloc(this->cpus, cpus * sizeof(CPUData));

   for (int i = 0; i < cpus; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }

   return this;
}

void ProcessList_scan(ProcessList* this) {
   (void) this;
   // stub!
}
