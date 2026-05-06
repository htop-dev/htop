/*
htop - dragonflybsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "dragonflybsd/Platform.h"

#include <devstat.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <dev/acpica/acpiio.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vm/vm_param.h>

#include "CPUMeter.h"
#include "DateTimeMeter.h"
#include "FileDescriptorMeter.h"
#include "HostnameMeter.h"
#include "LoadAverageMeter.h"
#include "Macros.h"
#include "MemoryMeter.h"
#include "MemorySwapMeter.h"
#include "ProcessTable.h"
#include "SwapMeter.h"
#include "SysArchMeter.h"
#include "TasksMeter.h"
#include "UptimeMeter.h"
#include "XUtils.h"
#include "dragonflybsd/DragonFlyBSDMachine.h"
#include "dragonflybsd/DragonFlyBSDProcess.h"
#include "dragonflybsd/DragonFlyBSDProcessTable.h"
#include "generic/fdstat_sysctl.h"


const ScreenDefaults Platform_defaultScreens[] = {
   {
      .name = "Main",
      .columns = "PID USER PRIORITY NICE M_VIRT M_RESIDENT STATE PERCENT_CPU PERCENT_MEM TIME Command",
      .sortKey = "PERCENT_CPU",
   },
};

const unsigned int Platform_numberOfDefaultScreens = ARRAYSIZE(Platform_defaultScreens);

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
   { .name = " 1 SIGHUP",    .number =  1 },
   { .name = " 2 SIGINT",    .number =  2 },
   { .name = " 3 SIGQUIT",   .number =  3 },
   { .name = " 4 SIGILL",    .number =  4 },
   { .name = " 5 SIGTRAP",   .number =  5 },
   { .name = " 6 SIGABRT",   .number =  6 },
   { .name = " 7 SIGEMT",    .number =  7 },
   { .name = " 8 SIGFPE",    .number =  8 },
   { .name = " 9 SIGKILL",   .number =  9 },
   { .name = "10 SIGBUS",    .number = 10 },
   { .name = "11 SIGSEGV",   .number = 11 },
   { .name = "12 SIGSYS",    .number = 12 },
   { .name = "13 SIGPIPE",   .number = 13 },
   { .name = "14 SIGALRM",   .number = 14 },
   { .name = "15 SIGTERM",   .number = 15 },
   { .name = "16 SIGURG",    .number = 16 },
   { .name = "17 SIGSTOP",   .number = 17 },
   { .name = "18 SIGTSTP",   .number = 18 },
   { .name = "19 SIGCONT",   .number = 19 },
   { .name = "20 SIGCHLD",   .number = 20 },
   { .name = "21 SIGTTIN",   .number = 21 },
   { .name = "22 SIGTTOU",   .number = 22 },
   { .name = "23 SIGIO",     .number = 23 },
   { .name = "24 SIGXCPU",   .number = 24 },
   { .name = "25 SIGXFSZ",   .number = 25 },
   { .name = "26 SIGVTALRM", .number = 26 },
   { .name = "27 SIGPROF",   .number = 27 },
   { .name = "28 SIGWINCH",  .number = 28 },
   { .name = "29 SIGINFO",   .number = 29 },
   { .name = "30 SIGUSR1",   .number = 30 },
   { .name = "31 SIGUSR2",   .number = 31 },
   { .name = "32 SIGTHR",    .number = 32 },
   { .name = "33 SIGLIBRT",  .number = 33 },
};

const unsigned int Platform_numberOfSignals = ARRAYSIZE(Platform_signals);

enum {
   MEMORY_CLASS_WIRED = 0,
   MEMORY_CLASS_BUFFERS,
   MEMORY_CLASS_ACTIVE,
   MEMORY_CLASS_CACHE,
   MEMORY_CLASS_INACTIVE,
}; // N.B. the chart will display categories in this order

const MemoryClass Platform_memoryClasses[] = {
   [MEMORY_CLASS_WIRED] = { .label = "wired", .countsAsUsed = true, .countsAsCache = false, .color = MEMORY_1 },
   [MEMORY_CLASS_BUFFERS] = { .label = "buffers", .countsAsUsed = true, .countsAsCache = false, .color = MEMORY_2 },
   [MEMORY_CLASS_ACTIVE] = { .label = "active", .countsAsUsed = true, .countsAsCache = false, .color = MEMORY_3 },
   [MEMORY_CLASS_CACHE] = { .label = "cache", .countsAsUsed = false, .countsAsCache = true, .color = MEMORY_4 },
   [MEMORY_CLASS_INACTIVE] = { .label = "inactive", .countsAsUsed = false, .countsAsCache = true, .color = MEMORY_5 },
};

const unsigned int Platform_numberOfMemoryClasses = ARRAYSIZE(Platform_memoryClasses);

const MeterClass* const Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &DateMeter_class,
   &DateTimeMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &MemorySwapMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &UptimeMeter_class,
   &SecondsUptimeMeter_class,
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
   &DiskIORateMeter_class,
   &DiskIOTimeMeter_class,
   &DiskIOMeter_class,
   &NetworkIOMeter_class,
   &FileDescriptorMeter_class,
   &BlankMeter_class,
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
   int mib[2] = { CTL_KERN, KERN_BOOTTIME };
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
   int mib[2] = { CTL_VM, VM_LOADAVG };
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
   int maxPid;
   size_t size = sizeof(maxPid);
   int err = sysctlbyname("kern.pid_max", &maxPid, &size, NULL, 0);
   if (err) {
      return 999999;
   }
   return maxPid;
}

double Platform_setCPUValues(Meter* this, unsigned int cpu) {
   const Machine* host = this->host;
   const DragonFlyBSDMachine* dhost = (const DragonFlyBSDMachine*) host;
   unsigned int cpus = host->activeCPUs;
   const CPUData* cpuData;

   if (cpus == 1) {
      // single CPU box has everything in fpl->cpus[0]
      cpuData = &(dhost->cpus[0]);
   } else {
      cpuData = &(dhost->cpus[cpu]);
   }

   double  percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (host->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPercent;
      v[CPU_METER_IRQ]     = cpuData->irqPercent;
      this->curItems = 4;
   } else {
      v[CPU_METER_KERNEL] = cpuData->systemAllPercent;
      this->curItems = 3;
   }

   percent = sumPositiveValues(v, this->curItems);
   percent = MINIMUM(percent, 100.0);

   v[CPU_METER_FREQUENCY] = NAN;
   v[CPU_METER_TEMPERATURE] = NAN;

   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   const Machine* host = this->host;
   const DragonFlyBSDMachine* fhost = (const DragonFlyBSDMachine*) host;
   const Settings* settings = host->settings;

   this->total = host->totalMem;
   if (settings->showCachedMemory) {
      this->values[MEMORY_CLASS_WIRED]    = fhost->wiredMem;
      this->values[MEMORY_CLASS_BUFFERS]  = fhost->buffersMem;
   } else { // if showCachedMemory is disabled, merge buffers into the wired pages
      this->values[MEMORY_CLASS_WIRED]    = fhost->wiredMem + fhost->buffersMem;
      this->values[MEMORY_CLASS_BUFFERS]  = 0;
   }
   this->values[MEMORY_CLASS_ACTIVE]   = fhost->activeMem;
   this->values[MEMORY_CLASS_CACHE]    = fhost->cacheMem;
   this->values[MEMORY_CLASS_INACTIVE] = fhost->inactiveMem;
}

void Platform_setSwapValues(Meter* this) {
   const Machine* host = this->host;
   this->total = host->totalSwap;
   this->values[SWAP_METER_USED] = host->usedSwap;
}

char* Platform_getProcessEnv(pid_t pid) {
   // TODO
   (void)pid;  // prevent unused warning
   return NULL;
}

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid) {
   (void)pid;
   return NULL;
}

void Platform_getFileDescriptors(double* used, double* max) {
   Generic_getFileDescriptors_sysctl(used, max);
}

bool Platform_getDiskIO(DiskIOData* data) {
   struct statinfo dev_stats = { 0 };
   struct device_selection* dev_sel = NULL;
   int n_selected, n_selections;
   long sel_gen;

   dev_stats.dinfo = xCalloc(1, sizeof(struct devinfo));

   int ret = getdevs(&dev_stats);
   if (ret < 0) {
      CRT_debug("getdevs() failed [%d]: %s", ret, strerror(errno));
      free(dev_stats.dinfo);
      return false;
   }

   ret = selectdevs(&dev_sel, &n_selected, &n_selections, &sel_gen,
         dev_stats.dinfo->generation, dev_stats.dinfo->devices, dev_stats.dinfo->numdevs,
         NULL, 0, NULL, 0, DS_SELECT_ONLY, dev_stats.dinfo->numdevs, 1);
   if (ret < 0) {
      CRT_debug("selectdevs() failed [%d]: %s", ret, strerror(errno));
      free(dev_stats.dinfo);
      return false;
   }

   uint64_t bytesReadSum = 0;
   uint64_t bytesWriteSum = 0;
   uint64_t busyMsTimeSum = 0;
   uint64_t numDisks = 0;

   for (int i = 0; i < dev_stats.dinfo->numdevs; i++) {
      const struct devstat* device = &dev_stats.dinfo->devices[dev_sel[i].position];

      switch (device->device_type & DEVSTAT_TYPE_MASK) {
      case DEVSTAT_TYPE_DIRECT:
      case DEVSTAT_TYPE_SEQUENTIAL:
      case DEVSTAT_TYPE_WORM:
      case DEVSTAT_TYPE_CDROM:
      case DEVSTAT_TYPE_OPTICAL:
      case DEVSTAT_TYPE_CHANGER:
      case DEVSTAT_TYPE_STORARRAY:
      case DEVSTAT_TYPE_FLOPPY:
         break;
      default:
         continue;
      }

      bytesReadSum  += device->bytes_read;
      bytesWriteSum += device->bytes_written;
      busyMsTimeSum += (device->busy_time.tv_sec * 1000 + device->busy_time.tv_usec / 1000);
      numDisks++;
   }

   data->totalBytesRead = bytesReadSum;
   data->totalBytesWritten = bytesWriteSum;
   data->totalMsTimeSpend = busyMsTimeSum;
   data->numDisks = numDisks;

   free(dev_stats.dinfo);
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

void Platform_getBattery(BatteryInfo* info) {
   *info = (BatteryInfo) {
      .ac = AC_ERROR,
      .percent = NAN,
      .powerCurr = NAN,
      .energyCurr = NAN,
      .energyFull = NAN,
   };

   int life;
   size_t life_len = sizeof(life);
   if (sysctlbyname("hw.acpi.battery.life", &life, &life_len, NULL, 0) != -1)
      info->percent = life;

   int acline;
   size_t acline_len = sizeof(acline);
   if (sysctlbyname("hw.acpi.acline", &acline, &acline_len, NULL, 0) != -1)
      info->ac = (acline == 0) ? AC_ABSENT : AC_PRESENT;

   /* DragonFly does not define ACPIIO_BATT_GET_UNITS in
    * <dev/acpica/acpiio.h> (only the BIF/BST ioctls are exposed), so
    * obtain the unit count via the same hw.acpi.battery.units sysctl
    * that FreeBSD uses. The /dev/acpi BIF/BST ioctls below remain the
    * read source for per-unit data. */
   int units = 0;
   size_t units_len = sizeof(units);
   if (sysctlbyname("hw.acpi.battery.units", &units, &units_len, NULL, 0) == -1 || units <= 0)
      return;

   int fd = open("/dev/acpi", O_RDONLY | O_CLOEXEC);
   if (fd == -1)
      return;

   /* Energy totals (require voltage on mAh systems) — used for energyCurr/Full
    * and as the preferred basis for info->percent. */
   int64_t totalEnergyRemain = 0;
   int64_t totalEnergyFull = 0;
   int unitsWithEnergy = 0;

   /* Parallel raw-charge totals — used solely as a fallback basis for
    * info->percent when voltage is unknown on mAh systems and the energy
    * totals are therefore incomplete. */
   int64_t totalChargeRemain = 0;
   int64_t totalChargeFull = 0;
   int unitsWithCharge = 0;

   /* Power total. A unit is considered "covered" once its BIF/BST data is
    * in hand; powerComplete additionally requires every unit's rate to be
    * known and convertible to watts. Only publish powerCurr when both
    * conditions hold for every unit. */
   int64_t totalPower = 0;
   int unitsCovered = 0;
   bool powerComplete = true;

   /* Energy/charge completeness flags. Cleared when a per-unit ioctl fails
    * so we don't override the kernel's hw.acpi.battery.life sysctl with a
    * partial aggregate that omits units we couldn't read. */
   bool energyComplete = true;
   bool chargeComplete = true;

   /* Count slots that are physically present. ACPI may report N units while
    * one bay is empty; an empty bay's BST succeeds with NOT_PRESENT and zero
    * fields, which would otherwise drag down aggregate percent/energy. */
   int unitsPresent = 0;

   for (int u = 0; u < units; u++) {
      union acpi_battery_ioctl_arg bifArg = { .unit = u };
      if (ioctl(fd, ACPIIO_BATT_GET_BIF, &bifArg) == -1) {
         /* Failed read: we don't know whether this slot is present or not,
          * so conservatively mark every aggregate incomplete. Otherwise a
          * partial sum would override the kernel's authoritative sysctl. */
         energyComplete = false;
         chargeComplete = false;
         powerComplete = false;
         continue;
      }

      union acpi_battery_ioctl_arg bstArg = { .unit = u };
      if (ioctl(fd, ACPIIO_BATT_GET_BST, &bstArg) == -1) {
         /* Same reasoning as the BIF failure above. */
         energyComplete = false;
         chargeComplete = false;
         powerComplete = false;
         continue;
      }

      const struct acpi_bst* bst = &bstArg.bst;

      /* Empty battery bay: BST can succeed with zeroed cap/rate fields and
       * state reported as the NOT_PRESENT sentinel. NOT_PRESENT is a
       * synthetic value (the OR of all _BST state bits), not a standalone
       * flag, so test it with == rather than masking. Skip absent slots —
       * the completeness checks below use unitsPresent (not units) so absent
       * slots are treated as nonexistent rather than units with missing data. */
      if (bst->state == ACPI_BATT_STAT_NOT_PRESENT)
         continue;

      unitsPresent++;
      unitsCovered++;

      const struct acpi_bif* bif = &bifArg.bif;

      bool haveBatteryEnergyCurr = false;
      bool haveBatteryEnergyFull = false;
      bool haveBatteryChargeCurr = false;
      bool haveBatteryChargeFull = false;
      bool haveBatteryPower = false;

      int64_t batteryEnergyCurr = 0;
      int64_t batteryEnergyFull = 0;
      int64_t batteryChargeCurr = 0;
      int64_t batteryChargeFull = 0;
      int64_t batteryPower = 0;

      if (bif->lfcap != ACPI_BATT_UNKNOWN && bst->cap != ACPI_BATT_UNKNOWN) {
         if (bif->units == ACPI_BIF_UNITS_MW) {
            batteryEnergyCurr = (int64_t) bst->cap * 1000;
            batteryEnergyFull = (int64_t) bif->lfcap * 1000;
            haveBatteryEnergyCurr = true;
            haveBatteryEnergyFull = true;
         } else {
            /* Always populate the charge fallback so we can still compute
             * percent when voltage is unknown. */
            batteryChargeCurr = (int64_t) bst->cap;
            batteryChargeFull = (int64_t) bif->lfcap;
            haveBatteryChargeCurr = true;
            haveBatteryChargeFull = true;

            /* Use design voltage to convert charge into energy: bst->volt
             * (instantaneous terminal voltage) drifts as the pack charges and
             * discharges, which would skew batteryEnergyFull over time. Fall
             * back to the present voltage only if the design value is
             * missing. */
            uint32_t referenceVoltage = (bif->dvol != ACPI_BATT_UNKNOWN && bif->dvol != 0)
                                         ? bif->dvol
                                         : bst->volt;
            if (referenceVoltage != ACPI_BATT_UNKNOWN && referenceVoltage != 0) {
               batteryEnergyCurr = (int64_t) bst->cap * referenceVoltage;
               batteryEnergyFull = (int64_t) bif->lfcap * referenceVoltage;
               haveBatteryEnergyCurr = true;
               haveBatteryEnergyFull = true;
            }
         }
      }

      /* Clamp per-battery curr to full before summing: a quirky firmware
       * can report bst->cap > bif->lfcap, which would otherwise let the
       * published info->energyCurr / info->energyFull values exceed each
       * other's bounds even though the percent gate caps at 100. */
      if (haveBatteryEnergyCurr && haveBatteryEnergyFull && batteryEnergyFull > 0) {
         int64_t clampedEnergy = batteryEnergyCurr > batteryEnergyFull ? batteryEnergyFull : batteryEnergyCurr;
         totalEnergyRemain += clampedEnergy;
         totalEnergyFull += batteryEnergyFull;
         unitsWithEnergy++;
      }

      if (haveBatteryChargeCurr && haveBatteryChargeFull && batteryChargeFull > 0) {
         int64_t clampedCharge = batteryChargeCurr > batteryChargeFull ? batteryChargeFull : batteryChargeCurr;
         totalChargeRemain += clampedCharge;
         totalChargeFull += batteryChargeFull;
         unitsWithCharge++;
      }

      if (bst->rate == ACPI_BATT_UNKNOWN) {
         /* A genuinely unknown rate means the totalPower sum can't represent
          * this unit. Treat as incomplete so we don't publish a misleading
          * 0 W aggregate; the 0.0 fallback is reserved for known-zero rates
          * (e.g., idle on AC). */
         powerComplete = false;
         continue;
      }

      if (bst->rate == 0) {
         /* Zero rate is a known 0 W reading regardless of units or voltage:
          * I * V = 0 when I is zero, so the mAh path's voltage lookup is
          * unnecessary. batteryPower is already 0 from its declaration. */
         haveBatteryPower = true;
      } else if (bif->units == ACPI_BIF_UNITS_MW) {
         batteryPower = (int64_t) bst->rate * 1000;
         haveBatteryPower = true;
      } else {
         /* Instantaneous power P = I * V wants the present terminal voltage;
          * fall back to design voltage only when it isn't exposed. */
         uint32_t rateVoltage = (bst->volt != ACPI_BATT_UNKNOWN && bst->volt != 0)
                                 ? bst->volt
                                 : bif->dvol;

         if (rateVoltage != ACPI_BATT_UNKNOWN && rateVoltage != 0) {
            batteryPower = (int64_t) bst->rate * rateVoltage;
            haveBatteryPower = true;
         }
      }

      if (!haveBatteryPower) {
         /* A non-zero rate that we couldn't convert (mAh with unknown voltage)
          * means the totalPower sum is missing real charge/discharge activity. */
         powerComplete = false;
         continue;
      }

      if ((bst->state & ACPI_BATT_STAT_DISCHARG) == 0 &&
         (bst->state & ACPI_BATT_STAT_CHARGING) != 0)
         batteryPower = -batteryPower;

      totalPower += batteryPower;
   }

   close(fd);

   /* Only override the sysctl-derived percent when every present battery
    * unit contributed to the totals — partial aggregates would misrepresent
    * the pack against the kernel's hw.acpi.battery.life summary. Compare
    * against unitsPresent so empty bays don't fail the check, and require
    * the matching completeness flag (energyComplete / chargeComplete) so a
    * BIF/BST ioctl failure on any unit forces us back to the kernel's
    * authoritative summary instead of publishing a partial aggregate.
    * Prefer the energy (Wh) ratio over the raw-charge (mAh) ratio: on
    * multi-pack systems with differing voltages the energy ratio is the
    * physically correct aggregate, while an mAh-weighted ratio is not. */
   if (energyComplete && unitsPresent > 0 && unitsWithEnergy == unitsPresent && totalEnergyFull > 0) {
      info->percent = ((double) totalEnergyRemain * 100.0) / (double) totalEnergyFull;
      if (totalEnergyRemain >= totalEnergyFull)
         info->percent = 100;
   } else if (chargeComplete && unitsPresent > 0 && unitsWithCharge == unitsPresent && totalChargeFull > 0) {
      info->percent = ((double) totalChargeRemain * 100.0) / (double) totalChargeFull;
      if (totalChargeRemain >= totalChargeFull)
         info->percent = 100;
   }

   /* Only publish energy totals when complete in watt-hours for every present unit. */
   if (energyComplete && unitsPresent > 0 && unitsWithEnergy == unitsPresent && totalEnergyFull > 0) {
      info->energyCurr = (double) totalEnergyRemain / 1000000.0;
      info->energyFull = (double) totalEnergyFull / 1000000.0;
   }

   /* Only publish power when every present unit was covered and every rate
    * was known and convertible to W. An unknown rate on any unit leaves the
    * sysctl-derived powerCurr (NaN) in place rather than reporting a
    * misleading 0 W aggregate. */
   if (powerComplete && unitsPresent > 0 && unitsCovered == unitsPresent) {
      info->powerCurr = (double) totalPower / 1000000.0;
   }
}
