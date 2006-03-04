/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LoadAverageMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include <curses.h>

#include "debug.h"

/*{

typedef struct LoadAverageMeter_ LoadAverageMeter;

struct LoadAverageMeter_ {
   Meter super;
   ProcessList* pl;
};

}*/

/* private property */
void LoadAverageMeter_scan(double* one, double* five, double* fifteen);

LoadAverageMeter* LoadAverageMeter_new() {
   LoadAverageMeter* this = malloc(sizeof(LoadAverageMeter));
   Meter_init((Meter*)this, String_copy("LoadAverage"), String_copy("Load average: "), 3);
   ((Meter*)this)->attributes[0] = LOAD_AVERAGE_FIFTEEN;
   ((Meter*)this)->attributes[1] = LOAD_AVERAGE_FIVE;
   ((Meter*)this)->attributes[2] = LOAD_AVERAGE_ONE;
   ((Object*)this)->display = LoadAverageMeter_display;
   ((Meter*)this)->setValues = LoadAverageMeter_setValues;
   Meter_setMode((Meter*)this, TEXT);
   LoadAverageMeter_scan(&((Meter*)this)->values[0], &((Meter*)this)->values[1], &((Meter*)this)->values[2]);
   ((Meter*)this)->total = 100.0;
   return this;
}

/* private */
void LoadAverageMeter_scan(double* one, double* five, double* fifteen) {
   int activeProcs, totalProcs, lastProc;
   FILE *fd = fopen(PROCDIR "/loadavg", "r");
   int read = fscanf(fd, "%lf %lf %lf %d/%d %d", one, five, fifteen,
      &activeProcs, &totalProcs, &lastProc);
   (void) read;
   assert(read == 6);
   fclose(fd);
}

void LoadAverageMeter_setValues(Meter* cast) {
   LoadAverageMeter_scan(&cast->values[2], &cast->values[1], &cast->values[0]);
   snprintf(cast->displayBuffer.c, 25, "%.2f/%.2f/%.2f", cast->values[2], cast->values[1], cast->values[0]);
}

void LoadAverageMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   char buffer[20];
   RichString_prune(out);
   sprintf(buffer, "%.2f ", this->values[2]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_ONE], buffer);
   sprintf(buffer, "%.2f ", this->values[1]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_FIVE], buffer);
   sprintf(buffer, "%.2f ", this->values[0]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_FIFTEEN], buffer);
}
