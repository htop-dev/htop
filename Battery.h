#ifndef HEADER_Battery
#define HEADER_Battery
/*
htop - Battery.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stddef.h>

#include "BatteryMeter.h"


/* Per-battery measurement, normalized to canonical SI units.
 *
 * NaN means "unknown" for every measurement field; platforms set NaN
 * when the kernel/firmware does not expose the value, or expresses it
 * with a sentinel (-1, 0xFFFF..., FUNKNOWN, "invalid", etc.).
 *
 * `current` and `power` follow htop's sign convention:
 *   positive = discharging, negative = charging.
 *
 * Platforms whose firmware reports unsigned magnitude must combine it
 * with a charge-state flag at parse time before populating these fields. */
typedef struct BatteryRaw_ {
   double level;            /* 0..100 percent, NaN if unknown */
   double energyFull;       /* Wh,  NaN if unknown */
   double energyNow;        /* Wh,  NaN if unknown */
   double chargeFull;       /* Ah,  NaN if unknown */
   double chargeNow;        /* Ah,  NaN if unknown */
   double voltageNow;       /* V,   NaN if unknown */
   double voltageDesign;    /* V,   NaN if unknown */
   double current;          /* A,   NaN if unknown; +discharge, -charge */
   double power;            /* W,   NaN if unknown; +discharge, -charge */
} BatteryRaw;

/* Aggregate per-battery samples into a system-level reading.
 *
 * Inputs must contain only present, non-peripheral system batteries;
 * platform code is responsible for filtering before the call.
 *
 * Capacity contributions are all-or-nothing per dimension: a missing
 * field on any battery suppresses the corresponding output. */
void Battery_aggregate(const BatteryRaw* raws, size_t n, BatteryInfo* out);

#endif
