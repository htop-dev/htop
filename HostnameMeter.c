/*
htop - HostnameMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "HostnameMeter.h"

#include "CRT.h"
#include "Object.h"
#include "Platform.h"


static const int HostnameMeter_attributes[] = {
   HOSTNAME
};

static void HostnameMeter_updateValues(Meter* this) {
   Platform_getHostname(this->txtBuffer, sizeof(this->txtBuffer));
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
