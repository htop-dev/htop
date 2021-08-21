/*
htop - MemorySwapZramMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "MemorySwapZramMeter.h"

#include <math.h>
#include <stddef.h>

#include "CRT.h"
#include "MemoryMeter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "SwapMeter.h"
#include "ZramMeter.h"


typedef struct MemorySwapZramMeterData_ {
   Meter* memoryMeter;
   Meter* swapMeter;
   Meter* zramMeter;
} MemorySwapZramMeterData;

static void MemorySwapZramMeter_updateValues(Meter* this) {
   MemorySwapZramMeterData* data = this->meterData;

   Meter_updateValues(data->memoryMeter);
   Meter_updateValues(data->swapMeter);
   Meter_updateValues(data->zramMeter);
}

static void MemorySwapZramMeter_draw(Meter* this, int x, int y, int w) {
   MemorySwapZramMeterData* data = this->meterData;

   assert(data->memoryMeter->draw);
   data->memoryMeter->draw(data->memoryMeter, x, y, w / 2);
   assert(data->swapMeter->draw);
   data->swapMeter->draw(data->swapMeter, x + w / 2, y, w / 4);
   assert(data->zramMeter->draw);
   data->zramMeter->draw(data->zramMeter, x + w / 2 + w / 4, y, w - w / 2 - w / 4);
}

static void MemorySwapZramMeter_init(Meter* this) {
   MemorySwapZramMeterData* data = this->meterData;

   if (!data) {
      data = this->meterData = xMalloc(sizeof(MemorySwapZramMeterData));
      data->memoryMeter = NULL;
      data->swapMeter = NULL;
      data->zramMeter = NULL;
   }

   if (!data->memoryMeter)
      data->memoryMeter = Meter_new(this->pl, 0, (const MeterClass*) Class(MemoryMeter));
   if (!data->swapMeter)
      data->swapMeter = Meter_new(this->pl, 0, (const MeterClass*) Class(SwapMeter));
   if (!data->zramMeter)
      data->zramMeter = Meter_new(this->pl, 0, (const MeterClass*) Class(ZramMeter));

   if (Meter_initFn(data->memoryMeter))
      Meter_init(data->memoryMeter);
   if (Meter_initFn(data->swapMeter))
      Meter_init(data->swapMeter);
   if (Meter_initFn(data->zramMeter))
      Meter_init(data->zramMeter);

   if (this->mode == 0)
      this->mode = BAR_METERMODE;

   this->h = MAXIMUM3(Meter_modes[data->memoryMeter->mode]->h, Meter_modes[data->swapMeter->mode]->h, Meter_modes[data->zramMeter->mode]->h);
}

static void MemorySwapZramMeter_updateMode(Meter* this, int mode) {
   MemorySwapZramMeterData* data = this->meterData;

   this->mode = mode;

   Meter_setMode(data->memoryMeter, mode);
   Meter_setMode(data->swapMeter, mode);
   Meter_setMode(data->zramMeter, mode);

   this->h = MAXIMUM3(Meter_modes[data->memoryMeter->mode]->h, Meter_modes[data->swapMeter->mode]->h, Meter_modes[data->zramMeter->mode]->h);
}

static void MemorySwapZramMeter_done(Meter* this) {
   MemorySwapZramMeterData* data = this->meterData;

   Meter_delete((Object*)data->zramMeter);
   Meter_delete((Object*)data->swapMeter);
   Meter_delete((Object*)data->memoryMeter);

   free(data);
}

const MeterClass MemorySwapZramMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
   },
   .updateValues = MemorySwapZramMeter_updateValues,
   .defaultMode = CUSTOM_METERMODE,
   .name = "MemorySwapZram",
   .uiName = "Memory & Swap & Zram",
   .description = "Combined memory, swap and zram usage",
   .caption = "MSZ",
   .draw = MemorySwapZramMeter_draw,
   .init = MemorySwapZramMeter_init,
   .updateMode = MemorySwapZramMeter_updateMode,
   .done = MemorySwapZramMeter_done
};
