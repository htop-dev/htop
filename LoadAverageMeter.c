/*
htop - LoadAverageMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LoadAverageMeter.h"

#include "CRT.h"

#include <curses.h>
#include <assert.h>

/*{
#include "Meter.h"
}*/

int LoadAverageMeter_attributes[] = {
   LOAD_AVERAGE_FIFTEEN, LOAD_AVERAGE_FIVE, LOAD_AVERAGE_ONE
};

int LoadMeter_attributes[] = { LOAD };

static inline void LoadAverageMeter_scan(double* one, double* five, double* fifteen) {
   int activeProcs, totalProcs, lastProc;
   *one = 0; *five = 0; *fifteen = 0;
   FILE *fd = fopen(PROCDIR "/loadavg", "r");
   if (fd) {
      int total = fscanf(fd, "%32lf %32lf %32lf %32d/%32d %32d", one, five, fifteen,
         &activeProcs, &totalProcs, &lastProc);
      (void) total;
      assert(total == 6);
      fclose(fd);
   }
}

static void LoadAverageMeter_setValues(Meter* this, char* buffer, int size) {
   LoadAverageMeter_scan(&this->values[2], &this->values[1], &this->values[0]);
   snprintf(buffer, size, "%.2f/%.2f/%.2f", this->values[2], this->values[1], this->values[0]);
}

static void LoadAverageMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   char buffer[20];
   sprintf(buffer, "%.2f ", this->values[2]);
   RichString_write(out, CRT_colors[LOAD_AVERAGE_FIFTEEN], buffer);
   sprintf(buffer, "%.2f ", this->values[1]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_FIVE], buffer);
   sprintf(buffer, "%.2f ", this->values[0]);
   RichString_append(out, CRT_colors[LOAD_AVERAGE_ONE], buffer);
}

static void LoadMeter_setValues(Meter* this, char* buffer, int size) {
   double five, fifteen;
   LoadAverageMeter_scan(&this->values[0], &five, &fifteen);
   if (this->values[0] > this->total) {
      this->total = this->values[0];
   }
   snprintf(buffer, size, "%.2f", this->values[0]);
}

static void LoadMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   char buffer[20];
   sprintf(buffer, "%.2f ", ((Meter*)this)->values[0]);
   RichString_write(out, CRT_colors[LOAD], buffer);
}

MeterType LoadAverageMeter = {
   .setValues = LoadAverageMeter_setValues, 
   .display = LoadAverageMeter_display,
   .mode = TEXT_METERMODE,
   .items = 3,
   .total = 100.0,
   .attributes = LoadAverageMeter_attributes,
   .name = "LoadAverage",
   .uiName = "Load average",
   .caption = "Load average: "
};

MeterType LoadMeter = {
   .setValues = LoadMeter_setValues, 
   .display = LoadMeter_display,
   .mode = TEXT_METERMODE,
   .items = 1,
   .total = 100.0,
   .attributes = LoadMeter_attributes,
   .name = "Load",
   .uiName = "Load",
   .caption = "Load: "
};
