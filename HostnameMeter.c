/*
htop - HostnameMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "HostnameMeter.h"

#include <unistd.h>

#include "CRT.h"
#include "Object.h"


static const int HostnameMeter_attributes[] = {
   HOSTNAME
};

SYM_PRIVATE void HostnameMeter_updateValues(Meter* this, char* buffer, size_t size) {
   (void) this;
   gethostname(buffer, size - 1);
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
