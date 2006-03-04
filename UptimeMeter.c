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

/*{

typedef struct UptimeMeter_ UptimeMeter;

struct UptimeMeter_ {
   Meter super;
   ProcessList* pl;
   int seconds;
   int minutes;
   int hours;
   int days;
};

}*/

UptimeMeter* UptimeMeter_new() {
   UptimeMeter* this = malloc(sizeof(UptimeMeter));
   Meter_init((Meter*)this, String_copy("Uptime"), String_copy("Uptime: "), 1);
   ((Meter*)this)->attributes[0] = UPTIME;
   ((Object*)this)->display = UptimeMeter_display;
   ((Meter*)this)->setValues = UptimeMeter_setValues;
   Meter_setMode((Meter*)this, TEXT);
   ((Meter*)this)->total = 100.0;
   return this;
}

void UptimeMeter_setValues(Meter* cast) {
   UptimeMeter* this = (UptimeMeter*)cast;
   double uptime;
   FILE* fd = fopen(PROCDIR "/uptime", "r");
   fscanf(fd, "%lf", &uptime);
   fclose(fd);
   int totalseconds = (int) ceil(uptime);
   this->seconds = totalseconds % 60;
   this->minutes = (totalseconds-this->seconds) % 3600 / 60;
   this->hours = (totalseconds-this->seconds-(this->minutes*60)) % 86400 / 3600;
   this->days = (totalseconds-this->seconds-(this->minutes*60)-(this->hours*3600)) / 86400;
   cast->values[0] = this->days;
   if (this->days > cast->total) {
      cast->total = this->days;
   }
   snprintf(cast->displayBuffer.c, 14, "%d", this->days);
}

void UptimeMeter_display(Object* cast, RichString* out) {
   UptimeMeter* this = (UptimeMeter*)cast;
   char buffer[20];
   RichString_prune(out);
   if (this->days > 100) {
      sprintf(buffer, "%d days, ", this->days);
      RichString_write(out, CRT_colors[LARGE_NUMBER], buffer);
   } else if (this->days > 1) {
      sprintf(buffer, "%d days, ", this->days);
      RichString_write(out, CRT_colors[UPTIME], buffer);
   } else if (this->days == 1) {
      sprintf(buffer, "%d day, ", this->days);
      RichString_write(out, CRT_colors[UPTIME], buffer);
   }
   sprintf(buffer, "%02d:%02d:%02d ", this->hours, this->minutes, this->seconds);
   RichString_append(out, CRT_colors[UPTIME], buffer);
}
