/*
htop - linux/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include "linux/Platform.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "BatteryMeter.h"
#include "ClockMeter.h"
#include "Compat.h"
#include "CPUMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "DiskIOMeter.h"
#include "HostnameMeter.h"
#include "HugePageMeter.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MainPanel.h"
#include "Meter.h"
#include "MemoryMeter.h"
#include "MemorySwapMeter.h"
#include "NetworkIOMeter.h"
#include "Object.h"
#include "Panel.h"
#include "PressureStallMeter.h"
#include "ProcessList.h"
#include "ProvideCurses.h"
#include "linux/SELinuxMeter.h"
#include "Settings.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "linux/IOPriority.h"
#include "linux/IOPriorityPanel.h"
#include "linux/LinuxProcess.h"
#include "linux/LinuxProcessList.h"
#include "linux/SystemdMeter.h"
#include "linux/ZramMeter.h"
#include "linux/ZramStats.h"
#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsArcStats.h"
#include "zfs/ZfsCompressedArcMeter.h"

#ifdef HAVE_LIBCAP
#include <errno.h>
#include <sys/capability.h>
#endif

#ifdef HAVE_SENSORS_SENSORS_H
#include "LibSensors.h"
#endif

#ifndef O_PATH
#define O_PATH         010000000 // declare for ancient glibc versions
#endif


#ifdef HAVE_LIBCAP
enum CapMode {
   CAP_MODE_OFF,
   CAP_MODE_BASIC,
   CAP_MODE_STRICT
};
#endif

bool Running_containerized = false;

const ScreenDefaults Platform_defaultScreens[] = {
   {
      .name = "Main",
      .columns = "PID USER PRIORITY NICE M_VIRT M_RESIDENT M_SHARE STATE PERCENT_CPU PERCENT_MEM TIME Command",
      .sortKey = "PERCENT_CPU",
   },
   {
      .name = "I/O",
      .columns = "PID USER IO_PRIORITY IO_RATE IO_READ_RATE IO_WRITE_RATE PERCENT_SWAP_DELAY PERCENT_IO_DELAY Command",
      .sortKey = "IO_RATE",
   },
};

const unsigned int Platform_numberOfDefaultScreens = ARRAYSIZE(Platform_defaultScreens);

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number = 0 },
   { .name = " 1 SIGHUP",    .number = 1 },
   { .name = " 2 SIGINT",    .number = 2 },
   { .name = " 3 SIGQUIT",   .number = 3 },
   { .name = " 4 SIGILL",    .number = 4 },
   { .name = " 5 SIGTRAP",   .number = 5 },
   { .name = " 6 SIGABRT",   .number = 6 },
   { .name = " 6 SIGIOT",    .number = 6 },
   { .name = " 7 SIGBUS",    .number = 7 },
   { .name = " 8 SIGFPE",    .number = 8 },
   { .name = " 9 SIGKILL",   .number = 9 },
   { .name = "10 SIGUSR1",   .number = 10 },
   { .name = "11 SIGSEGV",   .number = 11 },
   { .name = "12 SIGUSR2",   .number = 12 },
   { .name = "13 SIGPIPE",   .number = 13 },
   { .name = "14 SIGALRM",   .number = 14 },
   { .name = "15 SIGTERM",   .number = 15 },
   { .name = "16 SIGSTKFLT", .number = 16 },
   { .name = "17 SIGCHLD",   .number = 17 },
   { .name = "18 SIGCONT",   .number = 18 },
   { .name = "19 SIGSTOP",   .number = 19 },
   { .name = "20 SIGTSTP",   .number = 20 },
   { .name = "21 SIGTTIN",   .number = 21 },
   { .name = "22 SIGTTOU",   .number = 22 },
   { .name = "23 SIGURG",    .number = 23 },
   { .name = "24 SIGXCPU",   .number = 24 },
   { .name = "25 SIGXFSZ",   .number = 25 },
   { .name = "26 SIGVTALRM", .number = 26 },
   { .name = "27 SIGPROF",   .number = 27 },
   { .name = "28 SIGWINCH",  .number = 28 },
   { .name = "29 SIGIO",     .number = 29 },
   { .name = "29 SIGPOLL",   .number = 29 },
   { .name = "30 SIGPWR",    .number = 30 },
   { .name = "31 SIGSYS",    .number = 31 },
};

const unsigned int Platform_numberOfSignals = ARRAYSIZE(Platform_signals);

static enum { BAT_PROC, BAT_SYS, BAT_ERR } Platform_Battery_method = BAT_PROC;
static time_t Platform_Battery_cacheTime;
static double Platform_Battery_cachePercent = NAN;
static ACPresence Platform_Battery_cacheIsOnAC;

#ifdef HAVE_LIBCAP
static enum CapMode Platform_capabilitiesMode = CAP_MODE_BASIC;
#endif

static Htop_Reaction Platform_actionSetIOPriority(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   const LinuxProcess* p = (const LinuxProcess*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return HTOP_OK;

   IOPriority ioprio1 = p->ioPriority;
   Panel* ioprioPanel = IOPriorityPanel_new(ioprio1);
   const void* set = Action_pickFromVector(st, ioprioPanel, 20, true);
   if (set) {
      IOPriority ioprio2 = IOPriorityPanel_getIOPriority(ioprioPanel);
      bool ok = MainPanel_foreachProcess(st->mainPanel, LinuxProcess_setIOPriority, (Arg) { .i = ioprio2 }, NULL);
      if (!ok) {
         beep();
      }
   }
   Panel_delete((Object*)ioprioPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

static bool Platform_changeAutogroupPriority(MainPanel* panel, int delta) {
   if (LinuxProcess_isAutogroupEnabled() == false) {
      beep();
      return false;
   }
   bool anyTagged;
   bool ok = MainPanel_foreachProcess(panel, LinuxProcess_changeAutogroupPriorityBy, (Arg) { .i = delta }, &anyTagged);
   if (!ok)
      beep();
   return anyTagged;
}

static Htop_Reaction Platform_actionHigherAutogroupPriority(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   bool changed = Platform_changeAutogroupPriority(st->mainPanel, -1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

static Htop_Reaction Platform_actionLowerAutogroupPriority(State* st) {
   if (Settings_isReadonly())
      return HTOP_OK;

   bool changed = Platform_changeAutogroupPriority(st->mainPanel, 1);
   return changed ? HTOP_REFRESH : HTOP_OK;
}

void Platform_setBindings(Htop_Action* keys) {
   keys['i'] = Platform_actionSetIOPriority;
   keys['{'] = Platform_actionLowerAutogroupPriority;
   keys['}'] = Platform_actionHigherAutogroupPriority;
   keys[KEY_F(19)] = Platform_actionLowerAutogroupPriority;  // Shift-F7
   keys[KEY_F(20)] = Platform_actionHigherAutogroupPriority; // Shift-F8
}

const MeterClass* const Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &DateMeter_class,
   &DateTimeMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &MemorySwapMeter_class,
   &SysArchMeter_class,
   &HugePageMeter_class,
   &TasksMeter_class,
   &UptimeMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &AllCPUs4Meter_class,
   &AllCPUs8Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &LeftCPUs4Meter_class,
   &RightCPUs4Meter_class,
   &LeftCPUs8Meter_class,
   &RightCPUs8Meter_class,
   &BlankMeter_class,
   &PressureStallCPUSomeMeter_class,
   &PressureStallIOSomeMeter_class,
   &PressureStallIOFullMeter_class,
   &PressureStallMemorySomeMeter_class,
   &PressureStallMemoryFullMeter_class,
   &ZfsArcMeter_class,
   &ZfsCompressedArcMeter_class,
   &ZramMeter_class,
   &DiskIOMeter_class,
   &NetworkIOMeter_class,
   &SELinuxMeter_class,
   &SystemdMeter_class,
   NULL
};

int Platform_getUptime() {
   double uptime = 0;
   FILE* fd = fopen(PROCDIR "/uptime", "r");
   if (fd) {
      int n = fscanf(fd, "%64lf", &uptime);
      fclose(fd);
      if (n <= 0) {
         return 0;
      }
   }
   return floor(uptime);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   FILE* fd = fopen(PROCDIR "/loadavg", "r");
   if (!fd)
      goto err;

   double scanOne, scanFive, scanFifteen;
   int r = fscanf(fd, "%lf %lf %lf", &scanOne, &scanFive, &scanFifteen);
   fclose(fd);
   if (r != 3)
      goto err;

   *one = scanOne;
   *five = scanFive;
   *fifteen = scanFifteen;
   return;

err:
   *one = NAN;
   *five = NAN;
   *fifteen = NAN;
}

int Platform_getMaxPid() {
   FILE* file = fopen(PROCDIR "/sys/kernel/pid_max", "r");
   if (!file)
      return -1;

   int maxPid = 4194303;
   int match = fscanf(file, "%32d", &maxPid);
   (void) match;
   fclose(file);
   return maxPid;
}

double Platform_setCPUValues(Meter* this, unsigned int cpu) {
   const LinuxProcessList* pl = (const LinuxProcessList*) this->pl;
   const CPUData* cpuData = &(pl->cpuData[cpu]);
   double total = (double) ( cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod);
   double percent;
   double* v = this->values;

   if (!cpuData->online) {
      this->curItems = 0;
      return NAN;
   }

   v[CPU_METER_NICE] = cpuData->nicePeriod / total * 100.0;
   v[CPU_METER_NORMAL] = cpuData->userPeriod / total * 100.0;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPeriod / total * 100.0;
      v[CPU_METER_IRQ]     = cpuData->irqPeriod / total * 100.0;
      v[CPU_METER_SOFTIRQ] = cpuData->softIrqPeriod / total * 100.0;
      v[CPU_METER_STEAL]   = cpuData->stealPeriod / total * 100.0;
      v[CPU_METER_GUEST]   = cpuData->guestPeriod / total * 100.0;
      v[CPU_METER_IOWAIT]  = cpuData->ioWaitPeriod / total * 100.0;
      this->curItems = 8;
      if (this->pl->settings->accountGuestInCPUMeter) {
         percent = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6];
      } else {
         percent = v[0] + v[1] + v[2] + v[3] + v[4];
      }
   } else {
      v[2] = cpuData->systemAllPeriod / total * 100.0;
      v[3] = (cpuData->stealPeriod + cpuData->guestPeriod) / total * 100.0;
      this->curItems = 4;
      percent = v[0] + v[1] + v[2] + v[3];
   }
   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) {
      percent = 0.0;
   }

   v[CPU_METER_FREQUENCY] = cpuData->frequency;

#ifdef HAVE_SENSORS_SENSORS_H
   v[CPU_METER_TEMPERATURE] = cpuData->temperature;
#else
   v[CPU_METER_TEMPERATURE] = NAN;
#endif

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   const ProcessList* pl = this->pl;
   const LinuxProcessList* lpl = (const LinuxProcessList*) pl;

   this->total     = pl->totalMem;
   this->values[0] = pl->usedMem;
   this->values[1] = pl->buffersMem;
   this->values[2] = pl->sharedMem;
   this->values[3] = pl->cachedMem;
   this->values[4] = pl->availableMem;

   if (lpl->zfs.enabled != 0 && !Running_containerized) {
      // ZFS does not shrink below the value of zfs_arc_min.
      unsigned long long int shrinkableSize = 0;
      if (lpl->zfs.size > lpl->zfs.min)
         shrinkableSize = lpl->zfs.size - lpl->zfs.min;
      this->values[0] -= shrinkableSize;
      this->values[3] += shrinkableSize;
      this->values[4] += shrinkableSize;
   }
}

void Platform_setSwapValues(Meter* this) {
   const ProcessList* pl = this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
   this->values[1] = pl->cachedSwap;
}

void Platform_setZramValues(Meter* this) {
   const LinuxProcessList* lpl = (const LinuxProcessList*) this->pl;
   this->total = lpl->zram.totalZram;
   this->values[0] = lpl->zram.usedZramComp;
   this->values[1] = lpl->zram.usedZramOrig;
}

void Platform_setZfsArcValues(Meter* this) {
   const LinuxProcessList* lpl = (const LinuxProcessList*) this->pl;

   ZfsArcMeter_readStats(this, &(lpl->zfs));
}

void Platform_setZfsCompressedArcValues(Meter* this) {
   const LinuxProcessList* lpl = (const LinuxProcessList*) this->pl;

   ZfsCompressedArcMeter_readStats(this, &(lpl->zfs));
}

char* Platform_getProcessEnv(pid_t pid) {
   char procname[128];
   xSnprintf(procname, sizeof(procname), PROCDIR "/%d/environ", pid);
   FILE* fd = fopen(procname, "r");
   if (!fd)
      return NULL;

   char* env = NULL;

   size_t capacity = 0;
   size_t size = 0;
   ssize_t bytes = 0;

   do {
      size += bytes;
      capacity += 4096;
      env = xRealloc(env, capacity);
   } while ((bytes = fread(env + size, 1, capacity - size, fd)) > 0);

   fclose(fd);

   if (bytes < 0) {
      free(env);
      return NULL;
   }

   size += bytes;

   env = xRealloc(env, size + 2);

   env[size] = '\0';
   env[size + 1] = '\0';

   return env;
}

/*
 * Return the absolute path of a file given its pid&inode number
 *
 * Based on implementation of lslocks from util-linux:
 * https://sources.debian.org/src/util-linux/2.36-3/misc-utils/lslocks.c/#L162
 */
char* Platform_getInodeFilename(pid_t pid, ino_t inode) {
   struct stat sb;
   const struct dirent* de;
   DIR* dirp;
   ssize_t len;
   int fd;

   char path[PATH_MAX];
   char sym[PATH_MAX];
   char* ret = NULL;

   memset(path, 0, sizeof(path));
   memset(sym, 0, sizeof(sym));

   xSnprintf(path, sizeof(path), "%s/%d/fd/", PROCDIR, pid);
   if (strlen(path) >= (sizeof(path) - 2))
      return NULL;

   if (!(dirp = opendir(path)))
      return NULL;

   if ((fd = dirfd(dirp)) < 0 )
      goto out;

   while ((de = readdir(dirp))) {
      if (String_eq(de->d_name, ".") || String_eq(de->d_name, ".."))
         continue;

      /* care only for numerical descriptors */
      if (!strtoull(de->d_name, (char **) NULL, 10))
         continue;

      if (!Compat_fstatat(fd, path, de->d_name, &sb, 0) && inode != sb.st_ino)
         continue;

      if ((len = Compat_readlinkat(fd, path, de->d_name, sym, sizeof(sym) - 1)) < 1)
         goto out;

      sym[len] = '\0';

      ret = xStrdup(sym);
      break;
   }

out:
   closedir(dirp);
   return ret;
}

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid) {
   FileLocks_ProcessData* pdata = xCalloc(1, sizeof(FileLocks_ProcessData));

   FILE* f = fopen(PROCDIR "/locks", "r");
   if (!f) {
      pdata->error = true;
      return pdata;
   }

   char buffer[1024];
   FileLocks_LockData** data_ref = &pdata->locks;
   while(fgets(buffer, sizeof(buffer), f)) {
      if (!strchr(buffer, '\n'))
         continue;

      int lock_id;
      char lock_type[16];
      char lock_excl[16];
      char lock_rw[16];
      pid_t lock_pid;
      unsigned int lock_dev[2];
      uint64_t lock_inode;
      char lock_start[25];
      char lock_end[25];

      if (10 != sscanf(buffer, "%d:  %15s  %15s %15s %d %x:%x:%"PRIu64" %24s %24s",
         &lock_id, lock_type, lock_excl, lock_rw, &lock_pid,
         &lock_dev[0], &lock_dev[1], &lock_inode,
         lock_start, lock_end))
         continue;

      if (pid != lock_pid)
         continue;

      FileLocks_LockData* ldata = xCalloc(1, sizeof(FileLocks_LockData));
      FileLocks_Data* data = &ldata->data;
      data->id = lock_id;
      data->locktype = xStrdup(lock_type);
      data->exclusive = xStrdup(lock_excl);
      data->readwrite = xStrdup(lock_rw);
      data->filename = Platform_getInodeFilename(lock_pid, lock_inode);
      data->dev[0] = lock_dev[0];
      data->dev[1] = lock_dev[1];
      data->inode = lock_inode;
      data->start = strtoull(lock_start, NULL, 10);
      if (!String_eq(lock_end, "EOF")) {
         data->end = strtoull(lock_end, NULL, 10);
      } else {
         data->end = ULLONG_MAX;
      }

      *data_ref = ldata;
      data_ref = &ldata->next;
   }

   fclose(f);
   return pdata;
}

void Platform_getPressureStall(const char* file, bool some, double* ten, double* sixty, double* threehundred) {
   *ten = *sixty = *threehundred = 0;
   char procname[128];
   xSnprintf(procname, sizeof(procname), PROCDIR "/pressure/%s", file);
   FILE* fd = fopen(procname, "r");
   if (!fd) {
      *ten = *sixty = *threehundred = NAN;
      return;
   }
   int total = fscanf(fd, "some avg10=%32lf avg60=%32lf avg300=%32lf total=%*f ", ten, sixty, threehundred);
   if (!some) {
      total = fscanf(fd, "full avg10=%32lf avg60=%32lf avg300=%32lf total=%*f ", ten, sixty, threehundred);
   }
   (void) total;
   assert(total == 3);
   fclose(fd);
}

bool Platform_getDiskIO(DiskIOData* data) {
   FILE* fd = fopen(PROCDIR "/diskstats", "r");
   if (!fd)
      return false;

   char lastTopDisk[32] = { '\0' };

   unsigned long long int read_sum = 0, write_sum = 0, timeSpend_sum = 0;
   char lineBuffer[256];
   while (fgets(lineBuffer, sizeof(lineBuffer), fd)) {
      char diskname[32];
      unsigned long long int read_tmp, write_tmp, timeSpend_tmp;
      if (sscanf(lineBuffer, "%*d %*d %31s %*u %*u %llu %*u %*u %*u %llu %*u %*u %llu", diskname, &read_tmp, &write_tmp, &timeSpend_tmp) == 4) {
         if (String_startsWith(diskname, "dm-"))
            continue;

         if (String_startsWith(diskname, "zram"))
            continue;

         /* only count root disks, e.g. do not count IO from sda and sda1 twice */
         if (lastTopDisk[0] && String_startsWith(diskname, lastTopDisk))
            continue;

         /* This assumes disks are listed directly before any of their partitions */
         String_safeStrncpy(lastTopDisk, diskname, sizeof(lastTopDisk));

         read_sum += read_tmp;
         write_sum += write_tmp;
         timeSpend_sum += timeSpend_tmp;
      }
   }
   fclose(fd);
   /* multiply with sector size */
   data->totalBytesRead = 512 * read_sum;
   data->totalBytesWritten = 512 * write_sum;
   data->totalMsTimeSpend = timeSpend_sum;
   return true;
}

bool Platform_getNetworkIO(NetworkIOData* data) {
   FILE* fd = fopen(PROCDIR "/net/dev", "r");
   if (!fd)
      return false;

   memset(data, 0, sizeof(NetworkIOData));
   char lineBuffer[512];
   while (fgets(lineBuffer, sizeof(lineBuffer), fd)) {
      char interfaceName[32];
      unsigned long long int bytesReceived, packetsReceived, bytesTransmitted, packetsTransmitted;
      if (sscanf(lineBuffer, "%31s %llu %llu %*u %*u %*u %*u %*u %*u %llu %llu",
                             interfaceName,
                             &bytesReceived,
                             &packetsReceived,
                             &bytesTransmitted,
                             &packetsTransmitted) != 5)
         continue;

      if (String_eq(interfaceName, "lo:"))
         continue;

      data->bytesReceived += bytesReceived;
      data->packetsReceived += packetsReceived;
      data->bytesTransmitted += bytesTransmitted;
      data->packetsTransmitted += packetsTransmitted;
   }

   fclose(fd);

   return true;
}

// Linux battery reading by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).

#define PROC_BATTERY_DIR PROCDIR "/acpi/battery"
#define PROC_POWERSUPPLY_DIR PROCDIR "/acpi/ac_adapter"
#define PROC_POWERSUPPLY_ACSTATE_FILE PROC_POWERSUPPLY_DIR "/AC/state"
#define SYS_POWERSUPPLY_DIR "/sys/class/power_supply"

// ----------------------------------------
// READ FROM /proc
// ----------------------------------------

static double Platform_Battery_getProcBatInfo(void) {
   DIR* batteryDir = opendir(PROC_BATTERY_DIR);
   if (!batteryDir)
      return NAN;

   uint64_t totalFull = 0;
   uint64_t totalRemain = 0;

   struct dirent* dirEntry = NULL;
   while ((dirEntry = readdir(batteryDir))) {
      const char* entryName = dirEntry->d_name;
      if (!String_startsWith(entryName, "BAT"))
         continue;

      char filePath[256];
      char bufInfo[1024] = {0};
      xSnprintf(filePath, sizeof(filePath), "%s/%s/info", PROC_BATTERY_DIR, entryName);
      ssize_t r = xReadfile(filePath, bufInfo, sizeof(bufInfo));
      if (r < 0)
         continue;

      char bufState[1024] = {0};
      xSnprintf(filePath, sizeof(filePath), "%s/%s/state", PROC_BATTERY_DIR, entryName);
      r = xReadfile(filePath, bufState, sizeof(bufState));
      if (r < 0)
         continue;

      const char* line;

      //Getting total charge for all batteries
      char* buf = bufInfo;
      while ((line = strsep(&buf, "\n")) != NULL) {
         char field[100] = {0};
         int val = 0;
         if (2 != sscanf(line, "%99[^:]:%d", field, &val))
            continue;

         if (String_eq(field, "last full capacity")) {
            totalFull += val;
            break;
         }
      }

      //Getting remaining charge for all batteries
      buf = bufState;
      while ((line = strsep(&buf, "\n")) != NULL) {
         char field[100] = {0};
         int val = 0;
         if (2 != sscanf(line, "%99[^:]:%d", field, &val))
            continue;

         if (String_eq(field, "remaining capacity")) {
            totalRemain += val;
            break;
         }
      }
   }

   closedir(batteryDir);

   return totalFull > 0 ? ((double) totalRemain * 100.0) / (double) totalFull : NAN;
}

static ACPresence procAcpiCheck(void) {
   char buffer[1024] = {0};
   ssize_t r = xReadfile(PROC_POWERSUPPLY_ACSTATE_FILE, buffer, sizeof(buffer));
   if (r < 1)
      return AC_ERROR;

   return String_eq(buffer, "on-line") ? AC_PRESENT : AC_ABSENT;
}

static void Platform_Battery_getProcData(double* percent, ACPresence* isOnAC) {
   *isOnAC = procAcpiCheck();
   *percent = AC_ERROR != *isOnAC ? Platform_Battery_getProcBatInfo() : NAN;
}

// ----------------------------------------
// READ FROM /sys
// ----------------------------------------

static void Platform_Battery_getSysData(double* percent, ACPresence* isOnAC) {
   *percent = NAN;
   *isOnAC = AC_ERROR;

   DIR* dir = opendir(SYS_POWERSUPPLY_DIR);
   if (!dir)
      return;

   uint64_t totalFull = 0;
   uint64_t totalRemain = 0;

   const struct dirent* dirEntry;
   while ((dirEntry = readdir(dir))) {
      const char* entryName = dirEntry->d_name;

#ifdef HAVE_OPENAT
      int entryFd = openat(dirfd(dir), entryName, O_DIRECTORY | O_PATH);
      if (entryFd < 0)
         continue;
#else
      char entryFd[4096];
      xSnprintf(entryFd, sizeof(entryFd), SYS_POWERSUPPLY_DIR "/%s", entryName);
#endif

      enum { AC, BAT } type;
      if (String_startsWith(entryName, "BAT")) {
         type = BAT;
      } else if (String_startsWith(entryName, "AC")) {
         type = AC;
      } else {
         char buffer[32];
         ssize_t ret = xReadfileat(entryFd, "type", buffer, sizeof(buffer));
         if (ret <= 0)
            goto next;

         /* drop optional trailing newlines */
         for (char* buf = &buffer[(size_t)ret - 1]; *buf == '\n'; buf--)
            *buf = '\0';

         if (String_eq(buffer, "Battery"))
            type = BAT;
         else if (String_eq(buffer, "Mains"))
            type = AC;
         else
            goto next;
      }

      if (type == BAT) {
         char buffer[1024];
         ssize_t r = xReadfileat(entryFd, "uevent", buffer, sizeof(buffer));
         if (r < 0)
            goto next;

         bool full = false;
         bool now = false;

         double fullCharge = 0;
         double capacityLevel = NAN;
         const char* line;

         char* buf = buffer;
         while ((line = strsep(&buf, "\n")) != NULL) {
            char field[100] = {0};
            int val = 0;
            if (2 != sscanf(line, "POWER_SUPPLY_%99[^=]=%d", field, &val))
               continue;

            if (String_eq(field, "CAPACITY")) {
               capacityLevel = val / 100.0;
               continue;
            }

            if (String_eq(field, "ENERGY_FULL") || String_eq(field, "CHARGE_FULL")) {
               fullCharge = val;
               totalFull += fullCharge;
               full = true;
               if (now)
                  break;
               continue;
            }

            if (String_eq(field, "ENERGY_NOW") || String_eq(field, "CHARGE_NOW")) {
               totalRemain += val;
               now = true;
               if (full)
                  break;
               continue;
            }
         }

         if (!now && full && !isnan(capacityLevel))
            totalRemain += capacityLevel * fullCharge;

      } else if (type == AC) {
         if (*isOnAC != AC_ERROR)
            goto next;

         char buffer[2];
         ssize_t r = xReadfileat(entryFd, "online", buffer, sizeof(buffer));
         if (r < 1) {
            *isOnAC = AC_ERROR;
            goto next;
         }

         if (buffer[0] == '0')
            *isOnAC = AC_ABSENT;
         else if (buffer[0] == '1')
            *isOnAC = AC_PRESENT;
      }

next:
      Compat_openatArgClose(entryFd);
   }

   closedir(dir);

   *percent = totalFull > 0 ? ((double) totalRemain * 100.0) / (double) totalFull : NAN;
}

void Platform_getBattery(double* percent, ACPresence* isOnAC) {
   time_t now = time(NULL);
   // update battery reading is slow. Update it each 10 seconds only.
   if (now < Platform_Battery_cacheTime + 10) {
      *percent = Platform_Battery_cachePercent;
      *isOnAC = Platform_Battery_cacheIsOnAC;
      return;
   }

   if (Platform_Battery_method == BAT_PROC) {
      Platform_Battery_getProcData(percent, isOnAC);
      if (isnan(*percent))
         Platform_Battery_method = BAT_SYS;
   }
   if (Platform_Battery_method == BAT_SYS) {
      Platform_Battery_getSysData(percent, isOnAC);
      if (isnan(*percent))
         Platform_Battery_method = BAT_ERR;
   }
   if (Platform_Battery_method == BAT_ERR) {
      *percent = NAN;
      *isOnAC = AC_ERROR;
   } else {
      *percent = CLAMP(*percent, 0.0, 100.0);
   }
   Platform_Battery_cachePercent = *percent;
   Platform_Battery_cacheIsOnAC = *isOnAC;
   Platform_Battery_cacheTime = now;
}

void Platform_longOptionsUsage(const char* name)
{
#ifdef HAVE_LIBCAP
   printf(
"   --drop-capabilities[=off|basic|strict] Drop Linux capabilities when running as root\n"
"                                off - do not drop any capabilities\n"
"                                basic (default) - drop all capabilities not needed by %s\n"
"                                strict - drop all capabilities except those needed for\n"
"                                         core functionality\n", name);
#else
   (void) name;
#endif
}

CommandLineStatus Platform_getLongOption(int opt, int argc, char** argv) {
#ifndef HAVE_LIBCAP
   (void) argc;
   (void) argv;
#endif

   switch (opt) {
#ifdef HAVE_LIBCAP
      case 160: {
         const char* mode = optarg;
         if (!mode && optind < argc && argv[optind] != NULL &&
            (argv[optind][0] != '\0' && argv[optind][0] != '-')) {
            mode = argv[optind++];
         }

         if (!mode || String_eq(mode, "basic")) {
            Platform_capabilitiesMode = CAP_MODE_BASIC;
         } else if (String_eq(mode, "off")) {
            Platform_capabilitiesMode = CAP_MODE_OFF;
         } else if (String_eq(mode, "strict")) {
            Platform_capabilitiesMode = CAP_MODE_STRICT;
         } else {
            fprintf(stderr, "Error: invalid capabilities mode \"%s\".\n", mode);
            return STATUS_ERROR_EXIT;
         }
         return STATUS_OK;
      }
#endif

      default:
         break;
   }
   return STATUS_ERROR_EXIT;
}

#ifdef HAVE_LIBCAP
static int dropCapabilities(enum CapMode mode) {

   if (mode == CAP_MODE_OFF)
      return 0;

   /* capabilities we keep to operate */
   const cap_value_t keepcapsStrict[] = {
      CAP_DAC_READ_SEARCH,
      CAP_SYS_PTRACE,
   };
   const cap_value_t keepcapsBasic[] = {
      CAP_DAC_READ_SEARCH,   /* read non world-readable process files of other users, like /proc/[pid]/io */
      CAP_KILL,              /* send signals to processes of other users */
      CAP_SYS_NICE,          /* lower process nice value / change nice value for arbitrary processes */
      CAP_SYS_PTRACE,        /* read /proc/[pid]/exe */
#ifdef HAVE_DELAYACCT
      CAP_NET_ADMIN,         /* communicate over netlink socket for delay accounting */
#endif
   };
   const cap_value_t* const keepcaps = (mode == CAP_MODE_BASIC) ? keepcapsBasic : keepcapsStrict;
   const size_t ncap = (mode == CAP_MODE_BASIC) ? ARRAYSIZE(keepcapsBasic) : ARRAYSIZE(keepcapsStrict);

   cap_t caps = cap_init();
   if (caps == NULL) {
      fprintf(stderr, "Error: can not initialize capabilities: %s\n", strerror(errno));
      return -1;
   }

   if (cap_clear(caps) < 0) {
      fprintf(stderr, "Error: can not clear capabilities: %s\n", strerror(errno));
      cap_free(caps);
      return -1;
   }

   cap_t currCaps = cap_get_proc();
   if (currCaps == NULL) {
      fprintf(stderr, "Error: can not get current process capabilities: %s\n", strerror(errno));
      cap_free(caps);
      return -1;
   }

   for (size_t i = 0; i < ncap; i++) {
      if (!CAP_IS_SUPPORTED(keepcaps[i]))
         continue;

      cap_flag_value_t current;
      if (cap_get_flag(currCaps, keepcaps[i], CAP_PERMITTED, &current) < 0) {
         fprintf(stderr, "Error: can not get current value of capability %d: %s\n", keepcaps[i], strerror(errno));
         cap_free(currCaps);
         cap_free(caps);
         return -1;
      }

      if (current != CAP_SET)
         continue;

      if (cap_set_flag(caps, CAP_PERMITTED, 1, &keepcaps[i], CAP_SET) < 0) {
         fprintf(stderr, "Error: can not set permitted capability %d: %s\n", keepcaps[i], strerror(errno));
         cap_free(currCaps);
         cap_free(caps);
         return -1;
      }

      if (cap_set_flag(caps, CAP_EFFECTIVE, 1, &keepcaps[i], CAP_SET) < 0) {
         fprintf(stderr, "Error: can not set effective capability %d: %s\n", keepcaps[i], strerror(errno));
         cap_free(currCaps);
         cap_free(caps);
         return -1;
      }
   }

   if (cap_set_proc(caps) < 0) {
      fprintf(stderr, "Error: can not set process capabilities: %s\n", strerror(errno));
      cap_free(currCaps);
      cap_free(caps);
      return -1;
   }

   cap_free(currCaps);
   cap_free(caps);

   return 0;
}
#endif

bool Platform_init(void) {
#ifdef HAVE_LIBCAP
   if (dropCapabilities(Platform_capabilitiesMode) < 0)
      return false;
#endif

   if (access(PROCDIR, R_OK) != 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      return false;
   }

#ifdef HAVE_SENSORS_SENSORS_H
   LibSensors_init();
#endif

   char target[PATH_MAX];
   ssize_t ret = readlink(PROCDIR "/self/ns/pid", target, sizeof(target) - 1);
   if (ret > 0) {
      target[ret] = '\0';

      if (!String_eq("pid:[4026531836]", target)) { // magic constant PROC_PID_INIT_INO from include/linux/proc_ns.h#L46
         Running_containerized = true;
         return true; // early return
      }
   }

   FILE* fd = fopen(PROCDIR "/1/mounts", "r");
   if (fd) {
      char lineBuffer[256];
      while (fgets(lineBuffer, sizeof(lineBuffer), fd)) {
         // detect lxc or overlayfs and guess that this means we are running containerized
         if (String_startsWith(lineBuffer, "lxcfs /proc") || String_startsWith(lineBuffer, "overlay ")) {
            Running_containerized = true;
            break;
         }
      }
      fclose(fd);
   } // if (fd)

   return true;
}

void Platform_done(void) {
#ifdef HAVE_SENSORS_SENSORS_H
   LibSensors_cleanup();
#endif
}
