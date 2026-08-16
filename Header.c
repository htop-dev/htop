/*
htop - Header.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Header.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "CPUMeter.h"
#include "DynamicMeter.h"
#include "HeaderLayout.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "Settings.h"
#include "Vector.h"
#include "XUtils.h"


Header* Header_new(Machine* host, HeaderLayout hLayout) {
   Header* this = xCalloc(1, sizeof(Header));
   this->columns = xMallocArray(HeaderLayout_getColumns(hLayout), sizeof(Vector*));
   this->maxColumns = HeaderLayout_getColumns(hLayout);
   this->headerLayout = hLayout;
   this->host = host;

   Header_forEachColumn(this, i) {
      this->columns[i] = Vector_new(Class(Meter), true, VECTOR_DEFAULT_SIZE);
   }

   return this;
}

void Header_delete(Header* this) {
   for (size_t i = 0; i < this->maxColumns; i++) {
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

   if (newColumns > this->maxColumns) {
      this->columns = xReallocArray(this->columns, newColumns, sizeof(Vector*));
      for (size_t i = this->maxColumns; i < newColumns; i++)
         this->columns[i] = Vector_new(Class(Meter), true, VECTOR_DEFAULT_SIZE);
      this->maxColumns = newColumns;
   }

   Header_calculateHeight(this);
}

void Header_undoMetersCopy(Header* this) {
   size_t currentColumns = HeaderLayout_getColumns(this->headerLayout);
   size_t maxColumns = this->maxColumns;

   size_t duplicateMeters = 0;
   for (size_t i = currentColumns; i < maxColumns; i++) {
      duplicateMeters += Vector_size(this->columns[i]);
   }

   Vector* lastColumn = this->columns[currentColumns - 1];
   lastColumn->owner = false;

   for (size_t j = 0; j < duplicateMeters; j++) {
      Vector_remove(lastColumn, Vector_size(lastColumn) - 1);
   }
   lastColumn->owner = true;
}

void Header_collapseLayout(Header* this) {
   size_t currentColumns = HeaderLayout_getColumns(this->headerLayout);
   size_t maxColumns = this->maxColumns;

   for (size_t i = currentColumns; i < maxColumns; i++) {
      Vector* column = this->columns[i];
      column->owner = false;
      Vector_delete(column);
   }
   this->columns = xReallocArray(this->columns, currentColumns, sizeof(Vector*));
   this->maxColumns = currentColumns;
}

static void Header_addMeterByName(Header* this, const char* name, MeterModeId mode, size_t column) {
   assert(column < HeaderLayout_getColumns(this->headerLayout));

   Vector* meters = this->columns[column];

   const char* paren = strchr(name, '(');
   unsigned int param = 0;
   size_t nameLen;
   if (paren) {
      if (sscanf(paren, "(%10u)", &param) != 1) { // not CPUMeter
         char dynamic[32] = {0};
         if (sscanf(paren, "(%30s)", dynamic) == 1) { // DynamicMeter
            char* end;
            if ((end = strrchr(dynamic, ')')) == NULL)
               return;    // htoprc parse failure
            *end = '\0';
            const Settings* settings = this->host->settings;
            if (!DynamicMeter_search(settings->dynamicMeters, dynamic, &param))
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
         Meter* meter = Meter_new(this->host, param, *type);
         if (mode != 0) {
            Meter_setMode(meter, mode);
         }
         Vector_add(meters, meter);
         break;
      }
   }
}

void Header_populateFromSettings(Header* this) {
   const Settings* settings = this->host->settings;
   Header_setLayout(this, settings->hLayout);

   Header_forEachColumn(this, col) {
      const MeterColumnSetting* colSettings = &settings->hColumns[col];
      Vector_prune(this->columns[col]);
      for (size_t i = 0; i < colSettings->len; i++) {
         Header_addMeterByName(this, colSettings->names[i], colSettings->modes[i], col);
      }
   }

   Header_calculateHeight(this);
}

void Header_writeBackToSettings(const Header* this) {
   Settings* settings = this->host->settings;
   Settings_setHeaderLayout(settings, this->headerLayout);
   const size_t numCols = HeaderLayout_getColumns(this->headerLayout);

   Header_forEachColumn(this, col) {
      MeterColumnSetting* colSettings = &settings->hColumns[col];

      if (colSettings->names) {
         for (size_t j = 0; j < colSettings->len; j++)
            free(colSettings->names[j]);
         free(colSettings->names);
      }
      free(colSettings->modes);

      size_t saveCol = col;
      int len = 0;
      do {
         len += Vector_size(this->columns[saveCol++]);
      } while ((col == numCols - 1) && (saveCol < this->maxColumns) && !this->metersCopied);

      colSettings->names = len ? xCalloc(len + 1, sizeof(*colSettings->names)) : NULL;
      colSettings->modes = len ? xCalloc(len, sizeof(*colSettings->modes)) : NULL;
      colSettings->len = len;

      int idx = 0;
      saveCol = col;
      if (!len)
         continue;
      do {
         const Vector* vec = this->columns[saveCol++];
         for (int i = 0; i < Vector_size(vec); i++) {
            const Meter* meter = (Meter*) Vector_get(vec, i);
            char* name = NULL;
            if (meter->param && As_Meter(meter) == &DynamicMeter_class) {
               const char* dynamic = DynamicMeter_lookup(settings->dynamicMeters, meter->param);
               xAsprintf(&name, "%s(%s)", As_Meter(meter)->name, dynamic);
            } else if (meter->param && As_Meter(meter) == &CPUMeter_class) {
               xAsprintf(&name, "%s(%u)", As_Meter(meter)->name, meter->param);
            } else {
               xAsprintf(&name, "%s", As_Meter(meter)->name);
            }
            colSettings->names[idx] = name;
            colSettings->modes[idx] = meter->mode;
            idx++;
         }
      } while ((col == numCols - 1) && (saveCol < this->maxColumns) && !this->metersCopied);
   }
}

Meter* Header_addMeterByClass(Header* this, const MeterClass* type, unsigned int param, size_t column) {
   assert(column < HeaderLayout_getColumns(this->headerLayout));

   Vector* meters = this->columns[column];

   Meter* meter = Meter_new(this->host, param, type);
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
   const size_t numCols = HeaderLayout_getColumns(this->headerLayout);
   const int width = COLS - 2 * pad - ((int)numCols - 1);
   int x = pad;
   float roundingLoss = 0.0F;

   Header_forEachColumn(this, col) {
      size_t drawCol = col;
      float colWidth = (float)width * HeaderLayout_layouts[this->headerLayout].widths[col] / 100.0F;

      roundingLoss += colWidth - floorf(colWidth);
      if (roundingLoss >= 1.0F) {
         colWidth += 1.0F;
         roundingLoss -= 1.0F;
      }

      int y = pad / 2;

      do {
         Vector* meters = this->columns[drawCol++];
         for (int i = 0; i < Vector_size(meters); i++) {
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
      } while ((col == numCols - 1) && (drawCol < this->maxColumns) && !this->metersCopied);

      x += floorf(colWidth);
      x++; /* separator column */
   }
}

void Header_updateData(Header* this) {
   const size_t numCols = HeaderLayout_getColumns(this->headerLayout);
   Header_forEachColumn(this, col) {
      size_t updCol = col;
      do {
         Vector* meters = this->columns[updCol++];
         for (int i = 0; i < Vector_size(meters); i++) {
            Meter* meter = (Meter*) Vector_get(meters, i);
            Meter_updateValues(meter);
         }
      } while ((col == numCols - 1) && (updCol < this->maxColumns) && !this->metersCopied);
   }
}

/*
 * Calculate how many columns the current meter is allowed to span,
 * by counting how many columns to the right are empty or contain a BlankMeter.
 * Returns the number of columns to span, i.e. if the direct neighbor is occupied 1.
 */
static int calcColumnWidthCount(const Header* this, const Meter* curMeter, const int pad, const size_t curColumn, const int curHeight) {
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
            return (int)(i - curColumn);
      }
   }

   return (int)(HeaderLayout_getColumns(this->headerLayout) - curColumn);
}

int Header_calculateHeight(Header* this) {
   const Settings* settings = this->host->settings;
   const int pad = settings->headerMargin ? 2 : 0;
   int maxHeight = pad;

   Header_forEachColumn(this, col) {
      size_t calcCol = col;
      int height = pad;

      do {
         const Vector* meters = this->columns[calcCol++];
         for (int i = 0; i < Vector_size(meters); i++) {
            Meter* meter = (Meter*) Vector_get(meters, i);
            meter->columnWidthCount = calcColumnWidthCount(this, meter, pad, col, height);
            height += meter->h;
         }
         maxHeight = MAXIMUM(maxHeight, height);
      } while ((col == HeaderLayout_getColumns(this->headerLayout) - 1) && (calcCol < this->maxColumns) && !this->metersCopied);
   }

   if (maxHeight == pad) {
      maxHeight = 0;
      this->pad = 0;
   } else {
      this->pad = pad;
   }

   if (settings->screenTabs) {
      maxHeight++;
   }

   this->height = maxHeight;

   return maxHeight;
}
