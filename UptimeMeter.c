/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UptimeMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include "CRT.h"

#include "debug.h"

int UptimeMeter_attributes[] = {
   UPTIME
};

static void UptimeMeter_setValues(Meter* this, char* buffer, int len) {
   double uptime;
   FILE* fd = fopen(PROCDIR "/uptime", "r");
   fscanf(fd, "%lf", &uptime);
   fclose(fd);
   int totalseconds = (int) ceil(uptime);
   int seconds = totalseconds % 60;
   int minutes = (totalseconds-seconds) % 3600 / 60;
   int hours = (totalseconds-seconds-(minutes*60)) % 86400 / 3600;
   int days = (totalseconds-seconds-(minutes*60)-(hours*3600)) / 86400;
   this->values[0] = days;
   if (days > this->total) {
      this->total = days;
   }
   char daysbuf[15];
   if (days > 100) {
      sprintf(daysbuf, "%d days(!), ", days);
   } else if (days > 1) {
      sprintf(daysbuf, "%d days, ", days);
   } else if (days == 1) {
      sprintf(daysbuf, "1 day, ");
   } else {
      daysbuf[0] = '\0';
   }
   snprintf(buffer, len, "%s%02d:%02d:%02d", daysbuf, hours, minutes, seconds);
}

MeterType UptimeMeter = {
   .setValues = UptimeMeter_setValues, 
   .display = NULL,
   .mode = TEXT_METERMODE,
   .items = 1,
   .total = 100.0,
   .attributes = UptimeMeter_attributes,
   .name = "Uptime",
   .uiName = "Uptime",
   .caption = "Uptime: "
};
