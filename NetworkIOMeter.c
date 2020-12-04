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

static unsigned long int cached_rxb_diff = 0;
static unsigned long int cached_rxp_diff = 0;
static unsigned long int cached_txb_diff = 0;
static unsigned long int cached_txp_diff = 0;

static void NetworkIOMeter_updateValues(ATTR_UNUSED Meter* this, char* buffer, size_t len) {
   static unsigned long long int cached_last_update = 0;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   unsigned long long int timeInMilliSeconds = (unsigned long long int)tv.tv_sec * 1000 + (unsigned long long int)tv.tv_usec / 1000;
   unsigned long long int passedTimeInMs = timeInMilliSeconds - cached_last_update;

   /* update only every 500ms */
   if (passedTimeInMs > 500) {
      static unsigned long int cached_rxb_total = 0;
      static unsigned long int cached_rxp_total = 0;
      static unsigned long int cached_txb_total = 0;
      static unsigned long int cached_txp_total = 0;

      cached_last_update = timeInMilliSeconds;

      unsigned long int bytesReceived, packetsReceived, bytesTransmitted, packetsTransmitted;

      hasData = Platform_getNetworkIO(&bytesReceived, &packetsReceived, &bytesTransmitted, &packetsTransmitted);
      if (!hasData) {
         xSnprintf(buffer, len, "no data");
         return;
      }

      if (bytesReceived > cached_rxb_total) {
         cached_rxb_diff = (bytesReceived - cached_rxb_total) / 1024; /* Meter_humanUnit() expects unit in kilo */
         cached_rxb_diff = 1000.0 * cached_rxb_diff / passedTimeInMs; /* convert to per second */
      } else {
         cached_rxb_diff = 0;
      }
      cached_rxb_total = bytesReceived;

      if (packetsReceived > cached_rxp_total) {
         cached_rxp_diff = packetsReceived - cached_rxp_total;
      } else {
         cached_rxp_diff = 0;
      }
      cached_rxp_total = packetsReceived;

      if (bytesTransmitted > cached_txb_total) {
         cached_txb_diff = (bytesTransmitted - cached_txb_total) / 1024; /* Meter_humanUnit() expects unit in kilo */
         cached_txb_diff = 1000.0 * cached_txb_diff / passedTimeInMs; /* convert to per second */
      } else {
         cached_txb_diff = 0;
      }
      cached_txb_total = bytesTransmitted;

      if (packetsTransmitted > cached_txp_total) {
         cached_txp_diff = packetsTransmitted - cached_txp_total;
      } else {
         cached_txp_diff = 0;
      }
      cached_txp_total = packetsTransmitted;
   }

   char bufferBytesReceived[12], bufferBytesTransmitted[12];
   Meter_humanUnit(bufferBytesReceived, cached_rxb_diff, sizeof(bufferBytesReceived));
   Meter_humanUnit(bufferBytesTransmitted, cached_txb_diff, sizeof(bufferBytesTransmitted));
   xSnprintf(buffer, len, "rx:%siB/s tx:%siB/s", bufferBytesReceived, bufferBytesTransmitted);
}

static void NetworkIOMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   if (!hasData) {
      RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "no data");
      return;
   }

   char buffer[64];

   RichString_writeAscii(out, CRT_colors[METER_TEXT], "rx: ");
   Meter_humanUnit(buffer, cached_rxb_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], buffer);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " tx: ");
   Meter_humanUnit(buffer, cached_txb_diff, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], buffer);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "iB/s");

   xSnprintf(buffer, sizeof(buffer), " (%lu/%lu packets) ", cached_rxp_diff, cached_txp_diff);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], buffer);
}

const MeterClass NetworkIOMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = NetworkIOMeter_display
   },
   .updateValues = NetworkIOMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = NetworkIOMeter_attributes,
   .name = "NetworkIO",
   .uiName = "Network IO",
   .caption = "Network: "
};
