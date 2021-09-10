/*
htop - Header.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "CPUMeter.h"
#include "DynamicMeter.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "XUtils.h"


Header* Header_new(ProcessList* pl, Settings* settings, HeaderLayout hLayout) {
   Header* this = xCalloc(1, sizeof(Header));
   this->columns = xMallocArray(HeaderLayout_getColumns(hLayout), sizeof(Vector*));
   this->settings = settings;
   this->pl = pl;
   this->headerLayout = hLayout;

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

void Header_setLayout(Header* this, HeaderLayout hLayout) {
   size_t oldColumns = HeaderLayout_getColumns(this->headerLayout);
   size_t newColumns = HeaderLayout_getColumns(hLayout);

   this->headerLayout = hLayout;

   if (newColumns == oldColumns)
      return;

   if (newColumns > oldColumns) {
      this->columns = xReallocArray(this->columns, newColumns, sizeof(Vector*));
      for (size_t i = oldColumns; i < newColumns; i++)
         this->columns[i] = Vector_new(Class(Meter), true, DEFAULT_SIZE);
   } else {
      // move meters from to-be-deleted columns into last one
      for (size_t i = newColumns; i < oldColumns; i++) {
         for (int j = this->columns[i]->items - 1; j >= 0; j--) {
            Vector_add(this->columns[newColumns - 1], Vector_take(this->columns[i], j));
         }
         Vector_delete(this->columns[i]);
      }
      this->columns = xReallocArray(this->columns, newColumns, sizeof(Vector*));
   }

   Header_calculateHeight(this);
}

static void Header_addMeterByName(Header* this, const char* name, MeterModeId mode, unsigned int column) {
   assert(column < HeaderLayout_getColumns(this->headerLayout));

   Vector* meters = this->columns[column];

   const char* paren = strchr(name, '(');
   unsigned int param = 0;
   size_t nameLen;
   if (paren) {
      int ok = sscanf(paren, "(%10u)", &param); // CPUMeter
      if (!ok) {
         char dynamic[32] = {0};
         if (sscanf(paren, "(%30s)", dynamic)) { // DynamicMeter
            char* end;
            if ((end = strrchr(dynamic, ')')) == NULL)
               return;    // htoprc parse failure
            *end = '\0';
            if (!DynamicMeter_search(this->pl->dynamicMeters, dynamic, &param))
               return;    // name lookup failure
         } else {
            param = 0;
         }
      }
      nameLen = paren - name;
   } else {
      nameLen = strlen(name);
   }

   for (const MeterClass* const* type = Platform_meterTypes; *type; type++) {
      if (0 == strncmp(name, (*type)->name, nameLen) && (*type)->name[nameLen] == '\0') {
         Meter* meter = Meter_new(this->pl, param, *type);
         if (mode != 0) {
            Meter_setMode(meter, mode);
         }
         Vector_add(meters, meter);
         break;
      }
   }
}

void Header_populateFromSettings(Header* this) {
   Header_setLayout(this, this->settings->hLayout);

   Header_forEachColumn(this, col) {
      const MeterColumnSetting* colSettings = &this->settings->hColumns[col];
      Vector_prune(this->columns[col]);
      for (size_t i = 0; i < colSettings->len; i++) {
         Header_addMeterByName(this, colSettings->names[i], colSettings->modes[i], col);
      }
   }

   Header_calculateHeight(this);
}

void Header_writeBackToSettings(const Header* this) {
   Settings_setHeaderLayout(this->settings, this->headerLayout);

   Header_forEachColumn(this, col) {
      MeterColumnSetting* colSettings = &this->settings->hColumns[col];

      if (colSettings->names) {
         for (size_t j = 0; j < colSettings->len; j++)
            free(colSettings->names[j]);
         free(colSettings->names);
      }
      free(colSettings->modes);

      const Vector* vec = this->columns[col];
      int len = Vector_size(vec);

      colSettings->names = len ? xCalloc(len + 1, sizeof(char*)) : NULL;
      colSettings->modes = len ? xCalloc(len, sizeof(int)) : NULL;
      colSettings->len = len;

      for (int i = 0; i < len; i++) {
         const Meter* meter = (Meter*) Vector_get(vec, i);
         char* name;
         if (meter->param && As_Meter(meter) == &DynamicMeter_class) {
            const char* dynamic = DynamicMeter_lookup(this->pl->dynamicMeters, meter->param);
            xAsprintf(&name, "%s(%s)", As_Meter(meter)->name, dynamic);
         } else if (meter->param && As_Meter(meter) == &CPUMeter_class) {
            xAsprintf(&name, "%s(%u)", As_Meter(meter)->name, meter->param);
         } else {
            xAsprintf(&name, "%s", As_Meter(meter)->name);
         }
         colSettings->names[i] = name;
         colSettings->modes[i] = meter->mode;
      }
   }
}

Meter* Header_addMeterByClass(Header* this, const MeterClass* type, unsigned int param, unsigned int column) {
   assert(column < HeaderLayout_getColumns(this->headerLayout));

   Vector* meters = this->columns[column];

   Meter* meter = Meter_new(this->pl, param, type);
   Vector_add(meters, meter);
   return meter;
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
   const int numCols = HeaderLayout_getColumns(this->headerLayout);
   const int width = COLS - 2 * pad - (numCols - 1);
   int x = pad;
   float roundingLoss = 0.0F;

   Header_forEachColumn(this, col) {
      Vector* meters = this->columns[col];
      float colWidth = (float)width * HeaderLayout_layouts[this->headerLayout].widths[col] / 100.0F;

      roundingLoss += colWidth - floorf(colWidth);
      if (roundingLoss >= 1.0F) {
         colWidth += 1.0F;
         roundingLoss -= 1.0F;
      }

      for (int y = (pad / 2), i = 0; i < Vector_size(meters); i++) {
         Meter* meter = (Meter*) Vector_get(meters, i);

         float actualWidth = colWidth;

         /* Let meters in text mode expand to the right on empty neighbors;
            except for multi column meters. */
         if (meter->mode == TEXT_METERMODE && !Meter_isMultiColumn(meter)) {
            for (int j = 1; j < meter->columnWidthCount; j++) {
               actualWidth++; /* separator column */
               actualWidth += (float)width * HeaderLayout_layouts[this->headerLayout].widths[col + j] / 100.0F;
            }
         }

         assert(meter->draw);
         meter->draw(meter, x, y, floorf(actualWidth));
         y += meter->h;
      }

      x += floorf(colWidth);
      x++; /* separator column */
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
static int calcColumnWidthCount(const Header* this, const Meter* curMeter, const int pad, const unsigned int curColumn, const int curHeight) {
   for (size_t i = curColumn + 1; i < HeaderLayout_getColumns(this->headerLayout); i++) {
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

   return HeaderLayout_getColumns(this->headerLayout) - curColumn;
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
   if (this->settings->screenTabs) {
      maxHeight++;
   }
   this->height = maxHeight;
   this->pad = pad;
   return maxHeight;
}
