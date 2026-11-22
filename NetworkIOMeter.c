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
static size_t interfaceNamesCount;

static NetworkIOData networkIOData;
static NetworkInterfaceIOData* networkIOInterfaces;
static size_t networkIOInterfacesCount;
static const Machine* networkIOHost;
static uint64_t networkIOTime;
static bool networkIOValid;

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

   for (size_t i = 0; i < interfaceNamesCount; i++) {
      if (String_eq(interfaceNames[i], name))
         return (unsigned int)(i + 1);
   }

   /* AvailableMetersPanel keeps param in the low 16 bits of the item key */
   if (interfaceNamesCount >= 0xffff)
      return 0;

   interfaceNames = xReallocArray(interfaceNames, interfaceNamesCount + 1, sizeof(*interfaceNames));
   interfaceNames[interfaceNamesCount] = xStrdup(name);
   interfaceNamesCount++;
   return (unsigned int)interfaceNamesCount;
}

const char* NetworkIOMeter_interfaceForParam(size_t param) {
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
   double rxpDiff;
   double txpDiff;
   char rxbDiffStr[6];
   char txbDiffStr[6];
   char rxpDiffStr[16];
   char txpDiffStr[16];
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

static double NetworkIOMeter_rate(uint64_t current, uint64_t last, uint64_t passedTimeInMs) {
   if (passedTimeInMs == 0)
      return NAN;
   if (current <= last)
      return 0;

   return 1000.0 * (current - last) / passedTimeInMs;
}

static void NetworkIOMeter_formatNumber(char* buffer, size_t size, double number) {
   if (isNaN(number))
      xSnprintf(buffer, size, "N/A");
   else
      xSnprintf(buffer, size, "%.0f", number);
}

/* share of the link speed used by a rate in bytes per second */
static double NetworkIOMeter_linkPercent(double bytesPerSecond, uint32_t linkSpeedMbps) {
   if (isNaN(bytesPerSecond))
      return NAN;

   return MINIMUM(bytesPerSecond / (linkSpeedMbps * 1250.0), 999.0);
}

static void NetworkIOMeter_updateNetworkIO(const Machine* host) {
   if (networkIOHost == host && networkIOTime == host->realtimeMs)
      return;

   free(networkIOInterfaces);
   networkIOInterfaces = NULL;
   networkIOInterfacesCount = 0;
   memset(&networkIOData, 0, sizeof(networkIOData));

   networkIOValid = Platform_getNetworkIO(&networkIOData, &networkIOInterfaces, &networkIOInterfacesCount);
   networkIOHost = host;
   networkIOTime = host->realtimeMs;
}

static void NetworkIOMeter_updateValues(Meter* this) {
   const Machine* host = this->host;
   NetworkIOMeterData* meterData = this->meterData;
   const char* interfaceName = NetworkIOMeter_interfaceForParam(this->param);

   NetworkIOMeter_updateNetworkIO(host);

   if (meterData->lastUpdate == host->realtimeMs)
      return;

   uint64_t passedTimeInMs = host->realtimeMs - meterData->lastUpdate;
   bool hasNewData = networkIOValid;
   NetworkIOData data = {0};

   if (interfaceName) {
      hasNewData = false;
      meterData->absent = networkIOValid && networkIOInterfaces;
      meterData->linkSpeedMbps = 0;

      for (size_t i = 0; i < networkIOInterfacesCount; i++) {
         const NetworkInterfaceIOData* entry = &networkIOInterfaces[i];
         if (!String_eq(entry->name, interfaceName))
            continue;

         data = entry->data;
         meterData->linkSpeedMbps = entry->linkSpeedMbps;
         meterData->absent = false;
         hasNewData = true;
         break;
      }
   } else if (hasNewData) {
      data = networkIOData;
   }

   if (!hasNewData) {
      meterData->status = RATESTATUS_NODATA;
      /* do not keep drawing the last known traffic */
      meterData->rxbDiff = 0;
      meterData->txbDiff = 0;
      meterData->rxpDiff = 0;
      meterData->txpDiff = 0;
   } else if (meterData->lastUpdate == 0) {
      meterData->status = RATESTATUS_INIT;
   } else if (passedTimeInMs > 30000) {
      meterData->status = RATESTATUS_STALE;
   } else {
      meterData->status = RATESTATUS_DATA;
   }

   /* an interface coming back starts over, its counters may have been reset */
   meterData->lastUpdate = hasNewData ? host->realtimeMs : 0;

   if (hasNewData) {
      if (meterData->status != RATESTATUS_INIT) {
         const NetworkIOData* last = &meterData->lastTotals;

         meterData->rxbDiff = NetworkIOMeter_rate(data.bytesReceived, last->bytesReceived, passedTimeInMs);
         meterData->rxpDiff = NetworkIOMeter_rate(data.packetsReceived, last->packetsReceived, passedTimeInMs);
         meterData->txbDiff = NetworkIOMeter_rate(data.bytesTransmitted, last->bytesTransmitted, passedTimeInMs);
         meterData->txpDiff = NetworkIOMeter_rate(data.packetsTransmitted, last->packetsTransmitted, passedTimeInMs);

         if (isNaN(meterData->rxbDiff))
            xSnprintf(meterData->rxbDiffStr, sizeof(meterData->rxbDiffStr), "N/A");
         else
            Meter_humanUnit(meterData->rxbDiffStr, meterData->rxbDiff / ONE_K, sizeof(meterData->rxbDiffStr));
         if (isNaN(meterData->txbDiff))
            xSnprintf(meterData->txbDiffStr, sizeof(meterData->txbDiffStr), "N/A");
         else
            Meter_humanUnit(meterData->txbDiffStr, meterData->txbDiff / ONE_K, sizeof(meterData->txbDiffStr));
         NetworkIOMeter_formatNumber(meterData->rxpDiffStr, sizeof(meterData->rxpDiffStr), meterData->rxpDiff);
         NetworkIOMeter_formatNumber(meterData->txpDiffStr, sizeof(meterData->txpDiffStr), meterData->txpDiff);
      }

      meterData->lastTotals = data;
   }

   this->values[0] = meterData->rxbDiff;
   this->values[1] = meterData->txbDiff;

   /* full duplex: rx and tx may both reach the link speed */
   if (meterData->linkSpeedMbps) {
      this->total = 2.0 * meterData->linkSpeedMbps * 1e6 / 8.0;
      meterData->scaledToLink = true;
   } else if (meterData->scaledToLink) {
      /* speed became unknown: back to automatic scaling */
      this->total = As_Meter(this)->total;
      meterData->scaledToLink = false;
   }

   char prefix[NETWORKIO_INTERFACE_NAME_MAX + 16] = "";
   if (interfaceName) {
      if (meterData->linkSpeedMbps) {
         char speed[16];
         Meter_humanUnitBase(speed, meterData->linkSpeedMbps * 1000.0, sizeof(speed), 1000.0);
         xSnprintf(prefix, sizeof(prefix), "%s %sb/s ", interfaceName, speed);
      } else {
         xSnprintf(prefix, sizeof(prefix), "%s ", interfaceName);
      }
   }

   if (meterData->status == RATESTATUS_NODATA) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%s", prefix, meterData->absent ? "absent" : "no data");
      return;
   }
   if (meterData->status == RATESTATUS_INIT) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%sinit", prefix);
      return;
   }
   if (meterData->status == RATESTATUS_STALE) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%sstale", prefix);
      return;
   }

   if (meterData->linkSpeedMbps) {
      char rxPercent[16];
      char txPercent[16];
      NetworkIOMeter_formatNumber(rxPercent, sizeof(rxPercent), NetworkIOMeter_linkPercent(meterData->rxbDiff, meterData->linkSpeedMbps));
      NetworkIOMeter_formatNumber(txPercent, sizeof(txPercent), NetworkIOMeter_linkPercent(meterData->txbDiff, meterData->linkSpeedMbps));
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%srx:%siB/s tx:%siB/s (%s%%/%s%%)",
         prefix, meterData->rxbDiffStr, meterData->txbDiffStr, rxPercent, txPercent);
   } else {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%srx:%siB/s tx:%siB/s (%s/%spps)",
         prefix, meterData->rxbDiffStr, meterData->txbDiffStr, meterData->rxpDiffStr, meterData->txpDiffStr);
   }
}

static void NetworkIOMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   const NetworkIOMeterData* data = this->meterData;
   const char* interfaceName = NetworkIOMeter_interfaceForParam(this->param);

   char buffer[64];

   if (interfaceName) {
      RichString_appendWide(out, CRT_colors[METER_TEXT], interfaceName);
      if (data->linkSpeedMbps) {
         Meter_humanUnitBase(buffer, data->linkSpeedMbps * 1000.0, sizeof(buffer), 1000.0);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], " ");
         RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);
         RichString_appendAscii(out, CRT_colors[METER_VALUE], "b/s");
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
      NetworkIOMeter_formatNumber(buffer, sizeof(buffer), NetworkIOMeter_linkPercent(data->rxbDiff, data->linkSpeedMbps));
      RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], buffer);
      RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], "%");
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "/");
      NetworkIOMeter_formatNumber(buffer, sizeof(buffer), NetworkIOMeter_linkPercent(data->txbDiff, data->linkSpeedMbps));
      RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], buffer);
      RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], "%");
      RichString_appendAscii(out, CRT_colors[METER_TEXT], ")");
   } else {
      RichString_appendAscii(out, CRT_colors[METER_VALUE_IOREAD], data->rxpDiffStr);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "/");
      RichString_appendAscii(out, CRT_colors[METER_VALUE_IOWRITE], data->txpDiffStr);
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
