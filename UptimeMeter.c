/*
htop - UptimeMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UptimeMeter.h"
#include "Platform.h"
#include "CRT.h"


int UptimeMeter_attributes[] = {
   UPTIME
};

static void UptimeMeter_updateValues(Meter* this, char* buffer, int len) {
   int totalseconds = Platform_getUptime();
   if (totalseconds == -1) {
      xSnprintf(buffer, len, "(unknown)");
      return;
   }
   int seconds = totalseconds % 60;
   int minutes = (totalseconds/60) % 60;
   int hours = (totalseconds/3600) % 24;
   int days = (totalseconds/86400);
   this->values[0] = days;
   if (days > this->total) {
      this->total = days;
   }
   char daysbuf[32];
   if (days > 100) {
      xSnprintf(daysbuf, sizeof(daysbuf), "%d days(!), ", days);
   } else if (days > 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "%d days, ", days);
   } else if (days == 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "1 day, ");
   } else {
      daysbuf[0] = '\0';
   }
   xSnprintf(buffer, len, "%s%02d:%02d:%02d", daysbuf, hours, minutes, seconds);
}

MeterClass UptimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = UptimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = UptimeMeter_attributes,
   .name = "Uptime",
   .uiName = "Uptime",
   .caption = "Uptime: "
};
