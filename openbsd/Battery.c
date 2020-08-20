/*
htop - openbsd/Battery.c
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "BatteryMeter.h"
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <errno.h>
#include <string.h>

static bool findDevice(const char* name, int* mib, struct sensordev* snsrdev, size_t* sdlen) {
   for (int devn = 0;; devn++) {
      mib[2] = devn;
      if (sysctl(mib, 3, snsrdev, sdlen, NULL, 0) == -1) {
         if (errno == ENXIO)
            continue;
         if (errno == ENOENT)
            return false;
      }
      if (strcmp(name, snsrdev->xname) == 0) {
         return true;
      }
   }
}

void Battery_getData(double* level, ACPresence* isOnAC) {
   static int mib[] = {CTL_HW, HW_SENSORS, 0, 0, 0};
   struct sensor s;
   size_t slen = sizeof(struct sensor);
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);

   bool found = findDevice("acpibat0", mib, &snsrdev, &sdlen);

   *level = -1;
   if (found) {
      /* last full capacity */
      mib[3] = 7;
      mib[4] = 0;
      double last_full_capacity = 0;
      if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1) {
         last_full_capacity = s.value;
      }
      if (last_full_capacity > 0) {
         /*  remaining capacity */
         mib[3] = 7;
         mib[4] = 3;
         if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1) {
            double charge = s.value;
            *level = 100*(charge / last_full_capacity);
            if (charge >= last_full_capacity) {
               *level = 100;
            }
         }
      }
   }

   found = findDevice("acpiac0", mib, &snsrdev, &sdlen);

   *isOnAC = AC_ERROR;
   if (found) {
      mib[3] = 9;
      mib[4] = 0;
      if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1) {
         *isOnAC = s.value;
      }
   }
}
