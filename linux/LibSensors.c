#include "LibSensors.h"

#ifdef HAVE_SENSORS_SENSORS_H

#include <dlfcn.h>
#include <errno.h>
#include <sensors/sensors.h>

#include "XUtils.h"


static int (*sym_sensors_init)(FILE*);
static void (*sym_sensors_cleanup)(void);
static const sensors_chip_name* (*sym_sensors_get_detected_chips)(const sensors_chip_name*, int*);
static int (*sym_sensors_snprintf_chip_name)(char*, size_t, const sensors_chip_name*);
static const sensors_feature* (*sym_sensors_get_features)(const sensors_chip_name*, int*);
static const sensors_subfeature* (*sym_sensors_get_subfeature)(const sensors_chip_name*, const sensors_feature*, sensors_subfeature_type);
static int (*sym_sensors_get_value)(const sensors_chip_name*, int, double*);

static void* dlopenHandle = NULL;

int LibSensors_init(FILE* input) {
   if (!dlopenHandle) {
      dlopenHandle = dlopen("libsensors.so", RTLD_LAZY);
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

int LibSensors_getCPUTemperatures(CPUData* cpus, int cpuCount) {
   if (!dlopenHandle)
      return -ENOTSUP;

   int tempCount = 0;

   int n = 0;
   for (const sensors_chip_name *chip = sym_sensors_get_detected_chips(NULL, &n); chip; chip = sym_sensors_get_detected_chips(NULL, &n)) {
      char buffer[32];
      sym_sensors_snprintf_chip_name(buffer, sizeof(buffer), chip);
      if (!String_startsWith(buffer, "coretemp") && !String_startsWith(buffer, "cpu_thermal"))
         continue;

      int m = 0;
      for (const sensors_feature *feature = sym_sensors_get_features(chip, &m); feature; feature = sym_sensors_get_features(chip, &m)) {
         if (feature->type != SENSORS_FEATURE_TEMP)
            continue;

         if (feature->number > cpuCount)
            continue;

         const sensors_subfeature *sub_feature = sym_sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
         if (sub_feature) {
            double temp;
            int r = sym_sensors_get_value(chip, sub_feature->number, &temp);
            if (r != 0)
               continue;

            cpus[feature->number].temperature = temp;
            tempCount++;
         }
      }
   }

   return tempCount;
}

#endif /* HAVE_SENSORS_SENSORS_H */
