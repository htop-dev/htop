/*
htop - NetworkIOMeter.c
(C) 2020-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "NetworkIOMeter.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "Row.h"
#include "XUtils.h"


/* Index into this table is param - 1. Entries are never removed, so a
   meter keeps its interface even while the interface is absent. */
static char** interfaceNames;
static unsigned int interfaceNamesCount;

bool NetworkIOMeter_isValidInterfaceName(const char* name) {
   size_t len = strlen(name);
   if (len == 0 || len > NETWORKIO_INTERFACE_NAME_MAX)
      return false;

   /* the kernel rejects '/', ':' and whitespace; parentheses and '=' would break
      htoprc, control characters the display. Non-ASCII (UTF-8) names are allowed. */
   for (const unsigned char* c = (const unsigned char*)name; *c; c++) {
      if (*c == '/' || *c == ':' || *c == '(' || *c == ')' || *c == '=' || *c <= ' ' || *c == 0x7f)
         return false;
   }

   return true;
}

unsigned int NetworkIOMeter_paramForInterface(const char* name) {
   if (!NetworkIOMeter_isValidInterfaceName(name))
      return 0;

   for (unsigned int i = 0; i < interfaceNamesCount; i++) {
      if (String_eq(interfaceNames[i], name))
         return i + 1;
   }

   /* AvailableMetersPanel keeps param in the low 16 bits of the item key */
   if (interfaceNamesCount >= 0xffff)
      return 0;

   interfaceNames = xReallocArray(interfaceNames, interfaceNamesCount + 1, sizeof(*interfaceNames));
   interfaceNames[interfaceNamesCount] = xStrdup(name);
   interfaceNamesCount++;
   return interfaceNamesCount;
}

const char* NetworkIOMeter_interfaceForParam(unsigned int param) {
   if (param == 0 || param > interfaceNamesCount)
      return NULL;

   return interfaceNames[param - 1];
}

static const int NetworkIOMeter_attributes[] = {
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
};

typedef struct NetworkIOMeterData_ {
   MeterRateStatus status;
   bool absent;
   uint64_t lastUpdate;
   NetworkIOData lastTotals;
   double rxbDiff;
   double txbDiff;
   uint32_t rxpDiff;
   uint32_t txpDiff;
   char rxbDiffStr[6];
   char txbDiffStr[6];
   uint32_t linkSpeedMbps;    /* 0 if unknown */
   bool scaledToLink;         /* Meter.total is set from the link speed */
} NetworkIOMeterData;

static void NetworkIOMeter_init(Meter* this) {
   /* Header_reinit() calls this again for existing meters: keep their state */
   if (this->meterData)
      return;

   NetworkIOMeterData* data = xCalloc(1, sizeof(NetworkIOMeterData));
   data->status = RATESTATUS_INIT;
   this->meterData = data;
}

static void NetworkIOMeter_done(Meter* this) {
   free(this->meterData);
   this->meterData = NULL;
}

static void NetworkIOMeter_getUiName(const Meter* this, char* buffer, size_t length) {
   const char* interfaceName = NetworkIOMeter_interfaceForParam(this->param);
   if (interfaceName)
      xSnprintf(buffer, length, "%s: %s", Meter_uiName(this), interfaceName);
   else
      xSnprintf(buffer, length, "%s", Meter_uiName(this));
}

static uint64_t NetworkIOMeter_rate(uint64_t current, uint64_t last, uint64_t passedTimeInMs) {
   if (current <= last)
      return 0;

   return (1000 * (current - last)) / passedTimeInMs;
}

static void NetworkIOMeter_formatSpeed(char* buffer, size_t size, uint32_t linkSpeedMbps) {
   if (linkSpeedMbps >= 1000 && linkSpeedMbps % 1000 == 0)
      xSnprintf(buffer, size, "%uGb/s", linkSpeedMbps / 1000);
   else
      xSnprintf(buffer, size, "%uMb/s", linkSpeedMbps);
}

/* share of the link speed used by a rate in bytes per second */
static unsigned int NetworkIOMeter_linkPercent(double bytesPerSecond, uint32_t linkSpeedMbps) {
   double percent = bytesPerSecond * 8.0 / (linkSpeedMbps * 1e6) * 100.0;
   return (unsigned int)lround(MINIMUM(percent, 999.0));
}

static void NetworkIOMeter_updateValues(Meter* this) {
   const Machine* host = this->host;
   NetworkIOMeterData* data = this->meterData;
   const char* interfaceName = NetworkIOMeter_interfaceForParam(this->param);

   uint64_t passedTimeInMs = host->realtimeMs - data->lastUpdate;
   bool hasNewData = false;
   NetworkIOData totals = {0};

   /* update only every 500ms to have a sane span for rate calculation */
   if (passedTimeInMs > 500) {
      if (interfaceName) {
         uint32_t linkSpeedMbps = 0;
         bool present = false;
         bool readable = Platform_getNetworkInterfaceIO(interfaceName, &totals, &linkSpeedMbps, &present);
         hasNewData = readable && present;
         /* "absent" only when the interface list could be read without it */
         data->absent = readable && !present;
         data->linkSpeedMbps = linkSpeedMbps;
      } else {
         hasNewData = Platform_getNetworkIO(&totals);
      }

      if (!hasNewData) {
         data->status = RATESTATUS_NODATA;
         /* do not keep drawing the last known traffic */
         data->rxbDiff = 0;
         data->txbDiff = 0;
         data->rxpDiff = 0;
         data->txpDiff = 0;
      } else if (data->lastUpdate == 0) {
         data->status = RATESTATUS_INIT;
      } else if (passedTimeInMs > 30000) {
         data->status = RATESTATUS_STALE;
      } else {
         data->status = RATESTATUS_DATA;
      }

      /* an interface coming back starts over, its counters may have been reset */
      data->lastUpdate = hasNewData ? host->realtimeMs : 0;
   }

   if (hasNewData) {
      if (data->status != RATESTATUS_INIT) {
         const NetworkIOData* last = &data->lastTotals;

         data->rxbDiff = NetworkIOMeter_rate(totals.bytesReceived, last->bytesReceived, passedTimeInMs);
         data->rxpDiff = (uint32_t)NetworkIOMeter_rate(totals.packetsReceived, last->packetsReceived, passedTimeInMs);
         data->txbDiff = NetworkIOMeter_rate(totals.bytesTransmitted, last->bytesTransmitted, passedTimeInMs);
         data->txpDiff = (uint32_t)NetworkIOMeter_rate(totals.packetsTransmitted, last->packetsTransmitted, passedTimeInMs);

         Meter_humanUnit(data->rxbDiffStr, data->rxbDiff / ONE_K, sizeof(data->rxbDiffStr));
         Meter_humanUnit(data->txbDiffStr, data->txbDiff / ONE_K, sizeof(data->txbDiffStr));
      }

      data->lastTotals = totals;
   }

   this->values[0] = data->rxbDiff;
   this->values[1] = data->txbDiff;

   /* full duplex: rx and tx may both reach the link speed */
   if (data->linkSpeedMbps) {
      this->total = 2.0 * data->linkSpeedMbps * 1e6 / 8.0;
      data->scaledToLink = true;
   } else if (data->scaledToLink) {
      /* speed became unknown: back to automatic scaling */
      this->total = As_Meter(this)->total;
      data->scaledToLink = false;
   }

   char prefix[NETWORKIO_INTERFACE_NAME_MAX + 16] = "";
   if (interfaceName) {
      if (data->linkSpeedMbps) {
         char speed[16];
         NetworkIOMeter_formatSpeed(speed, sizeof(speed), data->linkSpeedMbps);
         xSnprintf(prefix, sizeof(prefix), "%s %s ", interfaceName, speed);
      } else {
         xSnprintf(prefix, sizeof(prefix), "%s ", interfaceName);
      }
   }

   if (data->status == RATESTATUS_NODATA) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%s", prefix, data->absent ? "absent" : "no data");
      return;
   }
   if (data->status == RATESTATUS_INIT) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%sinit", prefix);
      return;
   }
   if (data->status == RATESTATUS_STALE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%sstale", prefix);
      return;
   }

   if (data->linkSpeedMbps) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%srx:%siB/s tx:%siB/s (%u%%/%u%%)",
         prefix, data->rxbDiffStr, data->txbDiffStr,
         NetworkIOMeter_linkPercent(data->rxbDiff, data->linkSpeedMbps),
         NetworkIOMeter_linkPercent(data->txbDiff, data->linkSpeedMbps));
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%srx:%siB/s tx:%siB/s (%u/%upps)",
         prefix, data->rxbDiffStr, data->txbDiffStr, data->rxpDiff, data->txpDiff);
   }
}

static void NetworkIOMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   const NetworkIOMeterData* data = this->meterData;
   const char* interfaceName = NetworkIOMeter_interfaceForParam(this->param);

   char buffer[64];
   int len;

   if (interfaceName) {
      RichString_appendWide(out, CRT_colors[METER_TEXT], interfaceName);
      if (data->linkSpeedMbps) {
         NetworkIOMeter_formatSpeed(buffer, sizeof(buffer), data->linkSpeedMbps);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], " ");
         RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " ");
   }

   switch (data->status) {
      case RATESTATUS_NODATA:
         RichString_appendAscii(out, CRT_colors[METER_VALUE_ERROR], data->absent ? "absent" : "no data");
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

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "rx: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], data->rxbDiffStr);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " tx: ");
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], data->txbDiffStr);
   RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "iB/s");

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " (");
   if (data->linkSpeedMbps) {
      len = xSnprintf(buffer, sizeof(buffer), "%u%%", NetworkIOMeter_linkPercent(data->rxbDiff, data->linkSpeedMbps));
      RichString_appendnAscii(out, CRT_colors[METER_VALUE_IOREAD], buffer, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "/");
      len = xSnprintf(buffer, sizeof(buffer), "%u%%", NetworkIOMeter_linkPercent(data->txbDiff, data->linkSpeedMbps));
      RichString_appendnAscii(out, CRT_colors[METER_VALUE_IOWRITE], buffer, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], ")");
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%u", (unsigned int)data->rxpDiff);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE_IOREAD], buffer, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "/");
      len = xSnprintf(buffer, sizeof(buffer), "%u", (unsigned int)data->txpDiff);
      RichString_appendnAscii(out, CRT_colors[METER_VALUE_IOWRITE], buffer, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], " pps)");
   }
}

const MeterClass NetworkIOMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = NetworkIOMeter_display
   },
   .init = NetworkIOMeter_init,
   .done = NetworkIOMeter_done,
   .getUiName = NetworkIOMeter_getUiName,
   .updateValues = NetworkIOMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 2,
   .isPercentChart = false,
   .total = 1.0,
   .attributes = NetworkIOMeter_attributes,
   .name = "NetworkIO",
   .uiName = "Network IO",
   .description = "Network bytes & packets received/sent per second",
   .caption = "Net: "
};
