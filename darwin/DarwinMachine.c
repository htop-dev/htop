/*
htop - DarwinMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "darwin/DarwinMachine.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include "CRT.h"
#include "Machine.h"
#include "darwin/Platform.h"
#include "darwin/PlatformHelpers.h"
#include "generic/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"


static void DarwinMachine_getHostInfo(host_basic_info_data_t* p) {
   mach_msg_type_number_t info_size = HOST_BASIC_INFO_COUNT;

   if (0 != host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)p, &info_size)) {
      CRT_fatalError("Unable to retrieve host info");
   }
}

static void DarwinMachine_freeCPULoadInfo(processor_cpu_load_info_t* p) {
   if (!p)
      return;

   if (!*p)
      return;

   if (0 != munmap(*p, vm_page_size)) {
      CRT_fatalError("Unable to free old CPU load information");
   }

   *p = NULL;
}

static unsigned int DarwinMachine_allocateCPULoadInfo(processor_cpu_load_info_t* p) {
   mach_msg_type_number_t info_size = sizeof(processor_cpu_load_info_t);
   natural_t cpu_count;

   // TODO Improving the accuracy of the load counts would help a lot.
   if (0 != host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, (processor_info_array_t*)p, &info_size)) {
      CRT_fatalError("Unable to retrieve CPU info");
   }

   return cpu_count;
}

static void DarwinMachine_getVMStats(DarwinMachine* this) {
   mach_msg_type_number_t info_size = HOST_VM_INFO64_COUNT;

   if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info_t)&this->vm_stats, &info_size) != 0) {
      CRT_fatalError("Unable to retrieve VM statistics64");
   }
}

void Machine_scan(Machine* super) {
   DarwinMachine* host = (DarwinMachine*) super;

   /* Update the global data (CPU times and VM stats) */
   DarwinMachine_freeCPULoadInfo(&host->prev_load);
   host->prev_load = host->curr_load;
   DarwinMachine_allocateCPULoadInfo(&host->curr_load);
   DarwinMachine_getVMStats(host);
   openzfs_sysctl_updateArcStats(&host->zfs);
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   DarwinMachine* this = xCalloc(1, sizeof(DarwinMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   /* Initialize the CPU information */
   super->activeCPUs = DarwinMachine_allocateCPULoadInfo(&this->prev_load);
   super->existingCPUs = super->activeCPUs;
   DarwinMachine_getHostInfo(&this->host_info);
   DarwinMachine_allocateCPULoadInfo(&this->curr_load);

   /* Initialize the VM statistics */
   DarwinMachine_getVMStats(this);

   /* Initialize the ZFS kstats, if zfs.kext loaded */
   openzfs_sysctl_init(&this->zfs);
   openzfs_sysctl_updateArcStats(&this->zfs);

   this->GPUService = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOGPU"));
   if (!this->GPUService) {
      CRT_debug("Cannot initialize IOGPU service");
   }

   return super;
}

void Machine_delete(Machine* super) {
   DarwinMachine* this = (DarwinMachine*) super;

   IOObjectRelease(this->GPUService);

   DarwinMachine_freeCPULoadInfo(&this->prev_load);

   Machine_done(super);
   free(this);
}

bool Machine_isCPUonline(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);

   // TODO: support offline CPUs and hot swapping
   (void) host; (void) id;

   return true;
}
