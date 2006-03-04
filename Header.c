/*
htop - Header.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "LoadMeter.h"
#include "LoadAverageMeter.h"
#include "UptimeMeter.h"
#include "ClockMeter.h"
#include "TasksMeter.h"

#include "debug.h"
#include <assert.h>

/*{

typedef enum HeaderSide_ {
   LEFT_HEADER,
   RIGHT_HEADER
} HeaderSide;

typedef struct Header_ {
   TypedVector* leftMeters;
   TypedVector* rightMeters;
   ProcessList* pl;
   bool margin;
   int height;
   int pad;
} Header;

}*/

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

Header* Header_new(ProcessList* pl) {
   Header* this = malloc(sizeof(Header));
   this->leftMeters = TypedVector_new(METER_CLASS, true, DEFAULT_SIZE);
   this->rightMeters = TypedVector_new(METER_CLASS, true, DEFAULT_SIZE);
   this->margin = true;
   this->pl = pl;
   return this;
}

void Header_delete(Header* this) {
   TypedVector_delete(this->leftMeters);
   TypedVector_delete(this->rightMeters);
   free(this);
}

void Header_createMeter(Header* this, char* name, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   if (String_eq(name, "Swap")) {
      TypedVector_add(meters, SwapMeter_new(this->pl));
   } else if (String_eq(name, "Memory")) {
      TypedVector_add(meters, MemoryMeter_new(this->pl));
   } else if (String_eq(name, "Clock")) {
      TypedVector_add(meters, ClockMeter_new(this->pl));
   } else if (String_eq(name, "Load")) {
      TypedVector_add(meters, LoadMeter_new(this->pl));
   } else if (String_eq(name, "LoadAverage")) {
      TypedVector_add(meters, LoadAverageMeter_new(this->pl));
   } else if (String_eq(name, "Uptime")) {
      TypedVector_add(meters, UptimeMeter_new(this->pl));
   } else if (String_eq(name, "Tasks")) {
      TypedVector_add(meters, TasksMeter_new(this->pl));
   } else if (String_startsWith(name, "CPUAverage")) {
      TypedVector_add(meters, CPUMeter_new(this->pl, 0));
   } else if (String_startsWith(name, "CPU")) {
      int num;
      int ok = sscanf(name, "CPU(%d)", &num);
      if (ok)
         TypedVector_add(meters, CPUMeter_new(this->pl, num));
   }
}

void Header_setMode(Header* this, int i, MeterMode mode, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = (Meter*) TypedVector_get(meters, i);
   Meter_setMode(meter, mode);
}

Meter* Header_getMeter(Header* this, int i, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   return (Meter*) TypedVector_get(meters, i);
}

int Header_size(Header* this, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   return TypedVector_size(meters);
}

char* Header_readMeterName(Header* this, int i, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = (Meter*) TypedVector_get(meters, i);
   return meter->name;
}

MeterMode Header_readMeterMode(Header* this, int i, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = (Meter*) TypedVector_get(meters, i);
   return meter->mode;
}

void Header_defaultMeters(Header* this) {
   for (int i = 1; i <= this->pl->processorCount; i++) {
      TypedVector_add(this->leftMeters, CPUMeter_new(this->pl, i));
   }
   TypedVector_add(this->leftMeters, MemoryMeter_new(this->pl));
   TypedVector_add(this->leftMeters, SwapMeter_new(this->pl));
   TypedVector_add(this->rightMeters, TasksMeter_new(this->pl));
   TypedVector_add(this->rightMeters, LoadAverageMeter_new(this->pl));
   TypedVector_add(this->rightMeters, UptimeMeter_new(this->pl));
}

void Header_draw(Header* this) {
   int height = this->height;
   int pad = this->pad;
   
   attrset(CRT_colors[RESET_COLOR]);
   for (int y = 0; y < height; y++) {
      mvhline(y, 0, ' ', COLS);
   }
   for (int y = (pad / 2), i = 0; i < TypedVector_size(this->leftMeters); i++) {
      Meter* meter = (Meter*) TypedVector_get(this->leftMeters, i);
      meter->draw(meter, pad, y, COLS / 2 - (pad * 2 - 1) - 1);
      y += meter->h;
   }
   for (int y = (pad / 2), i = 0; i < TypedVector_size(this->rightMeters); i++) {
      Meter* meter = (Meter*) TypedVector_get(this->rightMeters, i);
      meter->draw(meter, COLS / 2 + pad, y, COLS / 2 - (pad * 2 - 1) - 1);
      y += meter->h;
   }
}

int Header_calculateHeight(Header* this) {
   int pad = this->margin ? 2 : 0;
   int leftHeight = pad;
   int rightHeight = pad;

   for (int i = 0; i < TypedVector_size(this->leftMeters); i++) {
      Meter* meter = (Meter*) TypedVector_get(this->leftMeters, i);
      leftHeight += meter->h;
   }
   for (int i = 0; i < TypedVector_size(this->rightMeters); i++) {
      Meter* meter = (Meter*) TypedVector_get(this->rightMeters, i);
      rightHeight += meter->h;
   }
   this->pad = pad;
   this->height = MAX(leftHeight, rightHeight);
   return this->height;
}
