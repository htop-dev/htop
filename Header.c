/*
htop - Header.c
(C) 2004-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"
#include "Meter.h"

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

   char* paren = strchr(name, '(');
   int param = 0;
   if (paren) {
      int ok = sscanf(paren, "(%d)", &param);
      if (!ok) param = 0;
      *paren = '\0';
   }
   for (MeterType** type = Meter_types; *type; type++) {
      if (String_eq(name, (*type)->name)) {
         TypedVector_add(meters, Meter_new(this->pl, param, *type));
         break;
      }
   }
}

void Header_setMode(Header* this, int i, MeterModeId mode, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = (Meter*) TypedVector_get(meters, i);
   Meter_setMode(meter, mode);
}

Meter* Header_addMeter(Header* this, MeterType* type, int param, HeaderSide side) {
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = Meter_new(this->pl, param, type);
   TypedVector_add(meters, meter);
   return meter;
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
   TypedVector* meters = side == LEFT_HEADER
                       ? this->leftMeters
                       : this->rightMeters;

   Meter* meter = (Meter*) TypedVector_get(meters, i);
   return meter->mode;
}

void Header_defaultMeters(Header* this) {
   TypedVector_add(this->leftMeters, Meter_new(this->pl, 0, &AllCPUsMeter));
   TypedVector_add(this->leftMeters, Meter_new(this->pl, 0, &MemoryMeter));
   TypedVector_add(this->leftMeters, Meter_new(this->pl, 0, &SwapMeter));
   TypedVector_add(this->rightMeters, Meter_new(this->pl, 0, &TasksMeter));
   TypedVector_add(this->rightMeters, Meter_new(this->pl, 0, &LoadAverageMeter));
   TypedVector_add(this->rightMeters, Meter_new(this->pl, 0, &UptimeMeter));
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
