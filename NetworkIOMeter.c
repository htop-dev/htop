/*
htop - NetworkIOMeter.c
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "NetworkIOMeter.h"

#include <stdbool.h>

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "Row.h"
#include "XUtils.h"


static const int NetworkIOMeter_attributes[] = {
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

static MeterRateStatus status = RATESTATUS_INIT;
static double cached_rxb_diff;
static char cached_rxb_diff_str[6];
static uint32_t cached_rxp_diff;
static double cached_txb_diff;
static char cached_txb_diff_str[6];
static uint32_t cached_txp_diff;

static void NetworkIOMeter_updateValues(Meter* this) {
   const Machine* host = this->host;

   static uint64_t cached_last_update = 0;
   uint64_t passedTimeInMs = host->realtimeMs - cached_last_update;
   bool hasNewData = false;
   NetworkIOData data;

   /* update only every 500ms to have a sane span for rate calculation */
   if (passedTimeInMs > 500) {
      hasNewData = Platform_getNetworkIO(&data);
      if (!hasNewData) {
         status = RATESTATUS_NODATA;
      } else if (cached_last_update == 0) {
         status = RATESTATUS_INIT;
      } else if (passedTimeInMs > 30000) {
         status = RATESTATUS_STALE;
      } else {
         status = RATESTATUS_DATA;
      }

      cached_last_update = host->realtimeMs;
   }

   if (hasNewData) {
      static uint64_t cached_rxb_total;
      static uint64_t cached_rxp_total;
      static uint64_t cached_txb_total;
      static uint64_t cached_txp_total;

      if (status != RATESTATUS_INIT) {
         uint64_t diff;

         if (data.bytesReceived > cached_rxb_total) {
            diff = data.bytesReceived - cached_rxb_total;
            diff = (1000 * diff) / passedTimeInMs; /* convert to B/s */
            cached_rxb_diff = diff;
         } else {
            cached_rxb_diff = 0;
         }
         Meter_humanUnit(cached_rxb_diff_str, cached_rxb_diff / ONE_K, sizeof(cached_rxb_diff_str));

         if (data.packetsReceived > cached_rxp_total) {
            diff = data.packetsReceived - cached_rxp_total;
            diff = (1000 * diff) / passedTimeInMs; /* convert to pkts/s */
            cached_rxp_diff = (uint32_t)diff;
         } else {
            cached_rxp_diff = 0;
         }

         if (data.bytesTransmitted > cached_txb_total) {
            diff = data.bytesTransmitted - cached_txb_total;
            diff = (1000 * diff) / passedTimeInMs; /* convert to B/s */
            cached_txb_diff = diff;
         } else {
            cached_txb_diff = 0;
         }
         Meter_humanUnit(cached_txb_diff_str, cached_txb_diff / ONE_K, sizeof(cached_txb_diff_str));

         if (data.packetsTransmitted > cached_txp_total) {
            diff = data.packetsTransmitted - cached_txp_total;
            diff = (1000 * diff) / passedTimeInMs; /* convert to pkts/s */
            cached_txp_diff = (uint32_t)diff;
         } else {
            cached_txp_diff = 0;
         }
      }

      cached_rxb_total = data.bytesReceived;
      cached_rxp_total = data.packetsReceived;
      cached_txb_total = data.bytesTransmitted;
      cached_txp_total = data.packetsTransmitted;
   }

   this->values[0] = cached_rxb_diff;
   this->values[1] = cached_txb_diff;
   if (cached_rxb_diff + cached_txb_diff > this->total) {
      this->total = cached_rxb_diff + cached_txb_diff;
   }

   if (status == RATESTATUS_NODATA) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "no data");
      return;
   }
   if (status == RATESTATUS_INIT) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "init");
      return;
   }
   if (status == RATESTATUS_STALE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "stale");
      return;
   }

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "rx:%siB/s tx:%siB/s %u/%upkts/s",
      cached_rxb_diff_str, cached_txb_diff_str, cached_rxp_diff, cached_txp_diff);
}

static void NetworkIOMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   switch (status) {
      case RATESTATUS_NODATA:
         RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "no data");
         return;
      case RATESTATUS_INIT:
         RichString_writeAscii(out, CRT_colors[METER_VALUE], "initializing...");
         return;
      case RATESTATUS_STALE:
         RichString_writeAscii(out, CRT_colors[METER_VALUE_WARN], "stale data");
         return;
      case RATESTATUS_DATA:
         break;
   }

   char buffer[64];

   RichString_writeAscii(out, CRT_colors[METER_TEXT], "rx: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], cached_rxb_diff_str);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " tx: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], cached_txb_diff_str);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "iB/s");

   int len = xSnprintf(buffer, sizeof(buffer), " (%u/%u pkts/s) ", cached_rxp_diff, cached_txp_diff);
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
