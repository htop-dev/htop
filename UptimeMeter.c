/*
htop - UptimeMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UptimeMeter.h"

#include "ProcessList.h"
#include "CRT.h"

#include <math.h>

/*{
#include "Meter.h"
}*/

int UptimeMeter_attributes[] = {
   UPTIME
};

static void UptimeMeter_setValues(Meter* this, char* buffer, int len) {
   double uptime = 0;
   FILE* fd = fopen(PROCDIR "/uptime", "r");
   if (fd) {
      fscanf(fd, "%64lf", &uptime);
      fclose(fd);
   }
   int totalseconds = (int) ceil(uptime);
   int seconds = totalseconds % 60;
   int minutes = (totalseconds/60) % 60;
   int hours = (totalseconds/3600) % 24;
   int days = (totalseconds/86400);
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
