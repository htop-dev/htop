/*
htop - HostnameMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "HostnameMeter.h"

#include <netinet/in.h>

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "XUtils.h"


#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

static const int HostnameMeter_attributes[] = {
   HOSTNAME
};

static void HostnameMeter_updateValues(Meter* this) {
   Platform_getHostname(this->txtBuffer, sizeof(this->txtBuffer));
}

static void HostnameIPv4Meter_updateValues(Meter* this) {
   char hostnameBuffer[HOST_NAME_MAX];
   Platform_getHostname(hostnameBuffer, sizeof(hostnameBuffer));

   if (!this->curChoice) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s (invalid choice)", hostnameBuffer);
   } else {
      char ipv4[INET_ADDRSTRLEN];
      Platform_getLocalIPv4address(this->curChoice, ipv4, sizeof(ipv4));

      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s (%s @ %s)", hostnameBuffer, ipv4, this->curChoice);
   }
}

static void HostnameIPv6Meter_updateValues(ATTR_UNUSED Meter* this) {
   char hostnameBuffer[HOST_NAME_MAX];
   Platform_getHostname(hostnameBuffer, sizeof(hostnameBuffer));

   if (!this->curChoice) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s (invalid choice)", hostnameBuffer);
   } else {
      char ipv6[INET6_ADDRSTRLEN];
      Platform_getLocalIPv6address(this->curChoice, ipv6, sizeof(ipv6));

      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s (%s @ %s)", hostnameBuffer, ipv6, this->curChoice);
   }
}

const MeterClass HostnameMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = HostnameMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = HostnameMeter_attributes,
   .name = "Hostname",
   .uiName = "Hostname",
   .caption = "Hostname: ",
};

const MeterClass HostnameIPv4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = HostnameIPv4Meter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = HostnameMeter_attributes,
   .name = "HostnameIPv4",
   .uiName = "HostnameIPv4",
   .description = "Hostname with link-local IPv4 address",
   .caption = "Hostname: ",
   .getChoices = Platform_getLocalIPv4addressChoices,
};

const MeterClass HostnameIPv6Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = HostnameIPv6Meter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = HostnameMeter_attributes,
   .name = "HostnameIPv6",
   .uiName = "HostnameIPv6",
   .description = "Hostname with link-local IPv6 address",
   .caption = "Hostname: ",
   .getChoices = Platform_getLocalIPv6addressChoices,
};
