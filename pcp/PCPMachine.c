/*
htop - PCPProcessTable.c
(C) 2014 Hisham H. Muhammad
(C) 2020-2023 htop dev team
(C) 2020-2023 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/PCPMachine.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "Settings.h"
#include "XUtils.h"

#include "pcp/Metric.h"
#include "pcp/PCPProcess.h"


static void PCPMachine_updateCPUcount(PCPMachine* this) {
   Machine* super = &this->super;
   super->activeCPUs = Metric_instanceCount(PCP_PERCPU_SYSTEM);
   unsigned int cpus = Platform_getMaxCPU();
   if (cpus == super->existingCPUs)
      return;
   if (cpus == 0)
      cpus = super->activeCPUs;
   if (cpus <= 1)
      cpus = super->activeCPUs = 1;
   super->existingCPUs = cpus;

   free(this->percpu);
   free(this->values);

   this->percpu = xCalloc(cpus, sizeof(pmAtomValue*));
   for (unsigned int i = 0; i < cpus; i++)
      this->percpu[i] = xCalloc(CPU_METRIC_COUNT, sizeof(pmAtomValue));
   this->values = xCalloc(cpus, sizeof(pmAtomValue));
}

static void PCPMachine_updateSystemName(PCPMachine* this) {
   pmAtomValue sysname;
   if (!Metric_values(PCP_UNAME_SYSNAME, &sysname, 1, PM_TYPE_STRING))
      sysname.cp = NULL;
   else if (String_eq(sysname.cp, "Linux"))
      this->sys = SYSTEM_NAME_LINUX;
   else if (String_eq(sysname.cp, "Darwin"))
      this->sys = SYSTEM_NAME_DARWIN;
   free(sysname.cp);
}

static void PCPMachine_updateLinuxMemoryInfo(PCPMachine* this) {
   Machine* super = &this->super;
   unsigned long long int freeMem = 0;
   unsigned long long int swapFreeMem = 0;
   unsigned long long int sreclaimableMem = 0;

   pmAtomValue value;
   if (Metric_values(PCP_MEM_FREE, &value, 1, PM_TYPE_U64) != NULL)
      freeMem = value.ull;
   if (Metric_values(PCP_MEM_BUFFERS, &value, 1, PM_TYPE_U64) != NULL)
      this->memValue[MEMORY_CLASS_BUFFERS] = value.ull;
   if (Metric_values(PCP_MEM_SRECLAIM, &value, 1, PM_TYPE_U64) != NULL)
      sreclaimableMem = value.ull;
   if (Metric_values(PCP_MEM_SHARED, &value, 1, PM_TYPE_U64) != NULL)
      this->memValue[MEMORY_CLASS_SHARED] = value.ull;
   if (Metric_values(PCP_MEM_CACHED, &value, 1, PM_TYPE_U64) != NULL)
      this->memValue[MEMORY_CLASS_CACHE] = value.ull + sreclaimableMem - this->memValue[MEMORY_CLASS_SHARED];
   const memory_t usedDiff = freeMem + this->memValue[MEMORY_CLASS_CACHE] + sreclaimableMem + this->memValue[MEMORY_CLASS_BUFFERS];
   this->memValue[MEMORY_CLASS_USED] = (super->totalMem >= usedDiff) ?
           super->totalMem - usedDiff : super->totalMem - freeMem;
   if (Metric_values(PCP_MEM_AVAILABLE, &value, 1, PM_TYPE_U64) != NULL)
      this->memValue[MEMORY_CLASS_AVAILABLE] = MINIMUM(value.ull, super->totalMem);
   else
      this->memValue[MEMORY_CLASS_AVAILABLE] = freeMem;
   if (Metric_values(PCP_MEM_SWAPFREE, &value, 1, PM_TYPE_U64) != NULL)
      swapFreeMem = value.ull;
   if (Metric_values(PCP_MEM_SWAPTOTAL, &value, 1, PM_TYPE_U64) != NULL)
      super->totalSwap = value.ull;
   if (Metric_values(PCP_MEM_SWAPCACHED, &value, 1, PM_TYPE_U64) != NULL)
      super->cachedSwap = value.ull;
   super->usedSwap = super->totalSwap - swapFreeMem - super->cachedSwap;
}

static void PCPMachine_updateDarwinMemoryInfo(PCPMachine* this, Settings* settings) {
   unsigned long long int activeMem = 0;
   unsigned long long int externalMem = 0;
   unsigned long long int purgeableMem = 0;
   unsigned long long int speculativeMem = 0;

   pmAtomValue value;
   if (Metric_values(PCP_MEM_WIRED, &value, 1, PM_TYPE_U64) != NULL)
      this->memValue[MEMORY_CLASS_WIRED] = value.ull;
   if (Metric_values(PCP_MEM_ACTIVE, &value, 1, PM_TYPE_U64) != NULL)
      activeMem = value.ull;
   if (Metric_values(PCP_MEM_EXTERNAL, &value, 1, PM_TYPE_U64) != NULL)
      externalMem = value.ull;
   if (Metric_values(PCP_MEM_PURGEABLE, &value, 1, PM_TYPE_U64) != NULL)
      purgeableMem = value.ull;
   if (Metric_values(PCP_MEM_SPECULATIVE, &value, 1, PM_TYPE_U64) != NULL)
      speculativeMem = value.ull;
   if (settings->showCachedMemory) {
      this->memValue[MEMORY_CLASS_SPECULATIVE] = speculativeMem;
      this->memValue[MEMORY_CLASS_ACTIVE] = (activeMem - purgeableMem - externalMem);
      this->memValue[MEMORY_CLASS_PURGEABLE] = purgeableMem;
   }
   else {
      this->memValue[MEMORY_CLASS_SPECULATIVE] = 0;
      this->memValue[MEMORY_CLASS_ACTIVE] = (speculativeMem + activeMem - externalMem);
      this->memValue[MEMORY_CLASS_PURGEABLE] = 0;
   }
   if (Metric_values(PCP_MEM_COMPRESSED, &value, 1, PM_TYPE_U64) != NULL)
      this->memValue[MEMORY_CLASS_COMPRESSED] = value.ull;
   if (Metric_values(PCP_MEM_INACTIVE, &value, 1, PM_TYPE_U64) != NULL)
      this->memValue[MEMORY_CLASS_INACTIVE] = value.ull;
}

static void PCPMachine_updateMemoryInfo(Machine* super) {
   PCPMachine* this = (PCPMachine*) super;

   pmAtomValue value;
   if (Metric_values(PCP_MEM_TOTAL, &value, 1, PM_TYPE_U64) != NULL)
      super->totalMem = value.ull;
   else
      super->totalMem = 0;

   memset(this->memValue, 0, sizeof(this->memValue));
   if (this->sys == SYSTEM_NAME_DARWIN)
      PCPMachine_updateDarwinMemoryInfo(this, super->settings);
   else if (this->sys == SYSTEM_NAME_LINUX)
      PCPMachine_updateLinuxMemoryInfo(this);
}

/* make copies of previously sampled values to avoid overwrite */
static inline void PCPMachine_backupCPUTime(pmAtomValue* values) {
   /* the PERIOD fields (must) mirror the TIME fields */
   for (int metric = CPU_TOTAL_TIME; metric < CPU_TOTAL_PERIOD; metric++) {
      values[metric + CPU_TOTAL_PERIOD] = values[metric];
   }
}

static inline void PCPMachine_saveCPUTimePeriod(pmAtomValue* values, CPUMetric previous, pmAtomValue* latest) {
   pmAtomValue* value;

   /* new value for period */
   value = &values[previous];
   if (latest->ull > value->ull)
      value->ull = latest->ull - value->ull;
   else
      value->ull = 0;

   /* new value for time */
   value = &values[previous - CPU_TOTAL_PERIOD];
   value->ull = latest->ull;
}

/* using copied sampled values and new values, calculate derivations */
static void PCPMachine_deriveCPUTime(pmAtomValue* values) {

   pmAtomValue* usertime = &values[CPU_USER_TIME];
   pmAtomValue* guesttime = &values[CPU_GUEST_TIME];
   usertime->ull -= guesttime->ull;

   pmAtomValue* nicetime = &values[CPU_NICE_TIME];
   pmAtomValue* guestnicetime = &values[CPU_GUESTNICE_TIME];
   nicetime->ull -= guestnicetime->ull;

   pmAtomValue* idletime = &values[CPU_IDLE_TIME];
   pmAtomValue* iowaittime = &values[CPU_IOWAIT_TIME];
   pmAtomValue* idlealltime = &values[CPU_IDLE_ALL_TIME];
   idlealltime->ull = idletime->ull + iowaittime->ull;

   pmAtomValue* systemtime = &values[CPU_SYSTEM_TIME];
   pmAtomValue* irqtime = &values[CPU_IRQ_TIME];
   pmAtomValue* softirqtime = &values[CPU_SOFTIRQ_TIME];
   pmAtomValue* systalltime = &values[CPU_SYSTEM_ALL_TIME];
   systalltime->ull = systemtime->ull + irqtime->ull + softirqtime->ull;

   pmAtomValue* virtalltime = &values[CPU_GUEST_TIME];
   virtalltime->ull = guesttime->ull + guestnicetime->ull;

   pmAtomValue* stealtime = &values[CPU_STEAL_TIME];
   pmAtomValue* totaltime = &values[CPU_TOTAL_TIME];
   totaltime->ull = usertime->ull + nicetime->ull + systalltime->ull +
                    idlealltime->ull + stealtime->ull + virtalltime->ull;

   PCPMachine_saveCPUTimePeriod(values, CPU_USER_PERIOD, usertime);
   PCPMachine_saveCPUTimePeriod(values, CPU_NICE_PERIOD, nicetime);
   PCPMachine_saveCPUTimePeriod(values, CPU_SYSTEM_PERIOD, systemtime);
   PCPMachine_saveCPUTimePeriod(values, CPU_SYSTEM_ALL_PERIOD, systalltime);
   PCPMachine_saveCPUTimePeriod(values, CPU_IDLE_ALL_PERIOD, idlealltime);
   PCPMachine_saveCPUTimePeriod(values, CPU_IDLE_PERIOD, idletime);
   PCPMachine_saveCPUTimePeriod(values, CPU_IOWAIT_PERIOD, iowaittime);
   PCPMachine_saveCPUTimePeriod(values, CPU_IRQ_PERIOD, irqtime);
   PCPMachine_saveCPUTimePeriod(values, CPU_SOFTIRQ_PERIOD, softirqtime);
   PCPMachine_saveCPUTimePeriod(values, CPU_STEAL_PERIOD, stealtime);
   PCPMachine_saveCPUTimePeriod(values, CPU_GUEST_PERIOD, virtalltime);
   PCPMachine_saveCPUTimePeriod(values, CPU_TOTAL_PERIOD, totaltime);
}

static void PCPMachine_updateAllCPUTime(PCPMachine* this, Metric metric, CPUMetric cpumetric)
{
   pmAtomValue* value = &this->cpu[cpumetric];
   if (Metric_values(metric, value, 1, PM_TYPE_U64) == NULL)
      memset(value, 0, sizeof(pmAtomValue));
}

static void PCPMachine_updatePerCPUTime(PCPMachine* this, Metric metric, CPUMetric cpumetric)
{
   int cpus = this->super.existingCPUs;
   if (Metric_values(metric, this->values, cpus, PM_TYPE_U64) == NULL)
      memset(this->values, 0, cpus * sizeof(pmAtomValue));
   for (int i = 0; i < cpus; i++)
      this->percpu[i][cpumetric].ull = this->values[i].ull;
}

static void PCPMachine_updatePerCPUReal(PCPMachine* this, Metric metric, CPUMetric cpumetric)
{
   int cpus = this->super.existingCPUs;
   if (Metric_values(metric, this->values, cpus, PM_TYPE_DOUBLE) == NULL)
      memset(this->values, 0, cpus * sizeof(pmAtomValue));
   for (int i = 0; i < cpus; i++)
      this->percpu[i][cpumetric].d = this->values[i].d;
}

static inline void PCPMachine_scanZswapInfo(PCPMachine* this) {
   pmAtomValue value;

   memset(&this->zswap, 0, sizeof(ZswapStats));
   if (Metric_values(PCP_MEM_ZSWAP, &value, 1, PM_TYPE_U64))
      this->zswap.usedZswapComp = value.ull;
   if (Metric_values(PCP_MEM_ZSWAPPED, &value, 1, PM_TYPE_U64))
      this->zswap.usedZswapOrig = value.ull;
}

static inline void PCPMachine_scanZfsArcstats(PCPMachine* this) {
   unsigned long long int dbufSize = 0;
   unsigned long long int dnodeSize = 0;
   unsigned long long int bonusSize = 0;
   pmAtomValue value;

   memset(&this->zfs, 0, sizeof(ZfsArcStats));
   if (Metric_values(PCP_ZFS_ARC_ANON_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.anon = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_C_MIN, &value, 1, PM_TYPE_U64))
      this->zfs.min = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_C_MAX, &value, 1, PM_TYPE_U64))
      this->zfs.max = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_BONUS_SIZE, &value, 1, PM_TYPE_U64))
      bonusSize = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_DBUF_SIZE, &value, 1, PM_TYPE_U64))
      dbufSize = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_DNODE_SIZE, &value, 1, PM_TYPE_U64))
      dnodeSize = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_COMPRESSED_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.compressed = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_UNCOMPRESSED_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.uncompressed = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_HDR_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.header = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_MFU_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.MFU = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_MRU_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.MRU = value.ull / ONE_K;
   if (Metric_values(PCP_ZFS_ARC_SIZE, &value, 1, PM_TYPE_U64))
      this->zfs.size = value.ull / ONE_K;

   this->zfs.other = (dbufSize + dnodeSize + bonusSize) / ONE_K;
   this->zfs.enabled = (this->zfs.size > 0);
   this->zfs.isCompressed = (this->zfs.compressed > 0);
}

static void PCPMachine_scan(PCPMachine* this) {
   Machine* super = &this->super;

   PCPMachine_updateMemoryInfo(super);
   PCPMachine_updateCPUcount(this);

   PCPMachine_backupCPUTime(this->cpu);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_USER, CPU_USER_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_NICE, CPU_NICE_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_SYSTEM, CPU_SYSTEM_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_IDLE, CPU_IDLE_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_IOWAIT, CPU_IOWAIT_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_IRQ, CPU_IRQ_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_SOFTIRQ, CPU_SOFTIRQ_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_STEAL, CPU_STEAL_TIME);
   PCPMachine_updateAllCPUTime(this, PCP_CPU_GUEST, CPU_GUEST_TIME);
   PCPMachine_deriveCPUTime(this->cpu);

   for (unsigned int i = 0; i < super->existingCPUs; i++)
      PCPMachine_backupCPUTime(this->percpu[i]);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_USER, CPU_USER_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_NICE, CPU_NICE_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_SYSTEM, CPU_SYSTEM_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_IDLE, CPU_IDLE_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_IOWAIT, CPU_IOWAIT_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_IRQ, CPU_IRQ_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_SOFTIRQ, CPU_SOFTIRQ_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_STEAL, CPU_STEAL_TIME);
   PCPMachine_updatePerCPUTime(this, PCP_PERCPU_GUEST, CPU_GUEST_TIME);
   for (unsigned int i = 0; i < super->existingCPUs; i++)
      PCPMachine_deriveCPUTime(this->percpu[i]);

   if (super->settings->showCPUFrequency)
      PCPMachine_updatePerCPUReal(this, PCP_HINV_CPUCLOCK, CPU_FREQUENCY);

   PCPMachine_scanZfsArcstats(this);
   PCPMachine_scanZswapInfo(this);
}

void Machine_scan(Machine* super) {
   PCPMachine* host = (PCPMachine*) super;
   const Settings* settings = super->settings;
   uint32_t flags = settings->ss->flags;
   bool flagged;

   for (int metric = PCP_PROC_PID; metric < PCP_METRIC_COUNT; metric++)
      Metric_enable(metric, true);

   flagged = settings->showCPUFrequency;
   Metric_enable(PCP_HINV_CPUCLOCK, flagged);
   flagged = flags & PROCESS_FLAG_LINUX_CGROUP;
   Metric_enable(PCP_PROC_CGROUPS, flagged);
   flagged = flags & PROCESS_FLAG_LINUX_OOM;
   Metric_enable(PCP_PROC_OOMSCORE, flagged);
   flagged = flags & PROCESS_FLAG_LINUX_CTXT;
   Metric_enable(PCP_PROC_VCTXSW, flagged);
   Metric_enable(PCP_PROC_NVCTXSW, flagged);
   flagged = flags & PROCESS_FLAG_LINUX_SECATTR;
   Metric_enable(PCP_PROC_LABELS, flagged);
   flagged = flags & PROCESS_FLAG_LINUX_AUTOGROUP;
   Metric_enable(PCP_PROC_AUTOGROUP_ID, flagged);
   Metric_enable(PCP_PROC_AUTOGROUP_NICE, flagged);

   /* Sample smaps metrics on every second pass to improve performance */
   host->smaps_flag = !!host->smaps_flag;
   Metric_enable(PCP_PROC_SMAPS_PSS, host->smaps_flag);
   Metric_enable(PCP_PROC_SMAPS_SWAP, host->smaps_flag);
   Metric_enable(PCP_PROC_SMAPS_SWAPPSS, host->smaps_flag);

   struct timeval timestamp;
   if (Metric_fetch(&timestamp) != true)
      return;

   double sample = host->timestamp;
   host->timestamp = pmtimevalToReal(&timestamp);
   host->period = (host->timestamp - sample) * 100;

   PCPMachine_scan(host);
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   PCPMachine* this = xCalloc(1, sizeof(PCPMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   struct timeval timestamp;
   gettimeofday(&timestamp, NULL);
   this->timestamp = pmtimevalToReal(&timestamp);

   this->sys = SYSTEM_NAME_UNKNOWN;
   PCPMachine_updateSystemName(this);

   this->cpu = xCalloc(CPU_METRIC_COUNT, sizeof(pmAtomValue));
   PCPMachine_updateCPUcount(this);

   Platform_updateTables(super);

   return super;
}

void Machine_delete(Machine* super) {
   PCPMachine* this = (PCPMachine*) super;
   Machine_done(super);
   free(this->values);
   for (unsigned int i = 0; i < super->existingCPUs; i++)
      free(this->percpu[i]);
   free(this->percpu);
   free(this->cpu);
   free(this);
}

bool Machine_isCPUonline(const Machine* host, unsigned int id) {
   assert(id < host->existingCPUs);
   (void) host;

   pmAtomValue value;
   if (Metric_instance(PCP_PERCPU_SYSTEM, id, id, &value, PM_TYPE_U32))
      return true;
   return false;
}
