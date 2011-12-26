/*
htop - Header.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"

#include "CRT.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "UptimeMeter.h"
#include "BatteryMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "String.h"

#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

/*{
#include "ProcessList.h"
#include "Meter.h"

typedef enum HeaderSide_ {
   LEFT_HEADER,
   RIGHT_HEADER
} HeaderSide;

typedef struct Header_ {
   Vector* leftMeters;
   Vector* rightMeters;
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
   Header* this = calloc(sizeof(Header), 1);
   this->leftMeters = Vector_new(METER_CLASS, true, DEFAULT_SIZE, NULL);
   this->rightMeters = Vector_new(METER_CLASS, true, DEFAULT_SIZE, NULL);
   this->margin = true;
   this->pl = pl;
   return this;
}

void Header_delete(Header* this) {
   Vector_delete(this->leftMeters);
   Vector_delete(this->rightMeters);
   free(this);
}

void Header_createMeter(Header* this, char* name, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   char* paren = strchr(name, '(');
   int param = 0;
   if (paren) {
      int ok = sscanf(paren, "(%d)", &param);
      if (!ok) param = 0;
      *paren = '\0';
   }
   for (MeterType** type = Meter_types; *type; type++) {
      if (String_eq(name, (*type)->name)) {
         Vector_add(meters, Meter_new(this->pl, param, *type));
         break;
      }
   }
}

void Header_setMode(Header* this, int i, MeterModeId mode, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   if (i >= Vector_size(meters))
      return;
   Meter* meter = (Meter*) Vector_get(meters, i);
   Meter_setMode(meter, mode);
}

Meter* Header_addMeter(Header* this, MeterType* type, int param, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = Meter_new(this->pl, param, type);
   Vector_add(meters, meter);
   return meter;
}

int Header_size(Header* this, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   return Vector_size(meters);
}

char* Header_readMeterName(Header* this, int i, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;
   Meter* meter = (Meter*) Vector_get(meters, i);

   int nameLen = strlen(meter->type->name);
   int len = nameLen + 100;
   char* name = malloc(len);
   strncpy(name, meter->type->name, nameLen);
   name[nameLen] = '\0';
   if (meter->param)
      snprintf(name + nameLen, len - nameLen, "(%d)", meter->param);

   return name;
}

MeterModeId Header_readMeterMode(Header* this, int i, HeaderSide side) {
   Vector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = (Meter*) Vector_get(meters, i);
   return meter->mode;
}

void Header_defaultMeters(Header* this, int cpuCount) {
   if (cpuCount > 8) {
      Vector_add(this->leftMeters, Meter_new(this->pl, 0, &LeftCPUs2Meter));
      Vector_add(this->rightMeters, Meter_new(this->pl, 0, &RightCPUs2Meter));
   } else if (cpuCount > 4) {
      Vector_add(this->leftMeters, Meter_new(this->pl, 0, &LeftCPUsMeter));
      Vector_add(this->rightMeters, Meter_new(this->pl, 0, &RightCPUsMeter));
   } else {
      Vector_add(this->leftMeters, Meter_new(this->pl, 0, &AllCPUsMeter));
   }
   Vector_add(this->leftMeters, Meter_new(this->pl, 0, &MemoryMeter));
   Vector_add(this->leftMeters, Meter_new(this->pl, 0, &SwapMeter));
   Vector_add(this->rightMeters, Meter_new(this->pl, 0, &TasksMeter));
   Vector_add(this->rightMeters, Meter_new(this->pl, 0, &LoadAverageMeter));
   Vector_add(this->rightMeters, Meter_new(this->pl, 0, &UptimeMeter));
}

void Header_reinit(Header* this) {
   for (int i = 0; i < Vector_size(this->leftMeters); i++) {
      Meter* meter = (Meter*) Vector_get(this->leftMeters, i);
      if (meter->type->init)
         meter->type->init(meter);
   }
   for (int i = 0; i < Vector_size(this->rightMeters); i++) {
      Meter* meter = (Meter*) Vector_get(this->rightMeters, i);
      if (meter->type->init)
         meter->type->init(meter);
   }
}

void Header_draw(const Header* this) {
   int height = this->height;
   int pad = this->pad;
   
   attrset(CRT_colors[RESET_COLOR]);
   for (int y = 0; y < height; y++) {
      mvhline(y, 0, ' ', COLS);
   }
   for (int y = (pad / 2), i = 0; i < Vector_size(this->leftMeters); i++) {
      Meter* meter = (Meter*) Vector_get(this->leftMeters, i);
      meter->draw(meter, pad, y, COLS / 2 - (pad * 2 - 1) - 1);
      y += meter->h;
   }
   for (int y = (pad / 2), i = 0; i < Vector_size(this->rightMeters); i++) {
      Meter* meter = (Meter*) Vector_get(this->rightMeters, i);
      meter->draw(meter, COLS / 2 + pad, y, COLS / 2 - (pad * 2 - 1) - 1);
      y += meter->h;
   }
}

int Header_calculateHeight(Header* this) {
   int pad = this->margin ? 2 : 0;
   int leftHeight = pad;
   int rightHeight = pad;

   for (int i = 0; i < Vector_size(this->leftMeters); i++) {
      Meter* meter = (Meter*) Vector_get(this->leftMeters, i);
      leftHeight += meter->h;
   }
   for (int i = 0; i < Vector_size(this->rightMeters); i++) {
      Meter* meter = (Meter*) Vector_get(this->rightMeters, i);
      rightHeight += meter->h;
   }
   this->pad = pad;
   this->height = MAX(leftHeight, rightHeight);
   return this->height;
}
