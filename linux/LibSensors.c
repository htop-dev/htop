#include "LibSensors.h"

#ifdef HAVE_SENSORS_SENSORS_H

#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <sensors/sensors.h>

#include "XUtils.h"


static int (*sym_sensors_init)(FILE*);
static void (*sym_sensors_cleanup)(void);
static const sensors_chip_name* (*sym_sensors_get_detected_chips)(const sensors_chip_name*, int*);
static int (*sym_sensors_snprintf_chip_name)(char*, size_t, const sensors_chip_name*);
static const sensors_feature* (*sym_sensors_get_features)(const sensors_chip_name*, int*);
static const sensors_subfeature* (*sym_sensors_get_subfeature)(const sensors_chip_name*, const sensors_feature*, sensors_subfeature_type);
static int (*sym_sensors_get_value)(const sensors_chip_name*, int, double*);
static char* (*sym_sensors_get_label)(const sensors_chip_name*, const sensors_feature*);

static void* dlopenHandle = NULL;

int LibSensors_init(FILE* input) {
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
      resolve(sensors_snprintf_chip_name);
      resolve(sensors_get_features);
      resolve(sensors_get_subfeature);
      resolve(sensors_get_value);
      resolve(sensors_get_label);

      #undef resolve
   }

   return sym_sensors_init(input);

dlfailure:
   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
   return -1;
}

void LibSensors_cleanup(void) {
   if (dlopenHandle) {
      sym_sensors_cleanup();

      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
}

void LibSensors_getCPUTemperatures(CPUData* cpus, unsigned int cpuCount) {
   for (unsigned int i = 0; i <= cpuCount; i++)
      cpus[i].temperature = NAN;

   if (!dlopenHandle)
      return;

   unsigned int coreTempCount = 0;

   int n = 0;
   for (const sensors_chip_name *chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      char buffer[32];
      sym_sensors_snprintf_chip_name(buffer, sizeof(buffer), chip);
      if (!String_startsWith(buffer, "coretemp") &&
          !String_startsWith(buffer, "cpu_thermal") &&
          !String_startsWith(buffer, "k10temp") &&
          !String_startsWith(buffer, "zenpower"))
         continue;

      int m = 0;
      for (const sensors_feature *feature = sym_sensors_get_features(chip, &m); feature; feature = sym_sensors_get_features(chip, &m)) {
         if (feature->type != SENSORS_FEATURE_TEMP)
            continue;

         char* label = sym_sensors_get_label(chip, feature);
         if (!label)
            continue;

         unsigned int tempId;
         if (String_startsWith(label, "Package ")) {
            tempId = 0;
         } else if (String_startsWith(label, "temp")) {
            /* Raspberry Pi has only temp1 */
            tempId = 0;
         } else if (String_startsWith(label, "Tdie")) {
            tempId = 0;
         } else if (String_startsWith(label, "Core ")) {
            tempId = 1 + atoi(label + strlen("Core "));
         } else {
            tempId = UINT_MAX;
         }

         free(label);

         if (tempId > cpuCount)
            continue;

         const sensors_subfeature *sub_feature = sym_sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
         if (sub_feature) {
            double temp;
            int r = sym_sensors_get_value(chip, sub_feature->number, &temp);
            if (r != 0)
               continue;

            cpus[tempId].temperature = temp;
            if (tempId > 0)
               coreTempCount++;
         }
      }
   }

   const double packageTemp = cpus[0].temperature;

   /* Only package temperature - copy to all cpus */
   if (coreTempCount == 0 && !isnan(packageTemp)) {
      for (unsigned int i = 1; i <= cpuCount; i++)
         cpus[i].temperature = packageTemp;

      return;
   }

   /* No package temperature - set to max core temperature */
   if (isnan(packageTemp) && coreTempCount != 0) {
      double maxTemp = NAN;
      for (unsigned int i = 1; i <= cpuCount; i++) {
         const double coreTemp = cpus[i].temperature;
         if (isnan(coreTemp))
            continue;

         maxTemp = MAXIMUM(maxTemp, coreTemp);
      }

      cpus[0].temperature = maxTemp;
   }

   /* Half the temperatures, probably HT/SMT - copy to second half */
   const unsigned int delta = cpuCount / 2;
   if (coreTempCount == delta) {
      for (unsigned int i = 1; i <= delta; i++)
         cpus[i + delta].temperature = cpus[i].temperature;
   }
}

#endif /* HAVE_SENSORS_SENSORS_H */
