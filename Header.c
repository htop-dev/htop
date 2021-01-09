/*
htop - Header.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "XUtils.h"


Header* Header_new(struct ProcessList_* pl, Settings* settings, int nrColumns) {
   Header* this = xCalloc(1, sizeof(Header));
   this->columns = xCalloc(nrColumns, sizeof(Vector*));
   this->settings = settings;
   this->pl = pl;
   this->nrColumns = nrColumns;
   Header_forEachColumn(this, i) {
      this->columns[i] = Vector_new(Class(Meter), true, DEFAULT_SIZE);
   }
   return this;
}

void Header_delete(Header* this) {
   Header_forEachColumn(this, i) {
      Vector_delete(this->columns[i]);
   }
   free(this->columns);
   free(this);
}

void Header_populateFromSettings(Header* this) {
   Header_forEachColumn(this, col) {
      const MeterColumnSettings* colSettings = &this->settings->columns[col];
      for (int i = 0; i < colSettings->len; i++) {
         Header_addMeterByName(this, colSettings->names[i], col);
         if (colSettings->modes[i] != 0) {
            Header_setMode(this, i, colSettings->modes[i], col);
         }
      }
   }
   Header_calculateHeight(this);
}

void Header_writeBackToSettings(const Header* this) {
   Header_forEachColumn(this, col) {
      MeterColumnSettings* colSettings = &this->settings->columns[col];

      String_freeArray(colSettings->names);
      free(colSettings->modes);

      const Vector* vec = this->columns[col];
      int len = Vector_size(vec);

      colSettings->names = xCalloc(len + 1, sizeof(char*));
      colSettings->modes = xCalloc(len, sizeof(int));
      colSettings->len = len;

      for (int i = 0; i < len; i++) {
         const Meter* meter = (Meter*) Vector_get(vec, i);
         char* name;
         if (meter->param) {
            xAsprintf(&name, "%s(%d)", As_Meter(meter)->name, meter->param);
         } else {
            xAsprintf(&name, "%s", As_Meter(meter)->name);
         }
         colSettings->names[i] = name;
         colSettings->modes[i] = meter->mode;
      }
   }
}

MeterModeId Header_addMeterByName(Header* this, const char* name, int column) {
   Vector* meters = this->columns[column];

   char* paren = strchr(name, '(');
   int param = 0;
   if (paren) {
      int ok = sscanf(paren, "(%10d)", &param);
      if (!ok)
         param = 0;
      *paren = '\0';
   }
   MeterModeId mode = TEXT_METERMODE;
   for (const MeterClass* const* type = Platform_meterTypes; *type; type++) {
      if (String_eq(name, (*type)->name)) {
         Meter* meter = Meter_new(this->pl, param, *type);
         Vector_add(meters, meter);
         mode = meter->mode;
         break;
      }
   }

   if (paren)
      *paren = '(';

   return mode;
}

void Header_setMode(Header* this, int i, MeterModeId mode, int column) {
   Vector* meters = this->columns[column];

   if (i >= Vector_size(meters))
      return;

   Meter* meter = (Meter*) Vector_get(meters, i);
   Meter_setMode(meter, mode);
}

Meter* Header_addMeterByClass(Header* this, const MeterClass* type, int param, int column) {
   Vector* meters = this->columns[column];

   Meter* meter = Meter_new(this->pl, param, type);
   Vector_add(meters, meter);
   return meter;
}

int Header_size(const Header* this, int column) {
   const Vector* meters = this->columns[column];
   return Vector_size(meters);
}

MeterModeId Header_readMeterMode(const Header* this, int i, int column) {
   const Vector* meters = this->columns[column];

   const Meter* meter = (const Meter*) Vector_get(meters, i);
   return meter->mode;
}

void Header_reinit(Header* this) {
   Header_forEachColumn(this, col) {
      for (int i = 0; i < Vector_size(this->columns[col]); i++) {
         Meter* meter = (Meter*) Vector_get(this->columns[col], i);
         if (Meter_initFn(meter)) {
            Meter_init(meter);
         }
      }
   }
}

void Header_draw(const Header* this) {
   int height = this->height;
   int pad = this->pad;
   attrset(CRT_colors[RESET_COLOR]);
   for (int y = 0; y < height; y++) {
      mvhline(y, 0, ' ', COLS);
   }
   int width = COLS / this->nrColumns - (pad * this->nrColumns - 1) - 1;
   int x = pad;

   Header_forEachColumn(this, col) {
      Vector* meters = this->columns[col];
      for (int y = (pad / 2), i = 0; i < Vector_size(meters); i++) {
         Meter* meter = (Meter*) Vector_get(meters, i);
         meter->draw(meter, x, y, width);
         y += meter->h;
      }
      x += width + pad;
   }
}

int Header_calculateHeight(Header* this) {
   int pad = this->settings->headerMargin ? 2 : 0;
   int maxHeight = pad;

   Header_forEachColumn(this, col) {
      const Vector* meters = this->columns[col];
      int height = pad;
      for (int i = 0; i < Vector_size(meters); i++) {
         const Meter* meter = (const Meter*) Vector_get(meters, i);
         height += meter->h;
      }
      maxHeight = MAXIMUM(maxHeight, height);
   }
   this->height = maxHeight;
   this->pad = pad;
   return maxHeight;
}
