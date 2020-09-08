/*
htop - HostnameMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "HostnameMeter.h"

#include "CRT.h"

#include <unistd.h>


int HostnameMeter_attributes[] = {
   HOSTNAME
};

static void HostnameMeter_updateValues(Meter* this, char* buffer, int size) {
   (void) this;
   gethostname(buffer, size-1);
}

MeterClass HostnameMeter_class = {
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
