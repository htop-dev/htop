/*
htop - SysArchMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/
#include "config.h"  // IWYU pragma: keep

#include "SysArchMeter.h"

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#include "XUtils.h"


static const int SysArchMeter_attributes[] = {HOSTNAME};

static void SysArchMeter_updateValues(Meter* this, char* buffer, size_t size) {
   static struct utsname uname_info;
   static int uname_result;
   static char distro[3][64] = { {'\0'}, {'\0'}, {'\0'} };
   static bool loaded_data = false;

   (void)this;

   if(!loaded_data) {
      uname_result = uname(&uname_info);
      FILE* fp = popen("lsb_release --id --release --codename", "r");
      if(fp) {
         char line[96] = {'\0'};
         size_t n = 0;

         while(fgets(line, sizeof(line), fp)) {
            n = strcspn(line, ":");
            if(n > 0 && (n + 1) < strlen(line)) {
               char* value = String_trim(&line[n + 1]);
               line[n] = '\0';

               if(String_eq(line, "Distributor ID"))
                  snprintf(distro[0], sizeof(distro[0]), "%s", value);
               else if(String_eq(line, "Release"))
                  snprintf(distro[1], sizeof(distro[1]), "%s", value);
               else if(String_eq(line, "Codename"))
                  snprintf(distro[2], sizeof(distro[2]), "%s", value);

               free(value);
            }
         }
         if(!distro[0][0])
            snprintf(distro[0], sizeof(distro[0]), "Unknown");
         pclose(fp);
      }
      loaded_data = true;
   }

   if(uname_result == 0) {
      if (distro[1][0] && distro[2][0])
         snprintf(buffer, size, "%s %s [%s] / %s %s (%s)", uname_info.sysname, uname_info.release, uname_info.machine, distro[0], distro[1], distro[2]);
      else if(distro[1][0])
         snprintf(buffer, size, "%s %s [%s] / %s %s", uname_info.sysname, uname_info.release, uname_info.machine, distro[0], distro[1]);
      else
         snprintf(buffer, size, "%s %s [%s]", uname_info.sysname, uname_info.release, uname_info.machine);
   } else {
      snprintf(buffer, size, "Unknown");
   }
}

const MeterClass SysArchMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = SysArchMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = SysArchMeter_attributes,
   .name = "System",
   .uiName = "System",
   .caption = "System: ",
};
