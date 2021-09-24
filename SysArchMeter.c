/*
htop - SysArchMeter.c
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"  // IWYU pragma: keep

#include "SysArchMeter.h"

#include <stddef.h>

#include "CRT.h"
#include "Object.h"
#include "Platform.h"
#include "XUtils.h"


static const int SysArchMeter_attributes[] = {HOSTNAME};

static void SysArchMeter_updateValues(Meter* this) {
   static char* string;

   if (string == NULL)
      Platform_getRelease(&string);

   String_safeStrncpy(this->txtBuffer, string, sizeof(this->txtBuffer));
}

const MeterClass SysArchMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = SysArchMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = SysArchMeter_attributes,
   .name = "System",
   .uiName = "System",
   .caption = "System: ",
};
