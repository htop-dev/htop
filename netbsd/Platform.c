/*
htop - netbsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
(C) 2021 Santhosh Raju
(C) 2021 Nia Alarie
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "netbsd/Platform.h"

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <paths.h>
#include <unistd.h>
#include <kvm.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <net/if.h>
#include <prop/proplib.h>
#include <sys/envsys.h>
#include <sys/iostat.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "CPUMeter.h"
#include "ClockMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "FileDescriptorMeter.h"
#include "HostnameMeter.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "MemorySwapMeter.h"
#include "Meter.h"
#include "Settings.h"
#include "SignalsPanel.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "generic/fdstat_sysctl.h"
#include "netbsd/NetBSDMachine.h"
#include "netbsd/NetBSDProcess.h"

/*
 * The older proplib APIs will be deprecated in NetBSD 10, but we still
 * want to support the 9.x stable branch.
 *
 * Create aliases for the newer functions that are missing from 9.x.
 */
#if !__NetBSD_Prereq__(9,99,65)
#define prop_string_equals_string prop_string_equals_cstring
#define prop_number_signed_value prop_number_integer_value
#endif

const ScreenDefaults Platform_defaultScreens[] = {
   {
      .name = "Main",
      .columns = "PID USER PRIORITY NICE M_VIRT M_RESIDENT STATE PERCENT_CPU PERCENT_MEM TIME Command",
      .sortKey = "PERCENT_CPU",
   },
};

const unsigned int Platform_numberOfDefaultScreens = ARRAYSIZE(Platform_defaultScreens);

/*
 * See /usr/include/sys/signal.h
 */
const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",      .number =  0 },
   { .name = " 1 SIGHUP",      .number =  1 },
   { .name = " 2 SIGINT",      .number =  2 },
   { .name = " 3 SIGQUIT",     .number =  3 },
   { .name = " 4 SIGILL",      .number =  4 },
   { .name = " 5 SIGTRAP",     .number =  5 },
   { .name = " 6 SIGABRT",     .number =  6 },
   { .name = " 6 SIGIOT",      .number =  6 },
   { .name = " 7 SIGEMT",      .number =  7 },
   { .name = " 8 SIGFPE",      .number =  8 },
   { .name = " 9 SIGKILL",     .number =  9 },
   { .name = "10 SIGBUS",      .number = 10 },
   { .name = "11 SIGSEGV",     .number = 11 },
   { .name = "12 SIGSYS",      .number = 12 },
   { .name = "13 SIGPIPE",     .number = 13 },
   { .name = "14 SIGALRM",     .number = 14 },
   { .name = "15 SIGTERM",     .number = 15 },
   { .name = "16 SIGURG",      .number = 16 },
   { .name = "17 SIGSTOP",     .number = 17 },
   { .name = "18 SIGTSTP",     .number = 18 },
   { .name = "19 SIGCONT",     .number = 19 },
   { .name = "20 SIGCHLD",     .number = 20 },
   { .name = "21 SIGTTIN",     .number = 21 },
   { .name = "22 SIGTTOU",     .number = 22 },
   { .name = "23 SIGIO",       .number = 23 },
   { .name = "24 SIGXCPU",     .number = 24 },
   { .name = "25 SIGXFSZ",     .number = 25 },
   { .name = "26 SIGVTALRM",   .number = 26 },
   { .name = "27 SIGPROF",     .number = 27 },
   { .name = "28 SIGWINCH",    .number = 28 },
   { .name = "29 SIGINFO",     .number = 29 },
   { .name = "30 SIGUSR1",     .number = 30 },
   { .name = "31 SIGUSR2",     .number = 31 },
   { .name = "32 SIGPWR",      .number = 32 },
   { .name = "33 SIGRTMIN",    .number = 33 },
   { .name = "34 SIGRTMIN+1",  .number = 34 },
   { .name = "35 SIGRTMIN+2",  .number = 35 },
   { .name = "36 SIGRTMIN+3",  .number = 36 },
   { .name = "37 SIGRTMIN+4",  .number = 37 },
   { .name = "38 SIGRTMIN+5",  .number = 38 },
   { .name = "39 SIGRTMIN+6",  .number = 39 },
   { .name = "40 SIGRTMIN+7",  .number = 40 },
   { .name = "41 SIGRTMIN+8",  .number = 41 },
   { .name = "42 SIGRTMIN+9",  .number = 42 },
   { .name = "43 SIGRTMIN+10", .number = 43 },
   { .name = "44 SIGRTMIN+11", .number = 44 },
   { .name = "45 SIGRTMIN+12", .number = 45 },
   { .name = "46 SIGRTMIN+13", .number = 46 },
   { .name = "47 SIGRTMIN+14", .number = 47 },
   { .name = "48 SIGRTMIN+15", .number = 48 },
   { .name = "49 SIGRTMIN+16", .number = 49 },
   { .name = "50 SIGRTMIN+17", .number = 50 },
   { .name = "51 SIGRTMIN+18", .number = 51 },
   { .name = "52 SIGRTMIN+19", .number = 52 },
   { .name = "53 SIGRTMIN+20", .number = 53 },
   { .name = "54 SIGRTMIN+21", .number = 54 },
   { .name = "55 SIGRTMIN+22", .number = 55 },
   { .name = "56 SIGRTMIN+23", .number = 56 },
   { .name = "57 SIGRTMIN+24", .number = 57 },
   { .name = "58 SIGRTMIN+25", .number = 58 },
   { .name = "59 SIGRTMIN+26", .number = 59 },
   { .name = "60 SIGRTMIN+27", .number = 60 },
   { .name = "61 SIGRTMIN+28", .number = 61 },
   { .name = "62 SIGRTMIN+29", .number = 62 },
   { .name = "63 SIGRTMAX",    .number = 63 },
};

const unsigned int Platform_numberOfSignals = ARRAYSIZE(Platform_signals);

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
   &TasksMeter_class,
   &UptimeMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &SysArchMeter_class,
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
   &DiskIOMeter_class,
   &NetworkIOMeter_class,
   &FileDescriptorMeter_class,
   NULL
};

bool Platform_init(void) {
   /* no platform-specific setup needed */
   return true;
}

void Platform_done(void) {
   /* no platform-specific cleanup needed */
}

void Platform_setBindings(Htop_Action* keys) {
   /* no platform-specific key bindings */
   (void) keys;
}

int Platform_getUptime(void) {
   struct timeval bootTime, currTime;
   const int mib[2] = { CTL_KERN, KERN_BOOTTIME };
   size_t size = sizeof(bootTime);

   int err = sysctl(mib, 2, &bootTime, &size, NULL, 0);
   if (err) {
      return -1;
   }
   gettimeofday(&currTime, NULL);

   return (int) difftime(currTime.tv_sec, bootTime.tv_sec);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   struct loadavg loadAverage;
   const int mib[2] = { CTL_VM, VM_LOADAVG };
   size_t size = sizeof(loadAverage);

   int err = sysctl(mib, 2, &loadAverage, &size, NULL, 0);
   if (err) {
      *one = 0;
      *five = 0;
      *fifteen = 0;
      return;
   }
   *one     = (double) loadAverage.ldavg[0] / loadAverage.fscale;
   *five    = (double) loadAverage.ldavg[1] / loadAverage.fscale;
   *fifteen = (double) loadAverage.ldavg[2] / loadAverage.fscale;
}

pid_t Platform_getMaxPid(void) {
   // https://nxr.netbsd.org/xref/src/sys/sys/ansi.h#__pid_t
   // pid is assigned as a 32bit Integer.
   return INT32_MAX;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   const Machine* host = this->host;
   const NetBSDMachine* nhost = (const NetBSDMachine*) host;
   const CPUData* cpuData = &nhost->cpuData[cpu];
   double total = cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod;
   double totalPercent;
   double* v = this->values;

   v[CPU_METER_NICE] = cpuData->nicePeriod / total * 100.0;
   v[CPU_METER_NORMAL] = cpuData->userPeriod / total * 100.0;
   if (host->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->sysPeriod / total * 100.0;
      v[CPU_METER_IRQ]     = cpuData->intrPeriod / total * 100.0;
      v[CPU_METER_SOFTIRQ] = 0.0;
      v[CPU_METER_STEAL]   = 0.0;
      v[CPU_METER_GUEST]   = 0.0;
      v[CPU_METER_IOWAIT]  = 0.0;
      v[CPU_METER_FREQUENCY] = NAN;
      this->curItems = 8;
   } else {
      v[CPU_METER_KERNEL] = cpuData->sysAllPeriod / total * 100.0;
      v[CPU_METER_IRQ] = 0.0; // No steal nor guest on NetBSD
      this->curItems = 4;
   }
   totalPercent = v[CPU_METER_NICE] + v[CPU_METER_NORMAL] + v[CPU_METER_KERNEL] + v[CPU_METER_IRQ];
   totalPercent = CLAMP(totalPercent, 0.0, 100.0);

   v[CPU_METER_FREQUENCY] = cpuData->frequency;
   v[CPU_METER_TEMPERATURE] = NAN;

   return totalPercent;
}

void Platform_setMemoryValues(Meter* this) {
   const Machine* host = this->host;
   this->total = host->totalMem;
   this->values[MEMORY_METER_USED] = host->usedMem;
   // this->values[MEMORY_METER_SHARED] = "shared memory, like tmpfs and shm"
   // this->values[MEMORY_METER_COMPRESSED] = "compressed memory, like zswap on linux"
   this->values[MEMORY_METER_BUFFERS] = host->buffersMem;
   this->values[MEMORY_METER_CACHE] = host->cachedMem;
   // this->values[MEMORY_METER_AVAILABLE] = "available memory"
}

void Platform_setSwapValues(Meter* this) {
   const Machine* host = this->host;
   this->total = host->totalSwap;
   this->values[SWAP_METER_USED] = host->usedSwap;
   // this->values[SWAP_METER_CACHE] = "pages that are both in swap and RAM, like SwapCached on linux"
   // this->values[SWAP_METER_FRONTSWAP] = "pages that are accounted to swap but stored elsewhere, like frontswap on linux"
}

char* Platform_getProcessEnv(pid_t pid) {
   char errbuf[_POSIX2_LINE_MAX];
   char* env;
   char** ptr;
   int count;
   kvm_t* kt;
   const struct kinfo_proc2* kproc;
   size_t capacity = 4096, size = 0;

   if ((kt = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf)) == NULL) {
      return NULL;
   }

   if ((kproc = kvm_getproc2(kt, KERN_PROC_PID, pid, sizeof(struct kinfo_proc2), &count)) == NULL) {
      (void) kvm_close(kt);
      return NULL;
   }

   if ((ptr = kvm_getenvv2(kt, kproc, 0)) == NULL) {
      (void) kvm_close(kt);
      return NULL;
   }

   env = xMalloc(capacity);
   for (char** p = ptr; *p; p++) {
      size_t len = strlen(*p) + 1;

      while (size + len > capacity) {
         if (capacity > (SIZE_MAX / 2)) {
            free(env);
            env = NULL;
            goto end;
         }

         capacity *= 2;
         env = xRealloc(env, capacity);
      }

      String_safeStrncpy(env + size, *p, len);
      size += len;
   }

   if (size < 2 || env[size - 1] || env[size - 2]) {
      if (size + 2 < capacity)
         env = xRealloc(env, capacity + 2);
      env[size] = 0;
      env[size + 1] = 0;
   }

end:
   (void) kvm_close(kt);
   return env;
}

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid) {
   (void)pid;
   return NULL;
}

void Platform_getFileDescriptors(double* used, double* max) {
   Generic_getFileDescriptors_sysctl(used, max);
}

bool Platform_getDiskIO(DiskIOData* data) {
   const int mib[] = { CTL_HW, HW_IOSTATS, sizeof(struct io_sysctl) };
   struct io_sysctl* iostats = NULL;
   size_t size = 0;

   for (int retry = 3; retry > 0; retry--) {
      /* get the size of the IO statistic array */
      if (sysctl(mib, __arraycount(mib), iostats, &size, NULL, 0) < 0)
         CRT_fatalError("Unable to get size of io_sysctl");

      if (size == 0) {
         free(iostats);
         return false;
      }

      iostats = xRealloc(iostats, size);

      errno = 0;

      if (sysctl(mib, __arraycount(mib), iostats, &size, NULL, 0) == 0)
         break;

      if (errno != ENOMEM)
         CRT_fatalError("Unable to get disk IO statistics");
   }

   if (errno == ENOMEM)
      CRT_fatalError("Unable to get disk IO statistics");

   uint64_t bytesReadSum = 0;
   uint64_t bytesWriteSum = 0;
   uint64_t busyTimeSum = 0;

   for (size_t i = 0, count = size / sizeof(struct io_sysctl); i < count; i++) {
      /* ignore NFS activity */
      if (iostats[i].type != IOSTAT_DISK)
         continue;

      bytesReadSum += iostats[i].rbytes;
      bytesWriteSum += iostats[i].wbytes;
      busyTimeSum += iostats[i].busysum_usec;
   }

   data->totalBytesRead = bytesReadSum;
   data->totalBytesWritten = bytesWriteSum;
   data->totalMsTimeSpend = busyTimeSum / 1000;

   free(iostats);
   return true;
}

bool Platform_getNetworkIO(NetworkIOData* data) {
   struct ifaddrs* ifaddrs = NULL;

   if (getifaddrs(&ifaddrs) != 0)
      return false;

   for (const struct ifaddrs* ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
      if (!ifa->ifa_addr)
         continue;
      if (ifa->ifa_addr->sa_family != AF_LINK)
         continue;
      if (ifa->ifa_flags & IFF_LOOPBACK)
         continue;

      const struct if_data* ifd = (const struct if_data*)ifa->ifa_data;

      data->bytesReceived += ifd->ifi_ibytes;
      data->packetsReceived += ifd->ifi_ipackets;
      data->bytesTransmitted += ifd->ifi_obytes;
      data->packetsTransmitted += ifd->ifi_opackets;
   }

   freeifaddrs(ifaddrs);
   return true;
}

void Platform_getBattery(double* percent, ACPresence* isOnAC) {
   prop_dictionary_t dict, fields, props;
   prop_object_t device, class;

   intmax_t totalCharge = 0;
   intmax_t totalCapacity = 0;

   *percent = NAN;
   *isOnAC = AC_ERROR;

   int fd = open(_PATH_SYSMON, O_RDONLY);
   if (fd == -1)
      goto error;

   if (prop_dictionary_recv_ioctl(fd, ENVSYS_GETDICTIONARY, &dict) != 0)
      goto error;

   prop_object_iterator_t devIter = prop_dictionary_iterator(dict);
   if (devIter == NULL)
      goto error;

   while ((device = prop_object_iterator_next(devIter)) != NULL) {
      prop_object_t fieldsArray = prop_dictionary_get_keysym(dict, device);
      if (fieldsArray == NULL)
         goto error;

      prop_object_iterator_t fieldsIter = prop_array_iterator(fieldsArray);
      if (fieldsIter == NULL)
         goto error;

      bool isACAdapter = false;
      bool isBattery = false;

      /* only assume battery is not present if explicitly stated */
      intmax_t isPresent = 1;
      intmax_t isConnected = 0;
      intmax_t curCharge = 0;
      intmax_t maxCharge = 0;

      while ((fields = prop_object_iterator_next(fieldsIter)) != NULL) {
         props = prop_dictionary_get(fields, "device-properties");
         if (props != NULL) {
            class = prop_dictionary_get(props, "device-class");

            if (prop_string_equals_string(class, "ac-adapter")) {
               isACAdapter = true;
            } else if (prop_string_equals_string(class, "battery")) {
               isBattery = true;
            }
            continue;
         }

         prop_object_t curValue = prop_dictionary_get(fields, "cur-value");
         prop_object_t maxValue = prop_dictionary_get(fields, "max-value");
         prop_object_t descField = prop_dictionary_get(fields, "description");

         if (descField == NULL || curValue == NULL)
            continue;

         if (prop_string_equals_string(descField, "connected")) {
            isConnected = prop_number_signed_value(curValue);
         } else if (prop_string_equals_string(descField, "present")) {
            isPresent = prop_number_signed_value(curValue);
         } else if (prop_string_equals_string(descField, "charge")) {
            if (maxValue == NULL)
               continue;
            curCharge = prop_number_signed_value(curValue);
            maxCharge = prop_number_signed_value(maxValue);
         }
      }

      if (isBattery && isPresent) {
         totalCharge += curCharge;
         totalCapacity += maxCharge;
      }

      if (isACAdapter && *isOnAC != AC_PRESENT) {
         *isOnAC = isConnected ? AC_PRESENT : AC_ABSENT;
      }
   }

   *percent = ((double)totalCharge / (double)totalCapacity) * 100.0;

error:
   if (fd != -1)
      close(fd);
}
