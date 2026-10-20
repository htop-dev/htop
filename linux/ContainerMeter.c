/*
htop - linux/ContainerMeter.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/ContainerMeter.h"

#include "MemoryMeter.h"
#include "Meter.h"
#include "Object.h"
#include "Platform.h"
#include "SwapMeter.h"


static void ContainerMemoryMeter_updateValues(Meter* this) {
   MemoryMeter_updateValuesWith(this, Platform_setCGroupMemoryValues);
}

const MeterClass ContainerMemoryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = MemoryMeter_display,
   },
   .updateValues = ContainerMemoryMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 6, // maximum of MEMORY_N settings
   .isPercentChart = true,
   .total = 100.0,
   .attributes = MemoryMeter_attributes,
   .name = "ContainerMemory",
   .uiName = "Container memory",
   .caption = "Mem"
};

static void ContainerSwapMeter_updateValues(Meter* this) {
   SwapMeter_updateValuesWith(this, Platform_setCGroupSwapValues);
}

const MeterClass ContainerSwapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SwapMeter_display,
   },
   .updateValues = ContainerSwapMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = SWAP_METER_ITEMCOUNT,
   .isPercentChart = true,
   .total = 100.0,
   .attributes = SwapMeter_attributes,
   .name = "ContainerSwap",
   .uiName = "Container swap",
   .caption = "Swp"
};
