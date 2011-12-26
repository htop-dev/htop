/*
htop - HostnameMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "HostnameMeter.h"

#include "CRT.h"

#include <unistd.h>

/*{
#include "Meter.h"
}*/

int HostnameMeter_attributes[] = {
   HOSTNAME
};

static void HostnameMeter_setValues(Meter* this, char* buffer, int size) {
   (void) this;
   gethostname(buffer, size-1);
}

MeterType HostnameMeter = {
   .setValues = HostnameMeter_setValues, 
   .display = NULL,
   .mode = TEXT_METERMODE,
   .total = 100.0,
   .items = 1,
   .attributes = HostnameMeter_attributes,
   .name = "Hostname",
   .uiName = "Hostname",
   .caption = "Hostname: ",
};
