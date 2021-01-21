/*
htop - linux/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include "Platform.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
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
#include "IOPriority.h"
#include "IOPriorityPanel.h"
#include "LinuxProcess.h"
#include "LinuxProcessList.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MainPanel.h"
#include "Meter.h"
#include "MemoryMeter.h"
#include "NetworkIOMeter.h"
#include "Object.h"
#include "Panel.h"
#include "PressureStallMeter.h"
#include "ProcessList.h"
#include "ProvideCurses.h"
#include "SELinuxMeter.h"
#include "Settings.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "SystemdMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "ZramMeter.h"
#include "ZramStats.h"

#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsArcStats.h"
#include "zfs/ZfsCompressedArcMeter.h"

#ifdef HAVE_LIBCAP
#include <sys/capability.h>
#endif

#ifdef HAVE_SENSORS_SENSORS_H
#include "LibSensors.h"
#endif


#ifdef HAVE_LIBCAP
enum CapMode {
   CAP_MODE_OFF,
   CAP_MODE_BASIC,
   CAP_MODE_STRICT
};
#endif

const ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_VIRT, M_RESIDENT, M_SHARE, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

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
   const void* set = Action_pickFromVector(st, ioprioPanel, 21, true);
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

void Platform_setBindings(Htop_Action* keys) {
   keys['i'] = Platform_actionSetIOPriority;
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
   const CPUData* cpuData = &(pl->cpus[cpu]);
   double total = (double) ( cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod);
   double percent;
   double* v = this->values;
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

   this->total     = pl->totalMem > lpl->totalHugePageMem ? pl->totalMem - lpl->totalHugePageMem : pl->totalMem;
   this->values[0] = pl->usedMem > lpl->totalHugePageMem ? pl->usedMem - lpl->totalHugePageMem : pl->usedMem;
   this->values[1] = pl->buffersMem;
   this->values[2] = pl->sharedMem;
   this->values[3] = pl->cachedMem;
   this->values[4] = pl->availableMem;

   if (lpl->zfs.enabled != 0) {
      this->values[0] -= lpl->zfs.size;
      this->values[3] += lpl->zfs.size;
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
   const struct dirent *de;
   DIR *dirp;
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

   unsigned long long int read_sum = 0, write_sum = 0, timeSpend_sum = 0;
   char lineBuffer[256];
   while (fgets(lineBuffer, sizeof(lineBuffer), fd)) {
      char diskname[32];
      unsigned long long int read_tmp, write_tmp, timeSpend_tmp;
      if (sscanf(lineBuffer, "%*d %*d %31s %*u %*u %llu %*u %*u %*u %llu %*u %*u %llu", diskname, &read_tmp, &write_tmp, &timeSpend_tmp) == 4) {
         if (String_startsWith(diskname, "dm-"))
            continue;

         /* only count root disks, e.g. do not count IO from sda and sda1 twice */
         if ((diskname[0] == 's' || diskname[0] == 'h')
             && diskname[1] == 'd'
             && isalpha((unsigned char)diskname[2])
             && isdigit((unsigned char)diskname[3]))
            continue;

         /* only count root disks, e.g. do not count IO from mmcblk0 and mmcblk0p1 twice */
         if (diskname[0] == 'm'
             && diskname[1] == 'm'
             && diskname[2] == 'c'
             && diskname[3] == 'b'
             && diskname[4] == 'l'
             && diskname[5] == 'k'
             && isdigit((unsigned char)diskname[6])
             && diskname[7] == 'p')
            continue;

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

#define MAX_BATTERIES 64
#define PROC_BATTERY_DIR PROCDIR "/acpi/battery"
#define PROC_POWERSUPPLY_DIR PROCDIR "/acpi/ac_adapter"
#define SYS_POWERSUPPLY_DIR "/sys/class/power_supply"

// ----------------------------------------
// READ FROM /proc
// ----------------------------------------

static unsigned long int parseBatInfo(const char* fileName, const unsigned short int lineNum, const unsigned short int wordNum) {
   const char batteryPath[] = PROC_BATTERY_DIR;
   DIR* batteryDir = opendir(batteryPath);
   if (!batteryDir)
      return 0;

   char* batteries[MAX_BATTERIES];
   unsigned int nBatteries = 0;
   memset(batteries, 0, MAX_BATTERIES * sizeof(char*));

   while (nBatteries < MAX_BATTERIES) {
      const struct dirent* dirEntry = readdir(batteryDir);
      if (!dirEntry)
         break;

      const char* entryName = dirEntry->d_name;
      if (!String_startsWith(entryName, "BAT"))
         continue;

      batteries[nBatteries] = xStrdup(entryName);
      nBatteries++;
   }
   closedir(batteryDir);

   unsigned long int total = 0;
   for (unsigned int i = 0; i < nBatteries; i++) {
      char infoPath[30];
      xSnprintf(infoPath, sizeof infoPath, "%s%s/%s", batteryPath, batteries[i], fileName);

      FILE* file = fopen(infoPath, "r");
      if (!file)
         break;

      char* line = NULL;
      for (unsigned short int j = 0; j < lineNum; j++) {
         free(line);
         line = String_readLine(file);
         if (!line)
            break;
      }

      fclose(file);

      if (!line)
         break;

      char* foundNumStr = String_getToken(line, wordNum);
      const unsigned long int foundNum = atoi(foundNumStr);
      free(foundNumStr);
      free(line);

      total += foundNum;
   }

   for (unsigned int i = 0; i < nBatteries; i++)
      free(batteries[i]);

   return total;
}

static ACPresence procAcpiCheck(void) {
   ACPresence isOn = AC_ERROR;
   const char* power_supplyPath = PROC_POWERSUPPLY_DIR;
   DIR* dir = opendir(power_supplyPath);
   if (!dir)
      return AC_ERROR;

   for (;;) {
      const struct dirent* dirEntry = readdir(dir);
      if (!dirEntry)
         break;

      const char* entryName = dirEntry->d_name;

      if (entryName[0] != 'A')
         continue;

      char statePath[256];
      xSnprintf(statePath, sizeof(statePath), "%s/%s/state", power_supplyPath, entryName);
      FILE* file = fopen(statePath, "r");
      if (!file) {
         isOn = AC_ERROR;
         continue;
      }
      char* line = String_readLine(file);

      fclose(file);

      if (!line)
         continue;

      char* isOnline = String_getToken(line, 2);
      free(line);

      if (String_eq(isOnline, "on-line"))
         isOn = AC_PRESENT;
      else
         isOn = AC_ABSENT;
      free(isOnline);
      if (isOn == AC_PRESENT)
         break;
   }

   closedir(dir);

   return isOn;
}

static double Platform_Battery_getProcBatInfo(void) {
   const unsigned long int totalFull = parseBatInfo("info", 3, 4);
   if (totalFull == 0)
      return NAN;

   const unsigned long int totalRemain = parseBatInfo("state", 5, 3);
   if (totalRemain == 0)
      return NAN;

   return totalRemain * 100.0 / (double) totalFull;
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

   unsigned long int totalFull = 0;
   unsigned long int totalRemain = 0;

   for (;;) {
      const struct dirent* dirEntry = readdir(dir);
      if (!dirEntry)
         break;

      const char* entryName = dirEntry->d_name;
      char filePath[256];

      xSnprintf(filePath, sizeof filePath, SYS_POWERSUPPLY_DIR "/%s/type", entryName);

      char type[8];
      ssize_t r = xReadfile(filePath, type, sizeof(type));
      if (r < 3)
         continue;

      if (type[0] == 'B' && type[1] == 'a' && type[2] == 't') {
         xSnprintf(filePath, sizeof filePath, SYS_POWERSUPPLY_DIR "/%s/uevent", entryName);

         char buffer[1024];
         r = xReadfile(filePath, buffer, sizeof(buffer));
         if (r < 0) {
            closedir(dir);
            return;
         }

         char* buf = buffer;
         const char* line;
         bool full = false;
         bool now = false;
         int fullSize = 0;
         double capacityLevel = NAN;

         #define match(str,prefix) \
            (String_startsWith(str,prefix) ? (str) + strlen(prefix) : NULL)

         while ((line = strsep(&buf, "\n")) != NULL) {
            const char* ps = match(line, "POWER_SUPPLY_");
            if (!ps)
               continue;
            const char* capacity = match(ps, "CAPACITY=");
            if (capacity)
               capacityLevel = atoi(capacity) / 100.0;
            const char* energy = match(ps, "ENERGY_");
            if (!energy)
               energy = match(ps, "CHARGE_");
            if (!energy)
               continue;
            const char* value = (!full) ? match(energy, "FULL=") : NULL;
            if (value) {
               fullSize = atoi(value);
               totalFull += fullSize;
               full = true;
               if (now)
                  break;
               continue;
            }
            value = (!now) ? match(energy, "NOW=") : NULL;
            if (value) {
               totalRemain += atoi(value);
               now = true;
               if (full)
                  break;
               continue;
            }
         }

         #undef match

         if (!now && full && !isnan(capacityLevel))
            totalRemain += (capacityLevel * fullSize);

      } else if (entryName[0] == 'A') {
         if (*isOnAC != AC_ERROR)
            continue;

         xSnprintf(filePath, sizeof filePath, SYS_POWERSUPPLY_DIR "/%s/online", entryName);

         char buffer[2];

         r = xReadfile(filePath, buffer, sizeof(buffer));
         if (r < 1) {
            closedir(dir);
            return;
         }

         if (buffer[0] == '0')
            *isOnAC = AC_ABSENT;
         else if (buffer[0] == '1')
            *isOnAC = AC_PRESENT;
      }
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

bool Platform_getLongOption(int opt, int argc, char** argv) {
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
            exit(1);
         }
         return true;
      }
#endif

      default:
         break;
   }
   return false;
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

void Platform_init(void) {
#ifdef HAVE_LIBCAP
   if (dropCapabilities(Platform_capabilitiesMode) < 0)
      exit(1);
#endif

   if (access(PROCDIR, R_OK) != 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      exit(1);
   }

#ifdef HAVE_SENSORS_SENSORS_H
   LibSensors_init(NULL);
#endif
}

void Platform_done(void) {
#ifdef HAVE_SENSORS_SENSORS_H
   LibSensors_cleanup();
#endif
}
