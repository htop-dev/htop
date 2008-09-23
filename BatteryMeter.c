/*
  htop
  (C) 2004-2006 Hisham H. Muhammad
  Released under the GNU GPL, see the COPYING file
  in the source distribution for its full text.

  This "Meter" written by Ian P. Hands (iphands@gmail.com).
*/

#include "BatteryMeter.h"
#include "Meter.h"
#include "ProcessList.h"
#include "CRT.h"
#include "String.h"
#include "debug.h"

int BatteryMeter_attributes[] = {
   BATTERY
};

static unsigned long int parseBatInfo(const char * fileName, const unsigned short int lineNum, const unsigned short int wordNum) {
   const DIR *batteryDir;
   const struct dirent *pDirEnt;
  
   const char batteryPath[] = PROCDIR "/acpi/battery/";
   batteryDir = opendir(batteryPath);
  
   if (batteryDir == NULL) {
      return 0;
   }
  
   char * string;
   typedef struct listLbl { char* content; struct listLbl* next; } list;
  
   list *myList = NULL;
   list *newEntry;
  
   /*
   Some of this is based off of code found in kismet (they claim it came from gkrellm).
   Written for multi battery use...
   */
   for (pDirEnt= readdir((DIR*)batteryDir); pDirEnt; pDirEnt = readdir((DIR*)batteryDir)) {
      string = (char*)pDirEnt->d_name;
      if(!strcmp(string, ".") || !strcmp(string, ".."))
         continue;
    
      newEntry = calloc(1, sizeof(list));
      newEntry->next = myList;
      newEntry->content = string;
      myList = newEntry;
   }

   unsigned long int total = 0;
   for (newEntry = myList; newEntry; newEntry = newEntry->next) {
      const char infoPath[30];
      const FILE * file;
      char line[50];  
    
      sprintf((char*)infoPath, "%s%s/%s", batteryPath, newEntry->content, fileName);
    
      if ((file = fopen(infoPath, "r")) == NULL) {
         return 0;
      }

      for (unsigned short int i = 0; i < lineNum; i++){
         fgets(line, sizeof line, (FILE*)file);
      }
    
      fclose((FILE*)file);
    
      const char * foundNumTmp = String_getToken(line, wordNum);
      const unsigned long int foundNum = atoi(foundNumTmp);
      free((char*)foundNumTmp);

      total += foundNum;
   }

   free(myList);
   free(newEntry);
   closedir((DIR*)batteryDir);
   return total;
}

static void BatteryMeter_setValues(Meter* this, char* buffer, int len) {
   FILE* file = fopen(PROCDIR "/acpi/ac_adapter/AC/state", "r");
   if (!file)
      file = fopen(PROCDIR "/acpi/ac_adapter/ADP1/state", "r");
   if (!file) {
      snprintf(buffer, len, "n/a");
      return;
   }  

   char line [100];
   fgets(line, sizeof line, file);
   line[sizeof(line) - 1] = '\0';
   fclose(file);

   const unsigned long int totalFull = parseBatInfo("info", 3, 4);
   const unsigned long int totalRemain = parseBatInfo("state", 5, 3);
   const double percent = totalFull > 0 ? ((double)totalRemain * 100) / (double)totalFull : 0;
   
   if (totalFull == 0) {
      snprintf(buffer, len, "n/a");
      return;
   }
   
   this->values[0] = percent;

   const char* isOnline = String_getToken(line, 2);
   
   char *onAcText, *onBatteryText;
   
   if (this->mode == TEXT_METERMODE) {
      onAcText = "%.1f%% (Running on A/C)";
      onBatteryText = "%.1f%% (Running on battery)";
   } else {
      onAcText = "%.1f%%(A/C)";
      onBatteryText = "%.1f%%(bat)";
   }

   if (strcmp(String_getToken(line, 2),"on-line") == 0) {
      snprintf(buffer, len, onAcText, percent);
   } else {
      snprintf(buffer, len, onBatteryText, percent); 
   }

   free((char*)isOnline);
   return;
}

MeterType BatteryMeter = {
   .setValues = BatteryMeter_setValues, 
   .display = NULL,
   .mode = TEXT_METERMODE,
   .items = 1,
   .total = 100.0,
   .attributes = BatteryMeter_attributes,
   .name = "Battery",
   .uiName = "Battery",
   .caption = "Battery: "
};
