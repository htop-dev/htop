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
#define sym_sensors_snprintf_chip_name sensors_snprintf_chip_name

#else

static int (*sym_sensors_init)(FILE*);
static void (*sym_sensors_cleanup)(void);
static const sensors_chip_name* (*sym_sensors_get_detected_chips)(const sensors_chip_name*, int*);
static const sensors_feature* (*sym_sensors_get_features)(const sensors_chip_name*, int*);
static const sensors_subfeature* (*sym_sensors_get_subfeature)(const sensors_chip_name*, const sensors_feature*, sensors_subfeature_type);
static int (*sym_sensors_get_value)(const sensors_chip_name*, int, double*);
static char* (*sym_sensors_get_label)(const sensors_chip_name*, const sensors_feature *feature);
static int (*sym_sensors_snprintf_chip_name)(char*, size_t, const sensors_chip_name*);

static void* dlopenHandle = NULL;

#endif /* BUILD_STATIC */

static bool libSensorsInitialized = false;

int LibSensors_init(void) {
#ifdef BUILD_STATIC

   const int result = sym_sensors_init(NULL);
   libSensorsInitialized = result == 0;
   return result;

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
      resolve(sensors_snprintf_chip_name);

      #undef resolve
   }

   const int result = sym_sensors_init(NULL);
   libSensorsInitialized = result == 0;
   return result;


dlfailure:
   libSensorsInitialized = false;
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

   libSensorsInitialized = false;
}

int LibSensors_reload(void) {
#ifndef BUILD_STATIC
   if (!dlopenHandle) {
      errno = ENOTSUP;
      return -1;
   }
#endif /* !BUILD_STATIC */

   sym_sensors_cleanup();

   const int result = sym_sensors_init(NULL);
   libSensorsInitialized = result == 0;
   return result;
}

static char* LibSensors_makeSensorId(const sensors_chip_name* chip, const sensors_feature* feature) {
   if (!feature->name || !feature->name[0])
      return NULL;

   const int chipLength = sym_sensors_snprintf_chip_name(NULL, 0, chip);
   if (chipLength < 0)
      return NULL;

   const size_t chipNameLength = (size_t)chipLength;
   const size_t featureNameLength = strlen(feature->name);
   char* id = xCalloc(chipNameLength + featureNameLength + 2, 1);

   sym_sensors_snprintf_chip_name(id, chipNameLength + 1, chip);
   id[chipNameLength] = ':';
   memcpy(id + chipNameLength + 1, feature->name, featureNameLength);

   return id;
}

HardwareSensor* LibSensors_getHardwareSensors(size_t* count) {
   assert(count);
   *count = 0;

   if (!libSensorsInitialized)
      return NULL;

#ifndef BUILD_STATIC
   if (!dlopenHandle)
      return NULL;
#endif

   HardwareSensor* sensors = NULL;
   size_t sensorCount = 0;
   size_t capacity = 0;

   int n = 0;
   for (const sensors_chip_name* chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      int m = 0;
      for (const sensors_feature* feature = sym_sensors_get_features(chip, &m); feature; feature = sym_sensors_get_features(chip, &m)) {
         HardwareSensorType type;
         enum sensors_subfeature_type inputType;

         switch (feature->type) {
            case SENSORS_FEATURE_TEMP:
               type = SENSOR_TEMPERATURE;
               inputType = SENSORS_SUBFEATURE_TEMP_INPUT;
               break;
            case SENSORS_FEATURE_FAN:
               type = SENSOR_FAN;
               inputType = SENSORS_SUBFEATURE_FAN_INPUT;
               break;
            default:
               continue;
         }

         const sensors_subfeature* subFeature = sym_sensors_get_subfeature(chip, feature, inputType);
         if (!subFeature)
            continue;

         double value;
         if (sym_sensors_get_value(chip, subFeature->number, &value) != 0 || !isfinite(value))
            continue;

         char* sensorId = LibSensors_makeSensorId(chip, feature);
         if (!sensorId)
            continue;

         if (sensorCount == capacity) {
            capacity = capacity ? capacity * 2 : 8;
            sensors = xReallocArray(sensors, capacity, sizeof(*sensors));
         }

         char* label = sym_sensors_get_label(chip, feature);
         HardwareSensorLabelSource labelSource = SENSOR_LABEL_LIBSENSORS;
         if (!label || !label[0] || String_eq(label, feature->name)) {
            free(label);
            label = chip->prefix && chip->prefix[0] ? xStrdup(chip->prefix) : NULL;
            labelSource = SENSOR_LABEL_FALLBACK;
         }

         sensors[sensorCount] = (HardwareSensor) {
            .id = sensorId,
            .label = label,
            .labelSource = labelSource,
            .type = type,
            .value = value,
            .min = value,
            .average = value,
            .max = value,
            .sampleCount = 0,
         };
         sensorCount++;
      }
   }

   *count = sensorCount;
   return sensors;
}

bool LibSensors_updateHardwareSensors(HardwareSensor* sensors, size_t count) {
   if (!libSensorsInitialized)
      return false;

#ifndef BUILD_STATIC
   if (!dlopenHandle)
      return false;
#endif

   if (count && !sensors)
      return false;

   size_t index = 0;

   int n = 0;
   for (const sensors_chip_name* chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      int m = 0;
      for (const sensors_feature* feature = sym_sensors_get_features(chip, &m); feature; feature = sym_sensors_get_features(chip, &m)) {
         enum sensors_subfeature_type inputType;

         switch (feature->type) {
            case SENSORS_FEATURE_TEMP:
               inputType = SENSORS_SUBFEATURE_TEMP_INPUT;
               break;
            case SENSORS_FEATURE_FAN:
               inputType = SENSORS_SUBFEATURE_FAN_INPUT;
               break;
            default:
               continue;
         }

         const sensors_subfeature* subFeature = sym_sensors_get_subfeature(chip, feature, inputType);
         if (!subFeature)
            continue;

         double value;
         if (sym_sensors_get_value(chip, subFeature->number, &value) != 0 || !isfinite(value))
            continue;

         char* sensorId = LibSensors_makeSensorId(chip, feature);
         if (!sensorId)
            continue;

         if (index >= count || !sensors[index].id || !String_eq(sensors[index].id, sensorId)) {
            free(sensorId);
            return false;
         }
         free(sensorId);

         HardwareSensor* sensor = &sensors[index++];
         sensor->value = value;

         if (sensor->sampleCount == 0) {
            sensor->min = value;
            sensor->average = value;
            sensor->max = value;
            sensor->sampleCount = 1;
            continue;
         }

         if (value < sensor->min)
            sensor->min = value;
         if (value > sensor->max)
            sensor->max = value;

         sensor->sampleCount++;
         sensor->average += (value - sensor->average) / (double)sensor->sampleCount;
      }
   }

   return index == count;
}

void LibSensors_freeHardwareSensors(HardwareSensor* sensors, size_t count) {
   for (size_t i = 0; i < count; i++) {
      free(sensors[i].id);
      free(sensors[i].label);
   }
   free(sensors);
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

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int existingCPUs, unsigned int activeCPUs) {
   assert(existingCPUs > 0 && existingCPUs < 16384);

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

      int physicalID = 0;

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

         const sensors_subfeature* subFeature = sym_sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
         if (!subFeature)
            continue;

         double temp;
         int r = sym_sensors_get_value(chip, subFeature->number, &temp);
         if (r != 0)
            continue;

         if (existingCPUs == 8) {
            /* Map temperature values to Snapdragon 8cx cores */
            if (String_startsWith(chip->prefix, "cpu") && chip->prefix[3] >= '0' && chip->prefix[3] <= '7' && String_eq(chip->prefix + 4, "_thermal")) {
               data[1 + chip->prefix[3] - '0'] = temp;
               coreTempCount++;
               continue;
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
               continue;
            }
            if (String_eq(chip->prefix, "bigcore0_thermal")) {
               data[5] = temp;
               data[6] = temp;
               coreTempCount += 2;
               continue;
            }
            if (String_eq(chip->prefix, "bigcore1_thermal") || String_eq(chip->prefix, "bigcore2_thermal")) {
               data[7] = temp;
               data[8] = temp;
               coreTempCount += 2;
               continue;
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
               continue;
            }
         }

         /* Snapdragon 410 */
         if (existingCPUs == 4) {
            if (String_eq(chip->prefix, "cpu0_1_thermal")) {
               data[1] = temp;
               data[2] = temp;
               coreTempCount += 2;
               continue;
            }
            if (String_eq(chip->prefix, "cpu2_3_thermal")) {
               data[3] = temp;
               data[4] = temp;
               coreTempCount += 2;
               continue;
            }
         }

         /* Amlogic S905W */
         if (String_eq(chip->prefix, "scpi_sensors")) {
            // Package temperature - assign to ALL cores and package
            for (size_t i = 0; i <= existingCPUs; i++) {
               data[i] = temp;
            }

            coreTempCount = existingCPUs;
            continue;
         }

         char *label = sym_sensors_get_label(chip, feature);
         if (label) {
            bool skip = true;
            int ID;
            /* Intel coretemp names, labels mention package and physical id */
            if (String_startsWith(label, "Package id ")) {
               if ((ID = LibSensors_stringToID(label + strlen("Package id "))) != -1)
                  physicalID = ID;
            } else if (String_startsWith(label, "Physical id ")) {
               if ((ID = LibSensors_stringToID(label + strlen("Physical id "))) != -1)
                  physicalID = ID;
            } else if (String_startsWith(label, "Core ")) {
               if ((ID = LibSensors_stringToID(label + strlen("Core "))) != -1) {
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

            free(label);

            if (skip)
               continue;
         }

         if (tempID > existingCPUs)
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

   free(data);
}

#endif /* HAVE_SENSORS_SENSORS_H */
