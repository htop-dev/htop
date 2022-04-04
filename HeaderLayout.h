#ifndef HEADER_HeaderLayout
#define HEADER_HeaderLayout
/*
htop - HeaderLayout.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "Macros.h"
#include "XUtils.h"


typedef enum HeaderLayout_ {
   HF_INVALID = -1,
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
   const char* name;
   const char* description;
} HeaderLayout_layouts[LAST_HEADER_LAYOUT] = {
   [HF_TWO_50_50]        = { 2, { 50, 50,  0,  0 }, "two_50_50",        "2 columns - 50/50 (default)", },
   [HF_TWO_33_67]        = { 2, { 33, 67,  0,  0 }, "two_33_67",        "2 columns - 33/67",           },
   [HF_TWO_67_33]        = { 2, { 67, 33,  0,  0 }, "two_67_33",        "2 columns - 67/33",           },
   [HF_THREE_33_34_33]   = { 3, { 33, 34, 33,  0 }, "three_33_34_33",   "3 columns - 33/34/33",        },
   [HF_THREE_25_25_50]   = { 3, { 25, 25, 50,  0 }, "three_25_25_50",   "3 columns - 25/25/50",        },
   [HF_THREE_25_50_25]   = { 3, { 25, 50, 25,  0 }, "three_25_50_25",   "3 columns - 25/50/25",        },
   [HF_THREE_50_25_25]   = { 3, { 50, 25, 25,  0 }, "three_50_25_25",   "3 columns - 50/25/25",        },
   [HF_THREE_40_20_40]   = { 3, { 40, 20, 40,  0 }, "three_40_20_40",   "3 columns - 40/20/40",        },
   [HF_FOUR_25_25_25_25] = { 4, { 25, 25, 25, 25 }, "four_25_25_25_25", "4 columns - 25/25/25/25",     },
};

static inline size_t HeaderLayout_getColumns(HeaderLayout hLayout) {
   /* assert the layout is initialized */
   assert(0 <= hLayout);
   assert(hLayout < LAST_HEADER_LAYOUT);
   assert(HeaderLayout_layouts[hLayout].name[0]);
   assert(HeaderLayout_layouts[hLayout].description[0]);
   return HeaderLayout_layouts[hLayout].columns;
}

static inline const char* HeaderLayout_getName(HeaderLayout hLayout) {
   /* assert the layout is initialized */
   assert(0 <= hLayout);
   assert(hLayout < LAST_HEADER_LAYOUT);
   assert(HeaderLayout_layouts[hLayout].name[0]);
   assert(HeaderLayout_layouts[hLayout].description[0]);
   return HeaderLayout_layouts[hLayout].name;
}

static inline HeaderLayout HeaderLayout_fromName(const char* name) {
   for (size_t i = 0; i < LAST_HEADER_LAYOUT; i++) {
      if (String_eq(HeaderLayout_layouts[i].name, name))
         return (HeaderLayout) i;
   }

   return LAST_HEADER_LAYOUT;
}

#endif /* HEADER_HeaderLayout */
