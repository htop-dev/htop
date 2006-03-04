/*
htop
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LoadMeter.h"
#include "Meter.h"

#include "ProcessList.h"

#include "debug.h"

/*{

typedef struct LoadMeter_ LoadMeter;

struct LoadMeter_ {
   Meter super;
   ProcessList* pl;
};

}*/

LoadMeter* LoadMeter_new() {
   LoadMeter* this = malloc(sizeof(LoadMeter));
   Meter_init((Meter*)this, String_copy("Load"), String_copy("Load: "), 1);
   ((Meter*)this)->attributes[0] = LOAD;
   ((Meter*)this)->setValues = LoadMeter_setValues;
   ((Object*)this)->display = LoadMeter_display;
   Meter_setMode((Meter*)this, GRAPH);
   ((Meter*)this)->total = 1.0;
   return this;
}

/* private */
void LoadMeter_scan(double* one, double* five, double* fifteen) {
   int activeProcs, totalProcs, lastProc;
   FILE *fd = fopen(PROCDIR "/loadavg", "r");
   int read = fscanf(fd, "%lf %lf %lf %d/%d %d", one, five, fifteen,
      &activeProcs, &totalProcs, &lastProc);
   (void) read;
   assert(read == 6);
   fclose(fd);
}

void LoadMeter_setValues(Meter* cast) {
   double five, fifteen;
   LoadMeter_scan(&cast->values[0], &five, &fifteen);
   if (cast->values[0] > cast->total) {
      cast->total = cast->values[0];
   }
   snprintf(cast->displayBuffer.c, 7, "%.2f", cast->values[0]);
}

void LoadMeter_display(Object* cast, RichString* out) {
   LoadMeter* this = (LoadMeter*)cast;
   char buffer[20];
   RichString_prune(out);
   sprintf(buffer, "%.2f ", ((Meter*)this)->values[0]);
   RichString_append(out, CRT_colors[LOAD], buffer);
}
