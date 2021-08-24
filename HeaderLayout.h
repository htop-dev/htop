#ifndef HEADER_HeaderLayout
#define HEADER_HeaderLayout
/*
htop - HeaderLayout.h
(C) 2021 htop dev team
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "Macros.h"


typedef enum HeaderLayout_ {
   HF_TWO_50_50,
   HF_TWO_33_67,
   HF_TWO_67_33,
   HF_THREE_33_34_33,
   HF_THREE_25_25_50,
   HF_THREE_25_50_25,
   HF_THREE_50_25_25,
   HF_THREE_40_20_40,
   HF_FOUR_25_25_25_25,
   LAST_HEADER_LAYOUT
} HeaderLayout;

static const struct {
   uint8_t columns;
   const uint8_t widths[4];
   const char* description;
} HeaderLayout_layouts[LAST_HEADER_LAYOUT] = {
   [HF_TWO_50_50]        = { 2, { 50, 50,  0,  0 }, "2 columns - 50/50 (default)", },
   [HF_TWO_33_67]        = { 2, { 33, 67,  0,  0 }, "2 columns - 33/67", },
   [HF_TWO_67_33]        = { 2, { 67, 33,  0,  0 }, "2 columns - 67/33", },
   [HF_THREE_33_34_33]   = { 3, { 33, 34, 33,  0 }, "3 columns - 33/34/33", },
   [HF_THREE_25_25_50]   = { 3, { 25, 25, 50,  0 }, "3 columns - 25/25/50", },
   [HF_THREE_25_50_25]   = { 3, { 25, 50, 25,  0 }, "3 columns - 25/50/25", },
   [HF_THREE_50_25_25]   = { 3, { 50, 25, 25,  0 }, "3 columns - 50/25/25", },
   [HF_THREE_40_20_40]   = { 3, { 40, 20, 40,  0 }, "3 columns - 40/20/40", },
   [HF_FOUR_25_25_25_25] = { 4, { 25, 25, 25, 25 }, "4 columns - 25/25/25/25", },
};

static inline size_t HeaderLayout_getColumns(HeaderLayout hLayout) {
   /* assert the layout is initialized */
   assert(0 <= hLayout);
   assert(hLayout < LAST_HEADER_LAYOUT);
   assert(HeaderLayout_layouts[hLayout].description[0]);
   return HeaderLayout_layouts[hLayout].columns;
}

#endif /* HEADER_HeaderLayout */
