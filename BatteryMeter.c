/*
htop - BatteryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "BatteryMeter.h"

#include "ProcessList.h"
#include "CRT.h"
#include "String.h"

#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

/*{
#include "Meter.h"

typedef enum ACPresence_ {
   AC_ABSENT,
   AC_PRESENT,
   AC_ERROR
} ACPresence;

}*/

int BatteryMeter_attributes[] = {
   BATTERY
};

static unsigned long int parseUevent(FILE * file, const char *key) {
   char line[100];
   unsigned long int dValue = 0;
   char* saveptr;

   while (fgets(line, sizeof line, file)) {
      if (strncmp(line, key, strlen(key)) == 0) {
         char *value;
         strtok_r(line, "=", &saveptr);
         value = strtok_r(NULL, "=", &saveptr);
         dValue = atoi(value);
         break;
      }
   }
   return dValue;
}

static unsigned long int parseBatInfo(const char *fileName, const unsigned short int lineNum, const unsigned short int wordNum) {
   const char batteryPath[] = PROCDIR "/acpi/battery/";
   DIR* batteryDir = opendir(batteryPath);
   if (!batteryDir)
      return 0;

   #define MAX_BATTERIES 64
   char* batteries[MAX_BATTERIES];
   unsigned int nBatteries = 0;
   memset(batteries, 0, MAX_BATTERIES * sizeof(char*));

   struct dirent result;
   struct dirent* dirEntry;
   while (nBatteries < MAX_BATTERIES) {
      int err = readdir_r(batteryDir, &result, &dirEntry);
      if (err || !dirEntry)
         break;
      char* entryName = dirEntry->d_name;
      if (strncmp(entryName, "BAT", 3))
         continue;
      batteries[nBatteries] = strdup(entryName);
      nBatteries++;
   }
   closedir(batteryDir);

   unsigned long int total = 0;
   for (unsigned int i = 0; i < nBatteries; i++) {
      char infoPath[30];
      snprintf(infoPath, sizeof infoPath, "%s%s/%s", batteryPath, batteries[i], fileName);

      FILE* file = fopen(infoPath, "r");
      if (!file) {
         break;
      }

      char line[50] = "";
      for (unsigned short int i = 0; i < lineNum; i++) {
         char* ok = fgets(line, sizeof line, file);
         if (!ok) break;
      }

      fclose(file);

      char *foundNumStr = String_getToken(line, wordNum);
      const unsigned long int foundNum = atoi(foundNumStr);
      free(foundNumStr);

      total += foundNum;
   }

   for (unsigned int i = 0; i < nBatteries; i++) {
      free(batteries[i]);
   }

   return total;
}

static ACPresence procAcpiCheck() {
   ACPresence isOn = AC_ERROR;
   const char *power_supplyPath = PROCDIR "/acpi/ac_adapter";
   DIR *power_supplyDir = opendir(power_supplyPath);
   if (!power_supplyDir) {
      return AC_ERROR;
   }

   struct dirent result;
   struct dirent* dirEntry;
   for (;;) {
      int err = readdir_r((DIR *) power_supplyDir, &result, &dirEntry);
      if (err || !dirEntry)
         break;

      char* entryName = (char *) dirEntry->d_name;

      if (entryName[0] != 'A')
         continue;

      char statePath[50];
      snprintf((char *) statePath, sizeof statePath, "%s/%s/state", power_supplyPath, entryName);
      FILE* file = fopen(statePath, "r");

      if (!file) {
         isOn = AC_ERROR;
         continue;
      }

      char line[100];
      fgets(line, sizeof line, file);
      line[sizeof(line) - 1] = '\0';

      fclose(file);

      const char *isOnline = String_getToken(line, 2);

      if (strcmp(isOnline, "on-line") == 0) {
         isOn = AC_PRESENT;
      } else {
         isOn = AC_ABSENT;
      }
      free((char *) isOnline);
      if (isOn == AC_PRESENT) {
         break;
      }
   }

   if (power_supplyDir)
      closedir(power_supplyDir);
   return isOn;
}

static ACPresence sysCheck() {
   ACPresence isOn = AC_ERROR;
   const char *power_supplyPath = "/sys/class/power_supply";
   DIR *power_supplyDir = opendir(power_supplyPath);
   if (!power_supplyDir) {
      return AC_ERROR;
   }

   struct dirent result;
   struct dirent* dirEntry;
   for (;;) {
      int err = readdir_r((DIR *) power_supplyDir, &result, &dirEntry);
      if (err || !dirEntry)
         break;

      char* entryName = (char *) dirEntry->d_name;
      if (strncmp(entryName, "A", 1)) {
         continue;
      }
      char onlinePath[50];
      snprintf((char *) onlinePath, sizeof onlinePath, "%s/%s/online", power_supplyPath, entryName);
      FILE* file = fopen(onlinePath, "r");
      if (!file) {
         isOn = AC_ERROR;
      } else {
         isOn = (fgetc(file) - '0');
         fclose(file);
         if (isOn == AC_PRESENT) {
            // If any AC adapter is being used then stop
            break;
         }
      }
   }

   if (power_supplyDir)
      closedir(power_supplyDir);

   return isOn;
}

static ACPresence chkIsOnline() {
   if (access(PROCDIR "/acpi/ac_adapter", F_OK) == 0) {
      return procAcpiCheck();
   } else if (access("/sys/class/power_supply", F_OK) == 0) {
      return sysCheck();
   } else {
      return AC_ERROR;
   }
}

static double getProcBatData() {
   const unsigned long int totalFull = parseBatInfo("info", 3, 4);
   if (totalFull == 0)
      return 0;

   const unsigned long int totalRemain = parseBatInfo("state", 5, 3);
   if (totalRemain == 0)
      return 0;

   return totalRemain * 100.0 / (double) totalFull;
}

static double getSysBatData() {
   const char *power_supplyPath = "/sys/class/power_supply/";
   DIR *power_supplyDir = opendir(power_supplyPath);
   if (!power_supplyDir)
      return 0;

   unsigned long int totalFull = 0;
   unsigned long int totalRemain = 0;

   struct dirent result;
   struct dirent* dirEntry;
   for (;;) {
      int err = readdir_r((DIR *) power_supplyDir, &result, &dirEntry);
      if (err || !dirEntry)
         break;
      char* entryName = (char *) dirEntry->d_name;

      if (strncmp(entryName, "BAT", 3)) {
         continue;
      }

      const char ueventPath[50];

      snprintf((char *) ueventPath, sizeof ueventPath, "%s%s/uevent", power_supplyPath, entryName);

      FILE *file;
      if ((file = fopen(ueventPath, "r")) == NULL) {
         closedir(power_supplyDir);
         return 0;
      }

      if ((totalFull += parseUevent(file, "POWER_SUPPLY_ENERGY_FULL="))) {
         totalRemain += parseUevent(file, "POWER_SUPPLY_ENERGY_NOW=");
      } else {
         //reset file pointer
         if (fseek(file, 0, SEEK_SET) < 0) {
            closedir(power_supplyDir);
            fclose(file);
            return 0;
         }
      }

      //Some systems have it as CHARGE instead of ENERGY.
      if ((totalFull += parseUevent(file, "POWER_SUPPLY_CHARGE_FULL="))) {
         totalRemain += parseUevent(file, "POWER_SUPPLY_CHARGE_NOW=");
      } else {
        //reset file pointer
         if (fseek(file, 0, SEEK_SET) < 0) {
            closedir(power_supplyDir);
            fclose(file);
            return 0;
         }
      }

      fclose(file);
   }

   const double percent = totalFull > 0 ? ((double) totalRemain * 100) / (double) totalFull : 0;
   closedir(power_supplyDir);
   return percent;
}

static void BatteryMeter_setValues(Meter * this, char *buffer, int len) {
   double percent = getProcBatData();

   if (percent == 0) {
      percent = getSysBatData();
      if (percent == 0) {
         snprintf(buffer, len, "n/a");
         return;
      }
   }

   this->values[0] = percent;

   const char *onAcText, *onBatteryText, *unknownText;

   unknownText = "%.1f%%";
   if (this->mode == TEXT_METERMODE) {
      onAcText = "%.1f%% (Running on A/C)";
      onBatteryText = "%.1f%% (Running on battery)";
   } else {
      onAcText = "%.1f%%(A/C)";
      onBatteryText = "%.1f%%(bat)";
   }

   ACPresence isOnLine = chkIsOnline();

   if (isOnLine == AC_PRESENT) {
      snprintf(buffer, len, onAcText, percent);
   } else if (isOnLine == AC_ABSENT) {
      snprintf(buffer, len, onBatteryText, percent);
   } else {
      snprintf(buffer, len, unknownText, percent);
   }

   return;
}

MeterClass BatteryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .setValues = BatteryMeter_setValues,
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = BatteryMeter_attributes,
   .name = "Battery",
   .uiName = "Battery",
   .caption = "Battery: "
};
