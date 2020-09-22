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
#include <math.h>
#include <stdio.h>

#include "BatteryMeter.h"
#include "ClockMeter.h"
#include "CPUMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "DiskIOMeter.h"
#include "HostnameMeter.h"
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
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "ZramMeter.h"

#include "zfs/ZfsArcMeter.h"
#include "zfs/ZfsArcStats.h"
#include "zfs/ZfsCompressedArcMeter.h"


ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, (int)M_SHARE, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

//static ProcessField defaultIoFields[] = { PID, IO_PRIORITY, USER, IO_READ_RATE, IO_WRITE_RATE, IO_RATE, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

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

static Htop_Reaction Platform_actionSetIOPriority(State* st) {
   Panel* panel = st->panel;

   LinuxProcess* p = (LinuxProcess*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   IOPriority ioprio1 = p->ioPriority;
   Panel* ioprioPanel = IOPriorityPanel_new(ioprio1);
   void* set = Action_pickFromVector(st, ioprioPanel, 21, true);
   if (set) {
      IOPriority ioprio2 = IOPriorityPanel_getIOPriority(ioprioPanel);
      bool ok = MainPanel_foreachProcess((MainPanel*)panel, LinuxProcess_setIOPriority, (Arg){ .i = ioprio2 }, NULL);
      if (!ok)
         beep();
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
   NULL
};

int Platform_getUptime() {
   double uptime = 0;
   FILE* fd = fopen(PROCDIR "/uptime", "r");
   if (fd) {
      int n = fscanf(fd, "%64lf", &uptime);
      fclose(fd);
      if (n <= 0) return 0;
   }
   return floor(uptime);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   int activeProcs, totalProcs, lastProc;
   *one = 0; *five = 0; *fifteen = 0;
   FILE *fd = fopen(PROCDIR "/loadavg", "r");
   if (fd) {
      int total = fscanf(fd, "%32lf %32lf %32lf %32d/%32d %32d", one, five, fifteen,
         &activeProcs, &totalProcs, &lastProc);
      (void) total;
      assert(total == 6);
      fclose(fd);
   }
}

int Platform_getMaxPid() {
   FILE* file = fopen(PROCDIR "/sys/kernel/pid_max", "r");
   if (!file) return -1;
   int maxPid = 4194303;
   int match = fscanf(file, "%32d", &maxPid);
   (void) match;
   fclose(file);
   return maxPid;
}

double Platform_setCPUValues(Meter* this, int cpu) {
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
         percent = v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6];
      } else {
         percent = v[0]+v[1]+v[2]+v[3]+v[4];
      }
   } else {
      v[2] = cpuData->systemAllPeriod / total * 100.0;
      v[3] = (cpuData->stealPeriod + cpuData->guestPeriod) / total * 100.0;
      this->curItems = 4;
      percent = v[0]+v[1]+v[2]+v[3];
   }
   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;

   v[CPU_METER_FREQUENCY] = cpuData->frequency;

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   const ProcessList* pl = this->pl;
   const LinuxProcessList* lpl = (const LinuxProcessList*) pl;

   long int usedMem = pl->usedMem;
   long int buffersMem = pl->buffersMem;
   long int cachedMem = pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   this->total = pl->totalMem;
   this->values[0] = usedMem;
   this->values[1] = buffersMem;
   this->values[2] = cachedMem;

   if (lpl->zfs.enabled != 0) {
      this->values[0] -= lpl->zfs.size;
      this->values[2] += lpl->zfs.size;
   }
}

void Platform_setSwapValues(Meter* this) {
   const ProcessList* pl = this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
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
   if(!fd)
      return NULL;

   char *env = NULL;

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
   env[size+1] = '\0';

   return env;
}

void Platform_getPressureStall(const char *file, bool some, double* ten, double* sixty, double* threehundred) {
   *ten = *sixty = *threehundred = 0;
   char procname[128+1];
   xSnprintf(procname, 128, PROCDIR "/pressure/%s", file);
   FILE *fd = fopen(procname, "r");
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
   FILE *fd = fopen(PROCDIR "/diskstats", "r");
   if (!fd)
      return false;

   unsigned long int read_sum = 0, write_sum = 0, timeSpend_sum = 0;
   char lineBuffer[256];
   while (fgets(lineBuffer, sizeof(lineBuffer), fd)) {
      char diskname[32];
      unsigned long int read_tmp, write_tmp, timeSpend_tmp;
      if (sscanf(lineBuffer, "%*d %*d %31s %*u %*u %lu %*u %*u %*u %lu %*u %*u %lu", diskname, &read_tmp, &write_tmp, &timeSpend_tmp) == 4) {
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

bool Platform_getNetworkIO(unsigned long int *bytesReceived,
                           unsigned long int *packetsReceived,
                           unsigned long int *bytesTransmitted,
                           unsigned long int *packetsTransmitted) {
   FILE *fd = fopen(PROCDIR "/net/dev", "r");
   if (!fd)
      return false;

   unsigned long int bytesReceivedSum = 0, packetsReceivedSum = 0, bytesTransmittedSum = 0, packetsTransmittedSum = 0;
   char lineBuffer[512];
   while (fgets(lineBuffer, sizeof(lineBuffer), fd)) {
      char interfaceName[32];
      unsigned long int bytesReceivedParsed, packetsReceivedParsed, bytesTransmittedParsed, packetsTransmittedParsed;
      if (sscanf(lineBuffer, "%31s %lu %lu %*u %*u %*u %*u %*u %*u %lu %lu",
                             interfaceName,
                             &bytesReceivedParsed,
                             &packetsReceivedParsed,
                             &bytesTransmittedParsed,
                             &packetsTransmittedParsed) != 5)
         continue;

      if (String_eq(interfaceName, "lo:"))
         continue;

      bytesReceivedSum += bytesReceivedParsed;
      packetsReceivedSum += packetsReceivedParsed;
      bytesTransmittedSum += bytesTransmittedParsed;
      packetsTransmittedSum += packetsTransmittedParsed;
   }

   fclose(fd);

   *bytesReceived = bytesReceivedSum;
   *packetsReceived = packetsReceivedSum;
   *bytesTransmitted = bytesTransmittedSum;
   *packetsTransmitted = packetsTransmittedSum;
   return true;
}
