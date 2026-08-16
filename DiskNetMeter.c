/*
htop - DiskNetMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "DiskNetMeter.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "Macros.h"
#include "DiskIOMeter.h"
#include "Object.h"
#include "NetworkIOMeter.h"
#include "XUtils.h"


typedef struct DiskNetMeterData_ {
   Meter* diskMeter;
   Meter* netMeter;
} DiskNetMeterData;

static void DiskNetMeter_updateValues(Meter* this) {
   DiskNetMeterData* data = this->meterData;

   Meter_updateValues(data->diskMeter);
   Meter_updateValues(data->netMeter);
}

static void DiskNetMeter_draw(Meter* this, int x, int y, int w) {
   DiskNetMeterData* data = this->meterData;

   /* Use the same width for each sub meter to align with CPU meter */
   const int colwidth = w / 2;
   const int diff = w - colwidth * 2;

   assert(data->diskMeter->draw);
   data->diskMeter->draw(data->diskMeter, x, y, colwidth);
   assert(data->netMeter->draw);
   data->netMeter->draw(data->netMeter, x + colwidth + diff, y, colwidth);
}

static void DiskNetMeter_init(Meter* this) {
   DiskNetMeterData* data = this->meterData;

   if (!data) {
      data = this->meterData = xMalloc(sizeof(DiskNetMeterData));
      data->diskMeter = NULL;
      data->netMeter = NULL;
   }

   if (!data->diskMeter)
      data->diskMeter = Meter_new(this->pl, 0, (const MeterClass*) Class(DiskIOMeter));
   if (!data->netMeter)
      data->netMeter = Meter_new(this->pl, 0, (const MeterClass*) Class(NetworkIOMeter));

   if (Meter_initFn(data->diskMeter))
      Meter_init(data->diskMeter);
   if (Meter_initFn(data->netMeter))
      Meter_init(data->netMeter);

   if (this->mode == 0)
      this->mode = BAR_METERMODE;

   this->h = MAXIMUM(Meter_modes[data->diskMeter->mode]->h, Meter_modes[data->netMeter->mode]->h);
}

static void DiskNetMeter_updateMode(Meter* this, int mode) {
   DiskNetMeterData* data = this->meterData;

   this->mode = mode;

   Meter_setMode(data->diskMeter, mode);
   Meter_setMode(data->netMeter, mode);

   this->h = MAXIMUM(Meter_modes[data->diskMeter->mode]->h, Meter_modes[data->netMeter->mode]->h);
}

static void DiskNetMeter_done(Meter* this) {
   DiskNetMeterData* data = this->meterData;

   Meter_delete((Object*)data->netMeter);
   Meter_delete((Object*)data->diskMeter);

   free(data);
}

const MeterClass DiskNetMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
   },
   .updateValues = DiskNetMeter_updateValues, 
   .defaultMode = CUSTOM_METERMODE,
   .isMultiColumn = true,
   .name = "DiskNet",
   .uiName = "Disk & Net",
   .description = "Combined Disk and Network IO usage",
   .caption = "D&N",
   .draw = DiskNetMeter_draw,
   .init = DiskNetMeter_init,
   .updateMode = DiskNetMeter_updateMode,
   .done = DiskNetMeter_done
};
