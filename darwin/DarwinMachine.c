/*
htop - DarwinMachine.c
(C) 2014 Hisham H. Muhammad
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "darwin/DarwinMachine.h"

#include <Availability.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOTypes.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "XUtils.h"
#include "darwin/Platform.h"
#include "darwin/PlatformHelpers.h"
#include "generic/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"


// Compatibility for older SDKs.
#if !defined(HAVE_DECL_KIOMAINPORTDEFAULT) || !HAVE_DECL_KIOMAINPORTDEFAULT
#define kIOMainPortDefault kIOMasterPortDefault
#endif

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

static bool DarwinMachine_cpuSlotsMatch(unsigned int cpus) {
   host_t host = mach_host_self();
   natural_t count;
   processor_info_array_t info;
   mach_msg_type_number_t size;
   kern_return_t result = host_processor_info(host, PROCESSOR_BASIC_INFO, &count, &info, &size);
   mach_port_deallocate(mach_task_self(), host);
   if (result != KERN_SUCCESS)
      return false;

   /* Both Mach info flavors use the same processor array. Verify that its
      indexes match the logical CPU IDs used by the registry. */
   const struct processor_basic_info* basic = (const void*)info;
   bool match = count == cpus && size == count * PROCESSOR_BASIC_INFO_COUNT;
   for (unsigned int i = 0; match && i < count; i++)
      match = basic[i].slot_num == (int)i;

   vm_deallocate(mach_task_self(), (vm_address_t)info, size * sizeof(*info));
   return match;
}

static bool DarwinMachine_readCPUClusterTypes(char* types, unsigned int cpus) {
   io_registry_entry_t root = IORegistryEntryFromPath(kIOMainPortDefault, kIODeviceTreePlane ":/cpus");
   if (root == IO_OBJECT_NULL)
      return false;

   io_iterator_t children;
   kern_return_t result = IORegistryEntryGetChildIterator(root, kIODeviceTreePlane, &children);
   IOObjectRelease(root);
   if (result != KERN_SUCCESS)
      return false;

   unsigned int found = 0;
   io_registry_entry_t child;
   while ((child = IOIteratorNext(children)) != IO_OBJECT_NULL) {
      CFMutableDictionaryRef props;
      result = IORegistryEntryCreateCFProperties(child, &props, kCFAllocatorDefault, kNilOptions);
      IOObjectRelease(child);
      if (result != KERN_SUCCESS)
         continue;

      CFNumberRef idProp = CFDictionaryGetValue(props, CFSTR("logical-cpu-id"));
      CFDataRef type = CFDictionaryGetValue(props, CFSTR("cluster-type"));
      int id;
      if (idProp && CFGetTypeID(idProp) == CFNumberGetTypeID() &&
         CFNumberGetValue(idProp, kCFNumberIntType, &id) && id >= 0 && (unsigned int)id < cpus &&
         type && CFGetTypeID(type) == CFDataGetTypeID() && CFDataGetLength(type) > 0) {
         if (types[id] != '\0') {
            CFRelease(props);
            IOObjectRelease(children);
            return false;
         }
         types[id] = (char)CFDataGetBytePtr(type)[0];
         found++;
      }
      CFRelease(props);
   }
   IOObjectRelease(children);
   return found == cpus;
}

static bool DarwinMachine_readSysctlUInt(const char* key, unsigned int* value) {
   size_t size = sizeof(*value);
   return sysctlbyname(key, value, &size, NULL, 0) == 0 && size == sizeof(*value);
}

static bool DarwinMachine_labelCPUClusters(const char* rawTypes, CPUTypeInfo* types, unsigned int cpus, unsigned int levels) {
   /* I/O Registry cluster types in descending performance order, matching
      hw.perflevel0..N-1. A raw 'P' can represent Performance or Super, so
      verify each cluster's core count against hw.perflevelN.logicalcpu_max
      and derive the label and tier from hw.perflevelN.name. */
   static const char order[] = { 'P', 'M', 'E' };
   unsigned int counts[ARRAYSIZE(order)] = {0};
   for (unsigned int i = 0; i < cpus; i++) {
      const char* type = memchr(order, rawTypes[i], sizeof(order));
      if (!type)
         return false;
      counts[type - order]++;
   }

   CPUTypeInfo clusterTypes[ARRAYSIZE(order)] = {0};
   unsigned int level = 0;
   for (size_t i = 0; i < ARRAYSIZE(order); i++) {
      if (!counts[i])
         continue;
      if (level == levels)
         return false;

      char key[40];
      unsigned int count;
      xSnprintf(key, sizeof(key), "hw.perflevel%u.logicalcpu_max", level);
      if (!DarwinMachine_readSysctlUInt(key, &count) || count != counts[i])
         return false;

      char name[32] = {0};
      size_t size = sizeof(name) - 1;
      xSnprintf(key, sizeof(key), "hw.perflevel%u.name", level);
      if (sysctlbyname(key, name, &size, NULL, 0) != 0)
         return false;

      CPUTypeTier tier;
      if (String_eq(name, "Efficiency"))
         tier = CPU_TYPE_TIER_LOW;
      else if (String_eq(name, "Performance"))
         tier = CPU_TYPE_TIER_MEDIUM;
      else if (String_eq(name, "Super"))
         tier = CPU_TYPE_TIER_HIGH;
      else
         return false;

      clusterTypes[i].label = name[0];
      clusterTypes[i].tier = tier;
      level++;
   }
   if (level != levels)
      return false;

   for (unsigned int i = 0; i < cpus; i++) {
      const char* type = memchr(order, rawTypes[i], sizeof(order));
      types[i] = clusterTypes[type - order];
   }
   return true;
}

static CPUTypeInfo* DarwinMachine_getCPUTypes(unsigned int cpus) {
   unsigned int arm64;
   unsigned int levels;
   if (cpus < 2 || !DarwinMachine_readSysctlUInt("hw.optional.arm64", &arm64) || !arm64 ||
      !DarwinMachine_readSysctlUInt("hw.nperflevels", &levels) || levels < 2 ||
      !DarwinMachine_cpuSlotsMatch(cpus))
      return NULL;

   char* rawTypes = xCalloc(cpus, sizeof(*rawTypes));
   CPUTypeInfo* types = xCalloc(cpus, sizeof(*types));
   bool ok = DarwinMachine_readCPUClusterTypes(rawTypes, cpus) &&
      DarwinMachine_labelCPUClusters(rawTypes, types, cpus, levels);
   free(rawTypes);
   if (ok)
      return types;

   free(types);
   return NULL;
}

static void DarwinMachine_getVMStats(DarwinMachine* this) {
#ifdef HAVE_STRUCT_VM_STATISTICS64
   mach_msg_type_number_t info_size = HOST_VM_INFO64_COUNT;

   if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info_t)&this->vm_stats, &info_size) != 0) {
      CRT_fatalError("Unable to retrieve VM statistics64");
   }
#else
   mach_msg_type_number_t info_size = HOST_VM_INFO_COUNT;

   if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&this->vm_stats, &info_size) != 0) {
      CRT_fatalError("Unable to retrieve VM statistics");
   }
#endif
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
   super->cpuTypes = DarwinMachine_getCPUTypes(super->existingCPUs);
   super->cpuTypeCount = super->cpuTypes ? super->existingCPUs : 0;
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

int Machine_getCPUPhysicalCoreID(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);
   (void) host;
   return (int)id;
}

int Machine_getCPUThreadIndex(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);
   (void) host; (void) id;
   return 0;
}
