/*
htop - LinuxMachine.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/LinuxMachine.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>

#include "Compat.h"
#include "CRT.h"
#include "Macros.h"
#include "ProcessTable.h"
#include "Row.h"
#include "Settings.h"
#include "UsersTable.h"
#include "XUtils.h"

#include "linux/Platform.h" // needed for GNU/hurd to get PATH_MAX  // IWYU pragma: keep

#ifdef HAVE_SENSORS_SENSORS_H
#include "LibSensors.h"
#endif

#ifndef O_PATH
#define O_PATH         010000000 // declare for ancient glibc versions
#endif

/* Similar to get_nprocs_conf(3) / _SC_NPROCESSORS_CONF
 * https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/unix/sysv/linux/getsysstats.c;hb=HEAD
 */
static void LinuxMachine_updateCPUcount(LinuxMachine* this) {
   unsigned int existing = 0, active = 0;
   Machine* super = &this->super;

   // Initialize the cpuData array before anything else.
   if (!this->cpuData) {
      this->cpuData = xCalloc(2, sizeof(CPUData));
      this->cpuData[0].online = true; /* average is always "online" */
      this->cpuData[1].online = true;
      super->activeCPUs = 1;
      super->existingCPUs = 1;
   }

   DIR* dir = opendir("/sys/devices/system/cpu");
   if (!dir)
      return;

   unsigned int currExisting = super->existingCPUs;

   const struct dirent* entry;
   while ((entry = readdir(dir)) != NULL) {
      if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN)
         continue;

      if (!String_startsWith(entry->d_name, "cpu"))
         continue;

      char* endp;
      unsigned long int id = strtoul(entry->d_name + 3, &endp, 10);
      if (id == ULONG_MAX || endp == entry->d_name + 3 || *endp != '\0')
         continue;

#ifdef HAVE_OPENAT
      int cpuDirFd = openat(dirfd(dir), entry->d_name, O_DIRECTORY | O_PATH | O_NOFOLLOW);
      if (cpuDirFd < 0)
         continue;
#else
      char cpuDirFd[4096];
      xSnprintf(cpuDirFd, sizeof(cpuDirFd), "/sys/devices/system/cpu/%s", entry->d_name);
#endif

      existing++;

      /* readdir() iterates with no specific order */
      unsigned int max = MAXIMUM(existing, id + 1);
      if (max > currExisting) {
         this->cpuData = xReallocArrayZero(this->cpuData, currExisting ? (currExisting + 1) : 0, max + /* aggregate */ 1, sizeof(CPUData));
         this->cpuData[0].online = true; /* average is always "online" */
         currExisting = max;
      }

      char buffer[8];
      ssize_t res = xReadfileat(cpuDirFd, "online", buffer, sizeof(buffer));
      /* If the file "online" does not exist or on failure count as active */
      if (res < 1 || buffer[0] != '0') {
         active++;
         this->cpuData[id + 1].online = true;
      } else {
         this->cpuData[id + 1].online = false;
      }

      Compat_openatArgClose(cpuDirFd);
   }

   closedir(dir);

   // return if no CPU is found
   if (existing < 1)
      return;

#ifdef HAVE_SENSORS_SENSORS_H
   /* When started with offline CPUs, libsensors does not monitor those,
    * even when they become online. */
   if (super->existingCPUs != 0 && (active > super->activeCPUs || currExisting > super->existingCPUs))
      LibSensors_reload();
#endif

   super->activeCPUs = active;
   assert(existing == currExisting);
   super->existingCPUs = currExisting;
}

static void LinuxMachine_scanMemoryInfo(LinuxMachine* this) {
   Machine* host = &this->super;
   memory_t availableMem = 0;
   memory_t freeMem = 0;
   memory_t totalMem = 0;
   memory_t buffersMem = 0;
   memory_t cachedMem = 0;
   memory_t sharedMem = 0;
   memory_t swapTotalMem = 0;
   memory_t swapCacheMem = 0;
   memory_t swapFreeMem = 0;
   memory_t sreclaimableMem = 0;
   memory_t zswapCompMem = 0;
   memory_t zswapOrigMem = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (!file)
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);

   char buffer[128];
   while (fgets(buffer, sizeof(buffer), file)) {

      #define tryRead(label, variable)                                       \
         if (String_startsWith(buffer, label)) {                             \
            memory_t parsed_;                                                \
            if (sscanf(buffer + strlen(label), "%llu kB", &parsed_) == 1) {  \
               (variable) = parsed_;                                         \
            }                                                                \
            break;                                                           \
         } else (void) 0 /* Require a ";" after the macro use. */

      switch (buffer[0]) {
         case 'M':
            tryRead("MemAvailable:", availableMem);
            tryRead("MemFree:", freeMem);
            tryRead("MemTotal:", totalMem);
            break;
         case 'B':
            tryRead("Buffers:", buffersMem);
            break;
         case 'C':
            tryRead("Cached:", cachedMem);
            break;
         case 'S':
            switch (buffer[1]) {
               case 'h':
                  tryRead("Shmem:", sharedMem);
                  break;
               case 'w':
                  tryRead("SwapTotal:", swapTotalMem);
                  tryRead("SwapCached:", swapCacheMem);
                  tryRead("SwapFree:", swapFreeMem);
                  break;
               case 'R':
                  tryRead("SReclaimable:", sreclaimableMem);
                  break;
            }
            break;
         case 'Z':
            tryRead("Zswap:", zswapCompMem);
            tryRead("Zswapped:", zswapOrigMem);
            break;
      }

      #undef tryRead
   }

   fclose(file);

   /*
    * Compute memory partition like procps(free)
    *  https://gitlab.com/procps-ng/procps/-/blob/master/proc/sysinfo.c
    *
    * Adjustments:
    *  - Shmem in part of Cached (see https://lore.kernel.org/patchwork/patch/648763/),
    *    do not show twice by subtracting from Cached and do not subtract twice from used.
    */
   host->totalMem = totalMem;
   host->cachedMem = cachedMem + sreclaimableMem - sharedMem;
   host->sharedMem = sharedMem;
   const memory_t usedDiff = freeMem + cachedMem + sreclaimableMem + buffersMem;
   host->usedMem = (totalMem >= usedDiff) ? totalMem - usedDiff : totalMem - freeMem;
   host->buffersMem = buffersMem;
   host->availableMem = availableMem != 0 ? MINIMUM(availableMem, totalMem) : freeMem;
   host->totalSwap = swapTotalMem;
   host->usedSwap = swapTotalMem - swapFreeMem - swapCacheMem;
   host->cachedSwap = swapCacheMem;
   this->zswap.usedZswapComp = zswapCompMem;
   this->zswap.usedZswapOrig = zswapOrigMem;
}

static void LinuxMachine_scanHugePages(LinuxMachine* this) {
   this->totalHugePageMem = 0;
   for (unsigned i = 0; i < HTOP_HUGEPAGE_COUNT; i++) {
      this->usedHugePageMem[i] = MEMORY_MAX;
   }

   DIR* dir = opendir("/sys/kernel/mm/hugepages");
   if (!dir)
      return;

   const struct dirent* entry;
   while ((entry = readdir(dir)) != NULL) {
      const char* name = entry->d_name;

      /* Ignore all non-directories */
      if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN)
         continue;

      if (!String_startsWith(name, "hugepages-"))
         continue;

      char* endptr;
      unsigned long int hugePageSize = strtoul(name + strlen("hugepages-"), &endptr, 10);
      if (!endptr || *endptr != 'k')
         continue;

      char content[64];
      char hugePagePath[128];
      ssize_t r;

      xSnprintf(hugePagePath, sizeof(hugePagePath), "/sys/kernel/mm/hugepages/%s/nr_hugepages", name);
      r = xReadfile(hugePagePath, content, sizeof(content));
      if (r <= 0)
         continue;

      memory_t total = strtoull(content, NULL, 10);
      if (total == 0)
         continue;

      xSnprintf(hugePagePath, sizeof(hugePagePath), "/sys/kernel/mm/hugepages/%s/free_hugepages", name);
      r = xReadfile(hugePagePath, content, sizeof(content));
      if (r <= 0)
         continue;

      memory_t free = strtoull(content, NULL, 10);

      int shift = ffsl(hugePageSize) - 1 - (HTOP_HUGEPAGE_BASE_SHIFT - 10);
      assert(shift >= 0 && shift < HTOP_HUGEPAGE_COUNT);

      this->totalHugePageMem += total * hugePageSize;
      this->usedHugePageMem[shift] = (total - free) * hugePageSize;
   }

   closedir(dir);
}

static void LinuxMachine_scanZramInfo(LinuxMachine* this) {
   memory_t totalZram = 0;
   memory_t usedZramComp = 0;
   memory_t usedZramOrig = 0;

   char mm_stat[34];
   char disksize[34];

   unsigned int i = 0;
   for (;;) {
      xSnprintf(mm_stat, sizeof(mm_stat), "/sys/block/zram%u/mm_stat", i);
      xSnprintf(disksize, sizeof(disksize), "/sys/block/zram%u/disksize", i);
      i++;
      FILE* disksize_file = fopen(disksize, "r");
      FILE* mm_stat_file = fopen(mm_stat, "r");
      if (disksize_file == NULL || mm_stat_file == NULL) {
         if (disksize_file) {
            fclose(disksize_file);
         }
         if (mm_stat_file) {
            fclose(mm_stat_file);
         }
         break;
      }
      memory_t size = 0;
      memory_t orig_data_size = 0;
      memory_t compr_data_size = 0;

      if (!fscanf(disksize_file, "%llu\n", &size) ||
          !fscanf(mm_stat_file, "    %llu       %llu", &orig_data_size, &compr_data_size)) {
         fclose(disksize_file);
         fclose(mm_stat_file);
         break;
      }

      totalZram += size;
      usedZramComp += compr_data_size;
      usedZramOrig += orig_data_size;

      fclose(disksize_file);
      fclose(mm_stat_file);
   }

   this->zram.totalZram = totalZram / 1024;
   this->zram.usedZramComp = usedZramComp / 1024;
   this->zram.usedZramOrig = usedZramOrig / 1024;
   if (this->zram.usedZramComp > this->zram.usedZramOrig) {
      this->zram.usedZramComp = this->zram.usedZramOrig;
   }
}

static void LinuxMachine_scanZfsArcstats(LinuxMachine* this) {
   memory_t dbufSize = 0;
   memory_t dnodeSize = 0;
   memory_t bonusSize = 0;

   FILE* file = fopen(PROCARCSTATSFILE, "r");
   if (file == NULL) {
      this->zfs.enabled = 0;
      return;
   }
   char buffer[128];
   while (fgets(buffer, 128, file)) {
      #define tryRead(label, variable)                                         \
         if (String_startsWith(buffer, label)) {                               \
            sscanf(buffer + strlen(label), " %*2u %32llu", variable);          \
            break;                                                             \
         } else (void) 0 /* Require a ";" after the macro use. */
      #define tryReadFlag(label, variable, flag)                               \
         if (String_startsWith(buffer, label)) {                               \
            (flag) = sscanf(buffer + strlen(label), " %*2u %32llu", variable); \
            break;                                                             \
         } else (void) 0 /* Require a ";" after the macro use. */

      switch (buffer[0]) {
         case 'c':
            tryRead("c_min", &this->zfs.min);
            tryRead("c_max", &this->zfs.max);
            tryReadFlag("compressed_size", &this->zfs.compressed, this->zfs.isCompressed);
            break;
         case 'u':
            tryRead("uncompressed_size", &this->zfs.uncompressed);
            break;
         case 's':
            tryRead("size", &this->zfs.size);
            break;
         case 'h':
            tryRead("hdr_size", &this->zfs.header);
            break;
         case 'd':
            tryRead("dbuf_size", &dbufSize);
            tryRead("dnode_size", &dnodeSize);
            break;
         case 'b':
            tryRead("bonus_size", &bonusSize);
            break;
         case 'a':
            tryRead("anon_size", &this->zfs.anon);
            break;
         case 'm':
            tryRead("mfu_size", &this->zfs.MFU);
            tryRead("mru_size", &this->zfs.MRU);
            break;
      }

      #undef tryRead
      #undef tryReadFlag
   }
   fclose(file);

   this->zfs.enabled = (this->zfs.size > 0 ? 1 : 0);
   this->zfs.size   /= 1024;
   this->zfs.min    /= 1024;
   this->zfs.max    /= 1024;
   this->zfs.MFU    /= 1024;
   this->zfs.MRU    /= 1024;
   this->zfs.anon   /= 1024;
   this->zfs.header /= 1024;
   this->zfs.other   = (dbufSize + dnodeSize + bonusSize) / 1024;
   if ( this->zfs.isCompressed ) {
      this->zfs.compressed /= 1024;
      this->zfs.uncompressed /= 1024;
   }
}

static void LinuxMachine_scanCPUTime(LinuxMachine* this) {
   const Machine* super = &this->super;

   LinuxMachine_updateCPUcount(this);

   FILE* file = fopen(PROCSTATFILE, "r");
   if (!file)
      CRT_fatalError("Cannot open " PROCSTATFILE);

   unsigned int lastAdjCpuId = 0;

   for (unsigned int i = 0; i <= super->existingCPUs; i++) {
      char buffer[PROC_LINE_LENGTH + 1];
      unsigned long long int usertime, nicetime, systemtime, idletime;
      unsigned long long int ioWait = 0, irq = 0, softIrq = 0, steal = 0, guest = 0, guestnice = 0;

      const char* ok = fgets(buffer, sizeof(buffer), file);
      if (!ok)
         break;

      // cpu fields are sorted first
      if (!String_startsWith(buffer, "cpu"))
         break;

      // Depending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      unsigned int adjCpuId;
      if (i == 0) {
         (void) sscanf(buffer, "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         adjCpuId = 0;
      } else {
         unsigned int cpuid;
         (void) sscanf(buffer, "cpu%4u %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         adjCpuId = cpuid + 1;
      }

      if (adjCpuId > super->existingCPUs)
         break;

      for (unsigned int j = lastAdjCpuId + 1; j < adjCpuId; j++) {
         // Skipped an ID, but /proc/stat is ordered => got offline CPU
         memset(&(this->cpuData[j]), '\0', sizeof(CPUData));
      }
      lastAdjCpuId = adjCpuId;

      // Guest time is already accounted in usertime
      usertime -= guest;
      nicetime -= guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      unsigned long long int idlealltime = idletime + ioWait;
      unsigned long long int systemalltime = systemtime + irq + softIrq;
      unsigned long long int virtalltime = guest + guestnice;
      unsigned long long int totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      CPUData* cpuData = &(this->cpuData[adjCpuId]);
      // Since we do a subtraction (usertime - guest) and cputime64_to_clock_t()
      // used in /proc/stat rounds down numbers, it can lead to a case where the
      // integer overflow.
      cpuData->userPeriod = saturatingSub(usertime, cpuData->userTime);
      cpuData->nicePeriod = saturatingSub(nicetime, cpuData->niceTime);
      cpuData->systemPeriod = saturatingSub(systemtime, cpuData->systemTime);
      cpuData->systemAllPeriod = saturatingSub(systemalltime, cpuData->systemAllTime);
      cpuData->idleAllPeriod = saturatingSub(idlealltime, cpuData->idleAllTime);
      cpuData->idlePeriod = saturatingSub(idletime, cpuData->idleTime);
      cpuData->ioWaitPeriod = saturatingSub(ioWait, cpuData->ioWaitTime);
      cpuData->irqPeriod = saturatingSub(irq, cpuData->irqTime);
      cpuData->softIrqPeriod = saturatingSub(softIrq, cpuData->softIrqTime);
      cpuData->stealPeriod = saturatingSub(steal, cpuData->stealTime);
      cpuData->guestPeriod = saturatingSub(virtalltime, cpuData->guestTime);
      cpuData->totalPeriod = saturatingSub(totaltime, cpuData->totalTime);
      cpuData->userTime = usertime;
      cpuData->niceTime = nicetime;
      cpuData->systemTime = systemtime;
      cpuData->systemAllTime = systemalltime;
      cpuData->idleAllTime = idlealltime;
      cpuData->idleTime = idletime;
      cpuData->ioWaitTime = ioWait;
      cpuData->irqTime = irq;
      cpuData->softIrqTime = softIrq;
      cpuData->stealTime = steal;
      cpuData->guestTime = virtalltime;
      cpuData->totalTime = totaltime;
   }

   this->period = (double)this->cpuData[0].totalPeriod / super->activeCPUs;

   char buffer[PROC_LINE_LENGTH + 1];
   while (fgets(buffer, sizeof(buffer), file)) {
      if (String_startsWith(buffer, "procs_running")) {
         ProcessTable* pt = (ProcessTable*) super->processTable;
         pt->runningTasks = strtoul(buffer + strlen("procs_running"), NULL, 10);
         break;
      }
   }

   fclose(file);
}

static int scanCPUFrequencyFromSysCPUFreq(LinuxMachine* this) {
   const Machine* super = &this->super;
   int numCPUsWithFrequency = 0;
   unsigned long totalFrequency = 0;

   /*
    * On some AMD and Intel CPUs read()ing scaling_cur_freq is quite slow (> 1ms). This delay
    * accumulates for every core. For details see issue#471.
    * If the read on CPU 0 takes longer than 500us bail out and fall back to reading the
    * frequencies from /proc/cpuinfo.
    * Once the condition has been met, bail out early for the next couple of scans.
    */
   static int timeout = 0;

   if (timeout > 0) {
      timeout--;
      return -1;
   }

   for (unsigned int i = 0; i < super->existingCPUs; ++i) {
      if (!Machine_isCPUonline(super, i))
         continue;

      char pathBuffer[64];
      xSnprintf(pathBuffer, sizeof(pathBuffer), "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", i);

      struct timespec start;
      if (i == 0)
         clock_gettime(CLOCK_MONOTONIC, &start);

      FILE* file = fopen(pathBuffer, "r");
      if (!file)
         return -errno;

      unsigned long frequency;
      if (fscanf(file, "%lu", &frequency) == 1) {
         /* convert kHz to MHz */
         frequency = frequency / 1000;
         this->cpuData[i + 1].frequency = frequency;
         numCPUsWithFrequency++;
         totalFrequency += frequency;
      }

      fclose(file);

      if (i == 0) {
         struct timespec end;
         clock_gettime(CLOCK_MONOTONIC, &end);
         const time_t timeTakenUs = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
         if (timeTakenUs > 500) {
            timeout = 30;
            return -1;
         }
      }
   }

   if (numCPUsWithFrequency > 0)
      this->cpuData[0].frequency = (double)totalFrequency / numCPUsWithFrequency;

   return 0;
}

static void scanCPUFrequencyFromCPUinfo(LinuxMachine* this) {
   const Machine* super = &this->super;

   FILE* file = fopen(PROCCPUINFOFILE, "r");
   if (file == NULL)
      return;

   int numCPUsWithFrequency = 0;
   double totalFrequency = 0;
   int cpuid = -1;

   while (!feof(file)) {
      double frequency;
      char buffer[PROC_LINE_LENGTH];

      if (fgets(buffer, PROC_LINE_LENGTH, file) == NULL)
         break;

      if (sscanf(buffer, "processor : %d", &cpuid) == 1) {
         continue;
      } else if (
         (sscanf(buffer, "cpu MHz : %lf", &frequency) == 1) ||
         (sscanf(buffer, "clock : %lfMHz", &frequency) == 1)
      ) {
         if (cpuid < 0 || (unsigned int)cpuid > (super->existingCPUs - 1)) {
            continue;
         }

         CPUData* cpuData = &(this->cpuData[cpuid + 1]);
         /* do not override sysfs data */
         if (!isNonnegative(cpuData->frequency)) {
            cpuData->frequency = frequency;
         }
         numCPUsWithFrequency++;
         totalFrequency += frequency;
      } else if (buffer[0] == '\n') {
         cpuid = -1;
      }
   }
   fclose(file);

   if (numCPUsWithFrequency > 0) {
      this->cpuData[0].frequency = totalFrequency / numCPUsWithFrequency;
   }
}

static void LinuxMachine_scanCPUFrequency(LinuxMachine* this) {
   const Machine* super = &this->super;

   for (unsigned int i = 0; i <= super->existingCPUs; i++)
      this->cpuData[i].frequency = NAN;

   if (scanCPUFrequencyFromSysCPUFreq(this) == 0)
      return;

   scanCPUFrequencyFromCPUinfo(this);
}

void Machine_scan(Machine* super) {
   LinuxMachine* this = (LinuxMachine*) super;

   LinuxMachine_scanMemoryInfo(this);
   LinuxMachine_scanHugePages(this);
   LinuxMachine_scanZfsArcstats(this);
   LinuxMachine_scanZramInfo(this);
   LinuxMachine_scanCPUTime(this);

   const Settings* settings = super->settings;
   if (settings->showCPUFrequency)
      LinuxMachine_scanCPUFrequency(this);

   #ifdef HAVE_SENSORS_SENSORS_H
   if (settings->showCPUTemperature)
      LibSensors_getCPUTemperatures(this->cpuData, super->existingCPUs, super->activeCPUs);
   #endif
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   LinuxMachine* this = xCalloc(1, sizeof(LinuxMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   // Initialize page size
   if ((this->pageSize = sysconf(_SC_PAGESIZE)) == -1)
      CRT_fatalError("Cannot get pagesize by sysconf(_SC_PAGESIZE)");
   this->pageSizeKB = this->pageSize / ONE_K;

   // Initialize clock ticks
   if ((this->jiffies = sysconf(_SC_CLK_TCK)) == -1)
      CRT_fatalError("Cannot get clock ticks by sysconf(_SC_CLK_TCK)");

   // Read btime (the kernel boot time, as number of seconds since the epoch)
   FILE* statfile = fopen(PROCSTATFILE, "r");
   if (statfile == NULL)
      CRT_fatalError("Cannot open " PROCSTATFILE);

   this->boottime = -1;

   while (true) {
      char buffer[PROC_LINE_LENGTH + 1];
      if (fgets(buffer, sizeof(buffer), statfile) == NULL)
         break;
      if (String_startsWith(buffer, "btime ") == false)
         continue;
      if (sscanf(buffer, "btime %lld\n", &this->boottime) == 1)
         break;
      CRT_fatalError("Failed to parse btime from " PROCSTATFILE);
   }
   fclose(statfile);

   if (this->boottime == -1)
      CRT_fatalError("No btime in " PROCSTATFILE);

   // Initialize CPU count
   LinuxMachine_updateCPUcount(this);

   return super;
}

void Machine_delete(Machine* super) {
   LinuxMachine* this = (LinuxMachine*) super;
   Machine_done(super);
   free(this->cpuData);
   free(this);
}

bool Machine_isCPUonline(const Machine* super, unsigned int id) {
   const LinuxMachine* this = (const LinuxMachine*) super;

   assert(id < super->existingCPUs);
   return this->cpuData[id + 1].online;
}
