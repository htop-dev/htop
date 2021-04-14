#include "NetworkIOMeter.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


static const int NetworkIOMeter_attributes[] = {
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

static bool hasData = false;

static uint32_t cached_rxb_diff;
static uint32_t cached_rxp_diff;
static uint32_t cached_txb_diff;
static uint32_t cached_txp_diff;

static void NetworkIOMeter_updateValues(Meter* this) {
   const ProcessList* pl = this->pl;
   static uint64_t cached_last_update = 0;

   uint64_t passedTimeInMs = pl->realtimeMs - cached_last_update;

   /* update only every 500ms */
   if (passedTimeInMs > 500) {
      static uint64_t cached_rxb_total;
      static uint64_t cached_rxp_total;
      static uint64_t cached_txb_total;
      static uint64_t cached_txp_total;
      uint64_t diff;

      cached_last_update = pl->realtimeMs;

      NetworkIOData data;
      hasData = Platform_getNetworkIO(&data);
      if (!hasData) {
         xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "no data");
         return;
      }

      if (data.bytesReceived > cached_rxb_total) {
         diff = data.bytesReceived - cached_rxb_total;
         diff /= ONE_K; /* Meter_humanUnit() expects unit in kilo */
         diff = (1000 * diff) / passedTimeInMs; /* convert to per second */
         cached_rxb_diff = (uint32_t)diff;
      } else {
         cached_rxb_diff = 0;
      }
      cached_rxb_total = data.bytesReceived;

      if (data.packetsReceived > cached_rxp_total) {
         diff = data.packetsReceived - cached_rxp_total;
         cached_rxp_diff = (uint32_t)diff;
      } else {
         cached_rxp_diff = 0;
      }
      cached_rxp_total = data.packetsReceived;

      if (data.bytesTransmitted > cached_txb_total) {
         diff = data.bytesTransmitted - cached_txb_total;
         diff /= ONE_K; /* Meter_humanUnit() expects unit in kilo */
         diff = (1000 * diff) / passedTimeInMs; /* convert to per second */
         cached_txb_diff = (uint32_t)diff;
      } else {
         cached_txb_diff = 0;
      }
      cached_txb_total = data.bytesTransmitted;

      if (data.packetsTransmitted > cached_txp_total) {
         diff = data.packetsTransmitted - cached_txp_total;
         cached_txp_diff = (uint32_t)diff;
      } else {
         cached_txp_diff = 0;
      }
      cached_txp_total = data.packetsTransmitted;
   }

   this->values[0] = cached_rxb_diff;
   this->values[1] = cached_txb_diff;
   if (cached_rxb_diff + cached_txb_diff > this->total) {
      this->total = cached_rxb_diff + cached_txb_diff;
   }

   char bufferBytesReceived[12], bufferBytesTransmitted[12];
   Meter_humanUnit(bufferBytesReceived, cached_rxb_diff, sizeof(bufferBytesReceived));
   Meter_humanUnit(bufferBytesTransmitted, cached_txb_diff, sizeof(bufferBytesTransmitted));
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "rx:%siB/s tx:%siB/s", bufferBytesReceived, bufferBytesTransmitted);
}

static void NetworkIOMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   if (!hasData) {
      RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "no data");
      return;
   }

   char buffer[64];
   int len;

   RichString_writeAscii(out, CRT_colors[METER_TEXT], "rx: ");
   Meter_humanUnit(buffer, cached_rxb_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], buffer);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " tx: ");
   Meter_humanUnit(buffer, cached_txb_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], buffer);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "iB/s");

   len = xSnprintf(buffer, sizeof(buffer), " (%u/%u packets) ", cached_rxp_diff, cached_txp_diff);
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
   .caption = "Network: "
};
