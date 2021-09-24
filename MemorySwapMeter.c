/*
htop - MemorySwapMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "MemorySwapMeter.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "Macros.h"
#include "MemoryMeter.h"
#include "Object.h"
#include "SwapMeter.h"
#include "XUtils.h"


typedef struct MemorySwapMeterData_ {
   Meter* memoryMeter;
   Meter* swapMeter;
} MemorySwapMeterData;

static void MemorySwapMeter_updateValues(Meter* this) {
   MemorySwapMeterData* data = this->meterData;

   Meter_updateValues(data->memoryMeter);
   Meter_updateValues(data->swapMeter);
}

static void MemorySwapMeter_draw(Meter* this, int x, int y, int w) {
   MemorySwapMeterData* data = this->meterData;

   /* Use the same width for each sub meter to align with CPU meter */
   const int colwidth = w / 2;
   const int diff = w - colwidth * 2;

   assert(data->memoryMeter->draw);
   data->memoryMeter->draw(data->memoryMeter, x, y, colwidth);
   assert(data->swapMeter->draw);
   data->swapMeter->draw(data->swapMeter, x + colwidth + diff, y, colwidth);
}

static void MemorySwapMeter_init(Meter* this) {
   MemorySwapMeterData* data = this->meterData;

   if (!data) {
      data = this->meterData = xMalloc(sizeof(MemorySwapMeterData));
      data->memoryMeter = NULL;
      data->swapMeter = NULL;
   }

   if (!data->memoryMeter)
      data->memoryMeter = Meter_new(this->pl, 0, (const MeterClass*) Class(MemoryMeter));
   if (!data->swapMeter)
      data->swapMeter = Meter_new(this->pl, 0, (const MeterClass*) Class(SwapMeter));

   if (Meter_initFn(data->memoryMeter))
      Meter_init(data->memoryMeter);
   if (Meter_initFn(data->swapMeter))
      Meter_init(data->swapMeter);

   if (this->mode == 0)
      this->mode = BAR_METERMODE;

   this->h = MAXIMUM(Meter_modes[data->memoryMeter->mode]->h, Meter_modes[data->swapMeter->mode]->h);
}

static void MemorySwapMeter_updateMode(Meter* this, int mode) {
   MemorySwapMeterData* data = this->meterData;

   this->mode = mode;

   Meter_setMode(data->memoryMeter, mode);
   Meter_setMode(data->swapMeter, mode);

   this->h = MAXIMUM(Meter_modes[data->memoryMeter->mode]->h, Meter_modes[data->swapMeter->mode]->h);
}

static void MemorySwapMeter_done(Meter* this) {
   MemorySwapMeterData* data = this->meterData;

   Meter_delete((Object*)data->swapMeter);
   Meter_delete((Object*)data->memoryMeter);

   free(data);
}

const MeterClass MemorySwapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
   },
   .updateValues = MemorySwapMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .name = "MemorySwap",
   .uiName = "Memory & Swap",
   .description = "Combined memory and swap usage",
   .caption = "M&S",
   .draw = MemorySwapMeter_draw,
   .init = MemorySwapMeter_init,
   .updateMode = MemorySwapMeter_updateMode,
   .done = MemorySwapMeter_done
};
