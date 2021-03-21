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


Header* Header_new(ProcessList* pl, Settings* settings, int nrColumns) {
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
            xAsprintf(&name, "%s(%u)", As_Meter(meter)->name, meter->param);
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
   unsigned int param = 0;
   if (paren) {
      int ok = sscanf(paren, "(%10u)", &param);
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

Meter* Header_addMeterByClass(Header* this, const MeterClass* type, unsigned int param, int column) {
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
   const int height = this->height;
   const int pad = this->pad;
   attrset(CRT_colors[RESET_COLOR]);
   for (int y = 0; y < height; y++) {
      mvhline(y, 0, ' ', COLS);
   }
   const int width = COLS / this->nrColumns - pad;
   int x = pad;

   Header_forEachColumn(this, col) {
      Vector* meters = this->columns[col];
      for (int y = (pad / 2), i = 0; i < Vector_size(meters); i++) {
         Meter* meter = (Meter*) Vector_get(meters, i);

         int actualWidth;
         if (meter->mode == TEXT_METERMODE)
            actualWidth = meter->columnWidthCount * width + (meter->columnWidthCount - 1) * (2 * pad + 1);
         else
            actualWidth = width;

         meter->draw(meter, x, y, actualWidth);
         y += meter->h;
      }
      x += width + pad;
   }
}

void Header_updateData(Header* this) {
   Header_forEachColumn(this, col) {
      Vector* meters = this->columns[col];
      int items = Vector_size(meters);
      for (int i = 0; i < items; i++) {
         Meter* meter = (Meter*) Vector_get(meters, i);
         Meter_updateValues(meter);
      }
   }
}

/*
 * Calculate how many columns the current meter is allowed to span,
 * by counting how many columns to the right are empty or contain a BlankMeter.
 * Returns the number of columns to span, i.e. if the direct neighbor is occupied 1.
 */
static int calcColumnWidthCount(const Header* this, const Meter* curMeter, const int pad, const int curColumn, const int curHeight) {
   for (int i = curColumn + 1; i < this->nrColumns; i++) {
      const Vector* meters = this->columns[i];

      int height = pad;
      for (int j = 0; j < Vector_size(meters); j++) {
         const Meter* meter = (const Meter*) Vector_get(meters, j);

         if (height >= curHeight + curMeter->h)
            break;

         height += meter->h;
         if (height <= curHeight)
            continue;

         if (!Object_isA((const Object*) meter, (const ObjectClass*) &BlankMeter_class))
            return i - curColumn;
      }
   }

   return this->nrColumns - curColumn;
}

int Header_calculateHeight(Header* this) {
   const int pad = this->settings->headerMargin ? 2 : 0;
   int maxHeight = pad;

   Header_forEachColumn(this, col) {
      const Vector* meters = this->columns[col];
      int height = pad;
      for (int i = 0; i < Vector_size(meters); i++) {
         Meter* meter = (Meter*) Vector_get(meters, i);
         meter->columnWidthCount = calcColumnWidthCount(this, meter, pad, col, height);
         height += meter->h;
      }
      maxHeight = MAXIMUM(maxHeight, height);
   }
   this->height = maxHeight;
   this->pad = pad;
   return maxHeight;
}
