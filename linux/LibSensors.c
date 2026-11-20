/*
htop - linux/LibSensors.c
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/LibSensors.h"

#ifdef HAVE_SENSORS_SENSORS_H

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sensors/sensors.h>

#include "Macros.h"
#include "XUtils.h"
#include "linux/LinuxMachine.h"


#ifdef BUILD_STATIC

#define sym_sensors_init sensors_init
#define sym_sensors_cleanup sensors_cleanup
#define sym_sensors_get_detected_chips sensors_get_detected_chips
#define sym_sensors_get_features sensors_get_features
#define sym_sensors_get_subfeature sensors_get_subfeature
#define sym_sensors_get_value sensors_get_value
#define sym_sensors_get_label sensors_get_label

#else

static int (*sym_sensors_init)(FILE*);
static void (*sym_sensors_cleanup)(void);
static const sensors_chip_name* (*sym_sensors_get_detected_chips)(const sensors_chip_name*, int*);
static const sensors_feature* (*sym_sensors_get_features)(const sensors_chip_name*, int*);
static const sensors_subfeature* (*sym_sensors_get_subfeature)(const sensors_chip_name*, const sensors_feature*, sensors_subfeature_type);
static int (*sym_sensors_get_value)(const sensors_chip_name*, int, double*);
static char* (*sym_sensors_get_label)(const sensors_chip_name*, const sensors_feature *feature);

static void* dlopenHandle = NULL;

#endif /* BUILD_STATIC */

int LibSensors_init(void) {
#ifdef BUILD_STATIC

   return sym_sensors_init(NULL);

#else

   if (!dlopenHandle) {
      /* Find the unversioned libsensors.so (symlink) and prefer that, but Debian has .so.5 and Fedora .so.4 without
         matching symlinks (unless people install the -dev packages) */
      dlopenHandle = dlopen("libsensors.so", RTLD_LAZY);
      if (!dlopenHandle)
         dlopenHandle = dlopen("libsensors.so.5", RTLD_LAZY);
      if (!dlopenHandle)
         dlopenHandle = dlopen("libsensors.so.4", RTLD_LAZY);
      if (!dlopenHandle)
         goto dlfailure;

      /* Clear any errors */
      dlerror();

      #define resolve(symbolname) do {                                      \
         *(void **)(&sym_##symbolname) = dlsym(dlopenHandle, #symbolname);  \
         if (!sym_##symbolname || dlerror() != NULL)                        \
            goto dlfailure;                                                 \
      } while(0)

      resolve(sensors_init);
      resolve(sensors_cleanup);
      resolve(sensors_get_detected_chips);
      resolve(sensors_get_features);
      resolve(sensors_get_subfeature);
      resolve(sensors_get_value);
      resolve(sensors_get_label);

      #undef resolve
   }

   return sym_sensors_init(NULL);


dlfailure:
   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
   return -1;

#endif /* BUILD_STATIC */
}

void LibSensors_cleanup(void) {
#ifdef BUILD_STATIC

   sym_sensors_cleanup();

#else

   if (dlopenHandle) {
      sym_sensors_cleanup();

      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }

#endif /* BUILD_STATIC */
}

int LibSensors_reload(void) {
#ifndef BUILD_STATIC
   if (!dlopenHandle) {
      errno = ENOTSUP;
      return -1;
   }
#endif /* !BUILD_STATIC */

   sym_sensors_cleanup();
   return sym_sensors_init(NULL);
}

static int tempDriverPriority(const sensors_chip_name* chip) {
   static const struct TempDriverDefs {
      const char* prefix;
      int priority;
   } tempDrivers[] =  {
      { "coretemp",           0 },
      { "via_cputemp",        0 },
      { "cpu_thermal",        0 },
      { "k10temp",            0 },
      { "zenpower",           0 },
      /* Rockchip RK3588 */
      { "littlecore_thermal", 0 },
      { "bigcore0_thermal",   0 },
      { "bigcore1_thermal",   0 },
      { "bigcore2_thermal",   0 },
      /* Rockchip RK3566 */
      { "soc_thermal",        0 },
      /* Snapdragon 8cx */
      { "cpu0_thermal",       0 },
      { "cpu1_thermal",       0 },
      { "cpu2_thermal",       0 },
      { "cpu3_thermal",       0 },
      { "cpu4_thermal",       0 },
      { "cpu5_thermal",       0 },
      { "cpu6_thermal",       0 },
      { "cpu7_thermal",       0 },
      /* Amlogic S905W */
      { "scpi_sensors",       0 },
      /* Snapdragon 410 */
      { "cpu0_1_thermal",     0 },
      { "cpu2_3_thermal",     0 },
      /* Low priority drivers */
      { "acpitz",             1 },
   };

   for (size_t i = 0; i < ARRAYSIZE(tempDrivers); i++)
      if (String_eq(chip->prefix, tempDrivers[i].prefix))
         return tempDrivers[i].priority;

   return -1;
}

int LibSensors_countCCDs(void) {

#ifndef BUILD_STATIC
   if (!dlopenHandle)
      return 0;
#endif /* !BUILD_STATIC */

   int ccds = 0;

   int n = 0;
   for (const sensors_chip_name* chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      int m = 0;
      for (const sensors_feature* feature = sym_sensors_get_features(chip, &m); feature; feature = sym_sensors_get_features(chip, &m)) {
         if (feature->type != SENSORS_FEATURE_TEMP)
            continue;

         if (!feature->name || !String_startsWith(feature->name, "temp"))
            continue;

         char *label = sym_sensors_get_label(chip, feature);
         if (label) {
            if (String_startsWith(label, "Tccd")) {
               ccds++;
            }
            free(label);
         }
      }
   }

   return ccds;
}

static int LibSensors_stringToID(const char* str) {
   char* endptr;
   unsigned long parsedID = strtoul(str, &endptr, 10);
   if (parsedID >= INT_MAX || *endptr != '\0')
      return -1;
   return (int)parsedID;
}

static bool LibSensors_chipIDFromLabel(const char* label, int* chipID) {
   int parsedID;
   if (String_startsWith(label, "Package id ")) {
      parsedID = LibSensors_stringToID(label + strlen("Package id "));
   } else if (String_startsWith(label, "Physical id ")) {
      parsedID = LibSensors_stringToID(label + strlen("Physical id "));
   } else {
      return false;
   }

   if (parsedID != -1)
      *chipID = parsedID;

   return true;
}

static int LibSensors_countCoretempChips(void) {
   int count = 0;
   int n = 0;

   for (const sensors_chip_name* chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      if (String_eq(chip->prefix, "coretemp"))
         count++;
   }

   return count;
}

/*
 * Collect the physical package IDs of the online CPUs in ascending order.
 *
 * Offline CPUs are not listed in /proc/cpuinfo and keep the zero-initialized
 * physical ID, so including them would invent a package.  libsensors and the
 * coretemp driver only cover online CPUs as well: a chip is removed when all
 * CPUs of its die go offline.
 *
 * The kernel numbers packages and dies in the same order, so the n-th
 * distinct physical ID belongs to the package of the n-th chip, as long as
 * every package has the same number of dies.
 */
static int LibSensors_collectPackageIDs(CPUData* cpus, unsigned int existingCPUs, int* packageIDs) {
   int count = 0;

   for (unsigned int i = 1; i <= existingCPUs; i++) {
      int j = 0;

      if (!cpus[i].online)
         continue;

      while (j < count && packageIDs[j] < cpus[i].physicalID)
         j++;

      if (j < count && packageIDs[j] == cpus[i].physicalID)
         continue;

      memmove(&packageIDs[j + 1], &packageIDs[j], (count - j) * sizeof(*packageIDs));
      packageIDs[j] = cpus[i].physicalID;
      count++;
   }

   return count;
}

/*
 * Collect the addresses of the coretemp chips in ascending order.
 *
 * The address is the platform device ID, which is also the package ID printed
 * by the driver's "Package id" label, so it identifies the chip when that
 * label is missing.
 */
static int LibSensors_collectCoretempAddrs(int* addrs, int max) {
   int kept = 0;
   int n = 0;

   for (const sensors_chip_name* chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      int i;

      if (!String_eq(chip->prefix, "coretemp") || kept == max)
         continue;

      for (i = kept; i > 0 && addrs[i - 1] > chip->addr; i--)
         addrs[i] = addrs[i - 1];
      addrs[i] = chip->addr;
      kept++;
   }

   return kept;
}

static int LibSensors_chipAddrRank(int addr, const int* addrs, int count) {
   for (int i = 0; i < count; i++) {
      if (addrs[i] == addr)
         return i;
   }

   return -1;
}

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs, unsigned int activeCPUs) {
   assert(existingCPUs > 0 && existingCPUs < 16384);

   int* packageIDs = NULL;
   int* chipAddrs = NULL;
   int coretempChips = 0;

   double* data = xMallocArray(existingCPUs + 1, sizeof(double));
   for (size_t i = 0; i < existingCPUs + 1; i++)
      data[i] = NAN;

#ifndef BUILD_STATIC
   if (!dlopenHandle)
      goto out;
#endif /* !BUILD_STATIC */

   unsigned int coreTempCount = 0;
   int topPriority = 99;

   int ccdID = 0;

   int packageCount = 0;
   int diesPerPackage = 0;

   coretempChips = LibSensors_countCoretempChips();
   if (coretempChips > 0) {
      packageIDs = xMallocArray(existingCPUs, sizeof(*packageIDs));
      chipAddrs = xMallocArray(coretempChips, sizeof(*chipAddrs));
      coretempChips = LibSensors_collectCoretempAddrs(chipAddrs, coretempChips);
      packageCount = LibSensors_collectPackageIDs(cpus, existingCPUs, packageIDs);

      if (packageCount > 0 && coretempChips % packageCount == 0)
         diesPerPackage = coretempChips / packageCount;
   }

   int n = 0;
   for (const sensors_chip_name* chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      const int priority = tempDriverPriority(chip);
      if (priority < 0)
         continue;

      if (priority > topPriority)
         continue;

      if (priority < topPriority) {
         /* Clear data from lower priority sensor */
         for (size_t i = 0; i < existingCPUs + 1; i++)
            data[i] = NAN;
      }

      topPriority = priority;

      /* coretemp names a chip after its platform device ID, which is the value
       * its "Package id" label reports too.  The dies of a package are
       * numbered consecutively and in the same order as the packages, so the
       * rank of the address among the detected chips, divided by the number of
       * dies per package, is the position of the package in the ascending list
       * of physical IDs.  Using the rank instead of the address itself also
       * covers addresses and physical IDs that are not consecutive */
      int physicalID = 0;
      if (String_eq(chip->prefix, "coretemp") && diesPerPackage > 0) {
         const int rank = LibSensors_chipAddrRank(chip->addr, chipAddrs, coretempChips);

         if (rank >= 0)
            physicalID = packageIDs[rank / diesPerPackage];
      }

      int m = 0;
      for (const sensors_feature* feature = sym_sensors_get_features(chip, &m); feature; feature = sym_sensors_get_features(chip, &m)) {
         if (feature->type != SENSORS_FEATURE_TEMP)
            continue;

         if (!feature->name || !String_startsWith(feature->name, "temp"))
            continue;

         unsigned long int tempID = strtoul(feature->name + strlen("temp"), NULL, 10);
         if (tempID == 0 || tempID == ULONG_MAX)
            continue;

         /* Feature name IDs start at 1, adjust to start at 0 to match data indices */
         tempID--;

         /* Read the label first: the package id it may carry must not be lost
          * when the temperature itself cannot be read */
         char* label = sym_sensors_get_label(chip, feature);
         bool isChipID = false;
         if (label)
            isChipID = LibSensors_chipIDFromLabel(label, &physicalID);

         const sensors_subfeature* subFeature = sym_sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
         if (!subFeature)
            goto next_feature;

         double temp;
         int r = sym_sensors_get_value(chip, subFeature->number, &temp);
         if (r != 0)
            goto next_feature;

         if (existingCPUs == 8) {
            /* Map temperature values to Snapdragon 8cx cores */
            if (String_startsWith(chip->prefix, "cpu") && chip->prefix[3] >= '0' && chip->prefix[3] <= '7' && String_eq(chip->prefix + 4, "_thermal")) {
               data[1 + chip->prefix[3] - '0'] = temp;
               coreTempCount++;
               goto next_feature;
            }

            /* Map temperature values to Rockchip cores
             *
             *   littlecore -> cores 1..4
             *   bigcore0   -> cores 5,6
             *   bigcore1   -> cores 7,8
             */
            if (String_eq(chip->prefix, "littlecore_thermal")) {
               data[1] = temp;
               data[2] = temp;
               data[3] = temp;
               data[4] = temp;
               coreTempCount += 4;
               goto next_feature;
            }
            if (String_eq(chip->prefix, "bigcore0_thermal")) {
               data[5] = temp;
               data[6] = temp;
               coreTempCount += 2;
               goto next_feature;
            }
            if (String_eq(chip->prefix, "bigcore1_thermal") || String_eq(chip->prefix, "bigcore2_thermal")) {
               data[7] = temp;
               data[8] = temp;
               coreTempCount += 2;
               goto next_feature;
            }
         }

         /* Rockchip RK3566 */
         if (existingCPUs == 4) {
            if (String_eq(chip->prefix, "soc_thermal")) {
               data[1] = temp;
               data[2] = temp;
               data[3] = temp;
               data[4] = temp;
               coreTempCount += 4;
               goto next_feature;
            }
         }

         /* Snapdragon 410 */
         if (existingCPUs == 4) {
            if (String_eq(chip->prefix, "cpu0_1_thermal")) {
               data[1] = temp;
               data[2] = temp;
               coreTempCount += 2;
               goto next_feature;
            }
            if (String_eq(chip->prefix, "cpu2_3_thermal")) {
               data[3] = temp;
               data[4] = temp;
               coreTempCount += 2;
               goto next_feature;
            }
         }

         /* Amlogic S905W */
         if (String_eq(chip->prefix, "scpi_sensors")) {
            // Package temperature - assign to ALL cores and package
            for (size_t i = 0; i <= existingCPUs; i++) {
               data[i] = temp;
            }

            coreTempCount = existingCPUs;
            goto next_feature;
         }

         if (isChipID)
            goto next_feature;

         if (label) {
            bool skip = true;
            int ID;
            /* Intel coretemp names, labels mention the core id */
            if (String_startsWith(label, "Core ")) {
               ID = LibSensors_stringToID(label + strlen("Core "));
               if (ID != -1) {
                  for (size_t i = 1; i < existingCPUs + 1; i++) {
                     if (cpus[i].physicalID == physicalID && cpus[i].coreID == ID) {
                        data[i] = temp;
                        coreTempCount++;
                     }
                  }
               }
            }

            /* AMD k10temp/zenpower names, only CCD is known */
            else if (String_startsWith(label, "Tccd")) {
               for (size_t i = 1; i <= existingCPUs; i++) {
                  if (cpus[i].ccdID == ccdID) {
                     data[i] = temp;
                     coreTempCount++;
                  }
               }
               ccdID++;
            }

            /* AMD k10temp with only one general Tctl */
            else if (String_eq(label, "Tctl")) {
               for (size_t i = 0; i <= existingCPUs; i++) {
                  if (isNaN(data[i])) {
                     data[i] = temp;
                     if (i > 0)
                        coreTempCount++;
                  }
               }
            } else {
               skip = false;
            }

            if (skip)
               goto next_feature;
         }

         if (tempID > existingCPUs)
            goto next_feature;

         /* If already set, e.g. Ryzen reporting platform temperature for each die, use the bigger one */
         if (isNaN(data[tempID])) {
            data[tempID] = temp;
            if (tempID > 0)
               coreTempCount++;
         } else {
            data[tempID] = MAXIMUM(data[tempID], temp);
         }

next_feature:
         free(label);
      }
   }

   /* Adjust data for chips not providing a platform temperature */
   if (coreTempCount + 1 == activeCPUs || coreTempCount + 1 == activeCPUs / 2) {
      memmove(&data[1], &data[0], existingCPUs * sizeof(*data));
      data[0] = NAN;
      coreTempCount++;

      /* Check for further adjustments */
   }

   /* Only package temperature - copy to all cores */
   if (coreTempCount == 0 && !isNaN(data[0])) {
      for (size_t i = 1; i <= existingCPUs; i++)
         data[i] = data[0];

      /* No further adjustments */
      goto out;
   }

   /* No package temperature - set to max core temperature */
   if (coreTempCount > 0 && isNaN(data[0])) {
      double maxTemp = -HUGE_VAL;
      for (size_t i = 1; i <= existingCPUs; i++) {
         if (isgreater(data[i], maxTemp)) {
            maxTemp = data[i];
            data[0] = data[i];
         }
      }

      /* Check for further adjustments */
   }

   /* Only temperature for core 0, maybe Ryzen - copy to all other cores */
   if (coreTempCount == 1 && !isNaN(data[1])) {
      for (size_t i = 2; i <= existingCPUs; i++)
         data[i] = data[1];

      /* No further adjustments */
      goto out;
   }

   /* Half the temperatures, probably HT/SMT - copy to second half */
   const size_t delta = activeCPUs / 2;
   if (coreTempCount == delta) {
      memcpy(&data[delta + 1], &data[1], delta * sizeof(*data));

      /* No further adjustments */
      goto out;
   }

out:
   for (size_t i = 0; i <= existingCPUs; i++)
      cpus[i].temperature = data[i];

   free(chipAddrs);
   free(packageIDs);
   free(data);
}

#endif /* HAVE_SENSORS_SENSORS_H */
