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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sensors/sensors.h>

#include "Macros.h"
#include "XUtils.h"
#include "linux/LinuxMachine.h"


#ifdef BUILD_STATIC

#define sym_sensors_init               sensors_init
#define sym_sensors_cleanup            sensors_cleanup
#define sym_sensors_get_detected_chips sensors_get_detected_chips
#define sym_sensors_get_features       sensors_get_features
#define sym_sensors_get_subfeature     sensors_get_subfeature
#define sym_sensors_get_value          sensors_get_value

#else

static int (*sym_sensors_init)(FILE*);
static void (*sym_sensors_cleanup)(void);
static const sensors_chip_name* (*sym_sensors_get_detected_chips)(const sensors_chip_name*, int*);
static const sensors_feature* (*sym_sensors_get_features)(const sensors_chip_name*, int*);
static const sensors_subfeature* (*sym_sensors_get_subfeature)(const sensors_chip_name*, const sensors_feature*, sensors_subfeature_type);
static int (*sym_sensors_get_value)(const sensors_chip_name*, int, double*);

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

typedef enum TempDriver_ {
   TD_CORETEMP,
   TD_CPUTEMP,
   TD_CPUTHERMAL,
   TD_K10TEMP,
   TD_ZENPOWER,
   TD_ACPITZ,
   TD_UNKNOWN,
} TempDriver;

static const struct TempDriverDefs {
      const char* prefix;
      int priority;
} tempDrivers[TD_UNKNOWN] =  {
   [TD_CORETEMP]   = { "coretemp",    0 },
   [TD_CPUTEMP]    = { "via_cputemp", 0 },
   [TD_CPUTHERMAL] = { "cpu_thermal", 0 },
   [TD_K10TEMP]    = { "k10temp",     0 },
   [TD_ZENPOWER]   = { "zenpower",    0 },
   /* Low priority drivers */
   [TD_ACPITZ]     = { "acpitz",      1 },
};

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs, unsigned int activeCPUs, unsigned short maxCoreId) {
   assert(existingCPUs > 0 && existingCPUs < 16384);

   float* data = xMallocArray(existingCPUs + 1, sizeof(float));
   for (size_t i = 0; i <= existingCPUs; i++)
      data[i] = NAN;

#ifndef BUILD_STATIC
   if (!dlopenHandle)
      goto out;
#endif /* !BUILD_STATIC */

   unsigned int coreTempCount = 0;
   int topPriority = 99;
   TempDriver topDriver = TD_UNKNOWN;

   int n = 0;
   for (const sensors_chip_name* chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      int priority = -1;
      TempDriver driver = TD_UNKNOWN;

      for (size_t i = 0; i < ARRAYSIZE(tempDrivers); i++) {
         if (chip->prefix && String_eq(chip->prefix, tempDrivers[i].prefix)) {
            priority = tempDrivers[i].priority;
            driver = i;
            break;
         }
      }

      if (driver == TD_UNKNOWN)
         continue;

      if (priority > topPriority)
         continue;

      if (priority < topPriority) {
         /* Clear data from lower priority sensor */
         for (size_t i = 0; i < existingCPUs + 1; i++)
            data[i] = NAN;
      }

      topPriority = priority;
      topDriver = driver;

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

         if (tempID > existingCPUs)
            continue;

         const sensors_subfeature* subFeature = sym_sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
         if (!subFeature)
            continue;

         double temp;
         int r = sym_sensors_get_value(chip, subFeature->number, &temp);
         if (r != 0)
            continue;

         /* If already set, e.g. Ryzen reporting platform temperature for each die, use the bigger one */
         if (isNaN(data[tempID])) {
            data[tempID] = temp;
            if (tempID > 0)
               coreTempCount++;
         } else {
            data[tempID] = MAXIMUM(data[tempID], temp);
         }
      }
   }

   /*
    * k10temp, see https://www.kernel.org/doc/html/latest/hwmon/k10temp.html
    *   temp1 = Tctl, (optional) temp2 = Tdie, temp3..temp10 = Tccd1..8
    */
   if (topDriver == TD_K10TEMP) {
      /* Display Tdie instead of Tctl if available */
      if (!isNaN(data[1]))
         data[0] = data[1];

      /* Compute number of CCD entries */
      unsigned int ccd_entries = 0;
      for (size_t i = 2; i <= existingCPUs; i++) {
         if (isNaN(data[i]))
            break;

         ccd_entries++;
      }

      if (ccd_entries == 0) {
         const float ccd_temp = data[0];
         for (size_t i = 1; i <= existingCPUs; i++)
            data[i] = ccd_temp;
      } else if (ccd_entries == 1) {
         const float ccd_temp = data[2];
         for (size_t i = 1; i <= existingCPUs; i++)
            data[i] = ccd_temp;
      } else {
         assert(ccd_entries <= 64);
         float ccd_data[ccd_entries];
         for (size_t i = 0; i < ccd_entries; i++)
            ccd_data[i] = data[i + 2];

         /* Estimate the size of the CCDs, might be off of due to
          * disabled cores on downgraded CPUs */
         const unsigned int ccd_size = maxCoreId / ccd_entries + 1;

         for (size_t i = 0; i < existingCPUs; i++) {
            unsigned short coreId = cpus[i + 1].coreId;
            unsigned int index = MINIMUM(coreId / ccd_size, ccd_entries - 1);
            data[i + 1] = ccd_data[index];
         }
      }

      /* No further adjustments */
      goto out;
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
      float maxTemp = -HUGE_VALF;
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

   free(data);
}

#endif /* HAVE_SENSORS_SENSORS_H */
