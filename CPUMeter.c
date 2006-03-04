/*
htop - CPUMeter.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CPUMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <math.h>

#include "debug.h"
#include <assert.h>

/*{

typedef struct CPUMeter_ CPUMeter;

struct CPUMeter_ {
   Meter super;
   ProcessList* pl;
   int processor;
};

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

CPUMeter* CPUMeter_new(ProcessList* pl, int processor) {
   CPUMeter* this = malloc(sizeof(CPUMeter));
   char* caption;
   if (pl->processorCount == 1 || processor == 0) {
      caption = String_copy("CPU");
   } else {
      caption = (char*) malloc(4);
      sprintf(caption, "%-3d", processor);
   }
   Meter_init((Meter*)this, NULL, caption, 3);
   ((Meter*)this)->name = malloc(20);
   sprintf(((Meter*)this)->name, "CPU(%d)", processor);
   ((Meter*)this)->attributes[0] = CPU_NICE;
   ((Meter*)this)->attributes[1] = CPU_NORMAL;
   ((Meter*)this)->attributes[2] = CPU_KERNEL;
   ((Meter*)this)->setValues = CPUMeter_setValues;
   ((Object*)this)->display = CPUMeter_display;
   ((Meter*)this)->total = 1.0;
   Meter_setMode((Meter*)this, BAR);
   this->processor = processor;
   this->pl = pl;
   return this;
}

void CPUMeter_setValues(Meter* cast) {
   CPUMeter* this = (CPUMeter*)cast;
   cast->values[0] = this->pl->nicePeriod[this->processor] / (double)this->pl->totalPeriod[this->processor];
   cast->values[1] = this->pl->userPeriod[this->processor] / (double)this->pl->totalPeriod[this->processor];
   cast->values[2] = this->pl->systemPeriod[this->processor] / (double)this->pl->totalPeriod[this->processor];
   double cpu = MIN(100.0, MAX(0.0, (cast->values[0]+cast->values[1]+cast->values[2])*100.0 ));
   snprintf(cast->displayBuffer.c, 7, "%5.1f%%", cpu );
}

void CPUMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_prune(out);
   sprintf(buffer, "%5.1f%% ", this->values[1] * 100.0);
   RichString_append(out, CRT_colors[METER_TEXT], ":");
   RichString_append(out, CRT_colors[CPU_NORMAL], buffer);
   sprintf(buffer, "%5.1f%% ", this->values[2] * 100.0);
   RichString_append(out, CRT_colors[METER_TEXT], "sys:");
   RichString_append(out, CRT_colors[CPU_KERNEL], buffer);
   sprintf(buffer, "%5.1f%% ", this->values[0] * 100.0);
   RichString_append(out, CRT_colors[METER_TEXT], "low:");
   RichString_append(out, CRT_colors[CPU_NICE], buffer);
}
