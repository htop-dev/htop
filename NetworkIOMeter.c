#include "NetworkIOMeter.h"

#include <stdbool.h>
#include <stdint.h>

#include "CRT.h"
#include "Macros.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessList.h"
#include "RichString.h"
#include "XUtils.h"


typedef struct NetworkIOMeterData_ {
   uint64_t last_update;
   uint64_t rxb_total;
   uint64_t rxp_total;
   uint64_t txb_total;
   uint64_t txp_total;
   uint32_t rxb_diff;
   uint32_t rxp_diff;
   uint32_t txb_diff;
   uint32_t txp_diff;
   MeterRateStatus status;
} NetworkIOMeterData;

static const int NetworkIOMeter_attributes[] = {
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

static void NetworkIOMeter_init(Meter* this) {
   if (!this->meterData)
      this->meterData = xCalloc(1, sizeof(NetworkIOMeterData));
}

static void NetworkIOMeter_done(Meter* this) {
   free(this->meterData);
}

static void NetworkIOMeter_updateValues(Meter* this) {
   const ProcessList* pl = this->pl;
   NetworkIOMeterData* mdata = this->meterData;

   uint64_t passedTimeInMs = pl->realtimeMs - mdata->last_update;

   /* update only every 500ms to have a sane span for rate calculation */
   if (passedTimeInMs > 500) {
      uint64_t diff;

      NetworkIOData data;
      if (!Platform_getNetworkIO(this->curChoice, &data)) {
         mdata->status = RATESTATUS_NODATA;
      } else if (mdata->last_update == 0) {
         mdata->status = RATESTATUS_INIT;
      } else if (passedTimeInMs > 30000) {
         mdata->status = RATESTATUS_STALE;
      } else {
         mdata->status = RATESTATUS_DATA;
      }

      mdata->last_update = pl->realtimeMs;

      if (mdata->status == RATESTATUS_NODATA) {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "no data");
         return;
      }

      if (data.bytesReceived > mdata->rxb_total) {
         diff = data.bytesReceived - mdata->rxb_total;
         diff /= ONE_K; /* Meter_humanUnit() expects unit in kilo */
         diff = (1000 * diff) / passedTimeInMs; /* convert to per second */
         mdata->rxb_diff = (uint32_t)diff;
      } else {
         mdata->rxb_diff = 0;
      }
      mdata->rxb_total = data.bytesReceived;

      if (data.packetsReceived > mdata->rxp_total) {
         diff = data.packetsReceived - mdata->rxp_total;
         mdata->rxp_diff = (uint32_t)diff;
      } else {
         mdata->rxp_diff = 0;
      }
      mdata->rxp_total = data.packetsReceived;

      if (data.bytesTransmitted > mdata->txb_total) {
         diff = data.bytesTransmitted - mdata->txb_total;
         diff /= ONE_K; /* Meter_humanUnit() expects unit in kilo */
         diff = (1000 * diff) / passedTimeInMs; /* convert to per second */
         mdata->txb_diff = (uint32_t)diff;
      } else {
         mdata->txb_diff = 0;
      }
      mdata->txb_total = data.bytesTransmitted;

      if (data.packetsTransmitted > mdata->txp_total) {
         diff = data.packetsTransmitted - mdata->txp_total;
         mdata->txp_diff = (uint32_t)diff;
      } else {
         mdata->txp_diff = 0;
      }
      mdata->txp_total = data.packetsTransmitted;
   }

   if (mdata->status == RATESTATUS_INIT) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "init");
      return;
   }
   if (mdata->status == RATESTATUS_STALE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "stale");
      return;
   }

   this->values[0] = mdata->rxb_diff;
   this->values[1] = mdata->txb_diff;
   if (this->values[0] + this->values[1] > this->total) {
      this->total = this->values[0] + this->values[1];
   }

   char bufferBytesReceived[12], bufferBytesTransmitted[12];
   Meter_humanUnit(bufferBytesReceived, mdata->rxb_diff, sizeof(bufferBytesReceived));
   Meter_humanUnit(bufferBytesTransmitted, mdata->txb_diff, sizeof(bufferBytesTransmitted));
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%srx:%siB/s tx:%siB/s",
             this->curChoice ? this->curChoice : "",
             this->curChoice ? " " : "",
             bufferBytesReceived,
             bufferBytesTransmitted);
}

static void NetworkIOMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter *)cast;
   const NetworkIOMeterData* mdata = this->meterData;

   if (this->curChoice) {
      RichString_appendAscii(out, CRT_colors[METER_TEXT], this->curChoice);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " ");
   }

   switch (mdata->status) {
   case RATESTATUS_NODATA:
      RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], "no data");
      return;
   case RATESTATUS_INIT:
      RichString_appendAscii(out, CRT_colors[METER_VALUE], "initializing...");
      return;
   case RATESTATUS_STALE:
      RichString_appendAscii(out, CRT_colors[METER_VALUE_WARN], "stale data");
      return;
   case RATESTATUS_DATA:
      break;
   }

   char buffer[64];
   int len;

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "rx: ");
   Meter_humanUnit(buffer, mdata->rxb_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], buffer);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " tx: ");
   Meter_humanUnit(buffer, mdata->txb_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], buffer);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "iB/s");

   len = xSnprintf(buffer, sizeof(buffer), " (%u/%u packets) ", mdata->rxp_diff, mdata->txp_diff);
   RichString_appendnAscii(out, CRT_colors[METER_TEXT], buffer, len);
}

const MeterClass NetworkIOMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = NetworkIOMeter_display
   },
   .updateValues = NetworkIOMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 2,
   .total = 100.0,
   .attributes = NetworkIOMeter_attributes,
   .name = "NetworkIO",
   .uiName = "Network IO",
   .caption = "Network: ",
   .init = NetworkIOMeter_init,
   .done = NetworkIOMeter_done,
};

const MeterClass NetworkInterfaceIOMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = NetworkIOMeter_display
   },
   .updateValues = NetworkIOMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 2,
   .total = 100.0,
   .attributes = NetworkIOMeter_attributes,
   .name = "NetworkInterfaceIO",
   .uiName = "Network IF IO",
   .description = "Network IO usage of single interface",
   .caption = "Network IF: ",
   .init = NetworkIOMeter_init,
   .done = NetworkIOMeter_done,
   .getChoices = Platform_getLocalIPv4addressChoices,
};
