/*
htop - Meter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Meter.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "Row.h"
#include "Settings.h"
#include "XUtils.h"


#ifndef UINT16_WIDTH
#define UINT16_WIDTH 16
#endif

#ifndef UINT32_WIDTH
#define UINT32_WIDTH 32
#endif

#define DEFAULT_GRAPH_HEIGHT 4 /* Unit: rows (lines) */
#define MAX_GRAPH_HEIGHT 8191 /* == (int)(UINT16_MAX / 8) */

typedef struct GraphColorCell_ {
   uint8_t itemNum;
   uint8_t details;
} GraphColorCell;

typedef union GraphDataCell_ {
   int16_t scaleExp;
   uint16_t numDots;
   GraphColorCell c;
} GraphDataCell;

typedef struct GraphDrawContext_ {
   uint8_t maxItems;
   bool isPercentChart;
   size_t nCellsPerValue;
} GraphDrawContext;

typedef struct GraphColorAdjStack_ {
   double startPoint;
   double fractionSum;
   double valueSum;
   uint8_t nItems;
} GraphColorAdjStack;

typedef struct GraphColorAdjOffset_ {
   uint32_t offsetVal; /* "offsetVal" requires at least 22 bits */
   unsigned int nCells;
} GraphColorAdjOffset;

typedef struct GraphColorComputeState_ {
   double valueSum;
   unsigned int nCellsPainted;
   uint8_t nItemsPainted;
} GraphColorComputeState;

typedef struct MeterMode_ {
   Meter_Draw draw;
   const char* uiName;
   int h;
} MeterMode;

/* Meter drawing modes */

static inline void Meter_displayBuffer(const Meter* this, RichString* out) {
   if (Object_displayFn(this)) {
      Object_display(this, out);
   } else {
      RichString_writeWide(out, CRT_colors[Meter_attributes(this)[0]], this->txtBuffer);
   }
}

static double Meter_computeSum(const Meter* this) {
   double sum = sumPositiveValues(this->values, this->curItems);
   // Prevent rounding to infinity in IEEE 754
   return MINIMUM(DBL_MAX, sum);
}

/* ---------- TextMeterMode ---------- */

static void TextMeterMode_draw(Meter* this, int x, int y, int w) {
   const char* caption = Meter_getCaption(this);
   attrset(CRT_colors[METER_TEXT]);
   mvaddnstr(y, x, caption, w);
   attrset(CRT_colors[RESET_COLOR]);

   int captionLen = strlen(caption);
   x += captionLen;
   w -= captionLen;
   if (w <= 0)
      return;

   RichString_begin(out);
   Meter_displayBuffer(this, &out);
   RichString_printoffnVal(out, y, x, 0, w);
   RichString_delete(&out);
}

/* ---------- BarMeterMode ---------- */

static const char BarMeterMode_characters[] = "|#*@$%&.";

static void BarMeterMode_draw(Meter* this, int x, int y, int w) {
   // Draw the caption
   const char* caption = Meter_getCaption(this);
   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, caption, captionLen);
   x += captionLen;
   w -= captionLen;

   // Draw the bar borders
   attrset(CRT_colors[BAR_BORDER]);
   mvaddch(y, x, '[');
   w--;
   mvaddch(y, x + MAXIMUM(w, 0), ']');
   w--;
   attrset(CRT_colors[RESET_COLOR]);

   x++;

   if (w < 1) {
      return;
   }

   // The text in the bar is right aligned;
   // Pad with maximal spaces and then calculate needed starting position offset
   RichString_begin(bar);
   RichString_appendChr(&bar, 0, ' ', w);
   RichString_appendWide(&bar, 0, this->txtBuffer);

   int startPos = RichString_sizeVal(bar) - w;
   if (startPos > w) {
      // Text is too large for bar
      // Truncate meter text at a space character
      for (int pos = 2 * w; pos > w; pos--) {
         if (RichString_getCharVal(bar, pos) == ' ') {
            while (pos > w && RichString_getCharVal(bar, pos - 1) == ' ')
               pos--;
            startPos = pos - w;
            break;
         }
      }

      // If still too large, print the start not the end
      startPos = MINIMUM(startPos, w);
   }

   assert(startPos >= 0);
   assert(startPos <= w);
   assert(startPos + w <= RichString_sizeVal(bar));

   int blockSizes[10];

   // First draw in the bar[] buffer...
   int offset = 0;
   if (!Meter_isPercentChart(this) && this->curItems > 0) {
      double sum = Meter_computeSum(this);
      if (this->total < sum) {
         this->total = sum;
      }
   }
   for (uint8_t i = 0; i < this->curItems; i++) {
      double value = this->values[i];
      if (isPositive(value) && this->total > 0.0) {
         value = MINIMUM(value, this->total);
         blockSizes[i] = ceil((value / this->total) * w);
      } else {
         blockSizes[i] = 0;
      }
      int nextOffset = offset + blockSizes[i];
      // (Control against invalid values)
      nextOffset = CLAMP(nextOffset, 0, w);
      for (int j = offset; j < nextOffset; j++)
         if (RichString_getCharVal(bar, startPos + j) == ' ') {
            if (CRT_colorScheme == COLORSCHEME_MONOCHROME) {
               assert(i < strlen(BarMeterMode_characters));
               RichString_setChar(&bar, startPos + j, BarMeterMode_characters[i]);
            } else {
               RichString_setChar(&bar, startPos + j, '|');
            }
         }
      offset = nextOffset;
   }

   // ...then print the buffer.
   offset = 0;
   for (uint8_t i = 0; i < this->curItems; i++) {
      int attr = this->curAttributes ? this->curAttributes[i] : Meter_attributes(this)[i];
      RichString_setAttrn(&bar, CRT_colors[attr], startPos + offset, blockSizes[i]);
      RichString_printoffnVal(bar, y, x + offset, startPos + offset, MINIMUM(blockSizes[i], w - offset));
      offset += blockSizes[i];
      offset = CLAMP(offset, 0, w);
   }
   if (offset < w) {
      RichString_setAttrn(&bar, CRT_colors[BAR_SHADOW], startPos + offset, w - offset);
      RichString_printoffnVal(bar, y, x + offset, startPos + offset, w - offset);
   }

   RichString_delete(&bar);

   move(y, x + w + 1);
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- GraphMeterMode ---------- */

static void GraphMeterMode_reallocateGraphBuffer(Meter* this, const GraphDrawContext* context, size_t nValues) {
   GraphData* data = &this->drawData;

   size_t nCellsPerValue = context->nCellsPerValue;
   size_t valueSize = nCellsPerValue * sizeof(GraphDataCell);

   if (!valueSize)
      goto bufferInitialized;

   data->buffer = xReallocArray(data->buffer, nValues, valueSize);

   // Move existing records ("values") to correct position
   assert(nValues >= data->nValues);
   size_t moveOffset = (nValues - data->nValues) * nCellsPerValue;
   GraphDataCell* dest = &((GraphDataCell*)data->buffer)[moveOffset];
   memmove(dest, data->buffer, data->nValues * valueSize);

   // Fill new spaces with blank records
   memset(data->buffer, 0, moveOffset * sizeof(GraphDataCell));

bufferInitialized:
   data->nValues = nValues;
}

static size_t GraphMeterMode_valueCellIndex(unsigned int graphHeight, bool isPercentChart, int deltaExp, unsigned int y, unsigned int* scaleFactor, unsigned int* increment) {
   assert(deltaExp >= 0);
   assert(deltaExp < UINT16_WIDTH);

   if (scaleFactor)
      *scaleFactor = 1;
   if (increment)
      *increment = isPercentChart ? 1 : (2U << deltaExp);

   unsigned int yTop = (graphHeight - 1) >> deltaExp;
   if (y > yTop) {
      return (size_t)-1;
   }

   if (isPercentChart) {
      assert(deltaExp == 0);
      return y;
   }

   // A record may be rendered in different scales depending on the largest
   // "scaleExp" value of a record set. The colors are precomputed for
   // different scales of the same record. It takes (2 * graphHeight - 1) cells
   // of space to store all the color information.
   //
   // An example for graphHeight = 6:
   //
   //    scale  1*n  2*n  4*n  8*n 16*n | n = value sum of all items
   // --------------------------------- |     rounded up to a power of
   // deltaExp    0    1    2    3    4 |     two. The exponent of n is
   // --------------------------------- |     stored in index [0].
   //    array [11]    X    X    X    X | X = empty cell
   //  indices  [9]    X    X    X    X | Cells whose array indices
   //           [7]    X    X    X    X | are >= (2 * graphHeight) are
   //           [5] [10]    X    X    X | computed from cells of a
   //           [3]  [6] (12)    X    X | lower scale and not stored in
   //           [1]  [2]  [4]  [8] (16) | the array.

   // "b" is the "base" offset or the upper bits of offset
   unsigned int b = (y * 2) << deltaExp;
   unsigned int offset = 1U << deltaExp;

   if (!scaleFactor) {
      // This function is called for writing. The "increment" argument is
      // optional, but the caller should assert the index is in bounds.
      if (b + offset > 2 * graphHeight - 1) {
         return (size_t)-1;
      }
      return b + offset;
   }

   // This function is called for reading.
   assert(!increment);

   if (y < yTop) {
      return b + offset;
   }

   assert(((2 * graphHeight - 1) & b) == b);

   unsigned int offsetTop = powerOf2Floor(2 * graphHeight - 1 - b);
   if (offsetTop) {
      *scaleFactor = offset / offsetTop;
   }

   return b + offsetTop;
}

static uint8_t GraphMeterMode_findTopCellItem(const Meter* this, double scaledTotal, unsigned int topCell) {
   unsigned int graphHeight = (unsigned int)this->h;
   assert(topCell < graphHeight);

   double valueSum = 0.0;
   double maxValue = 0.0;
   uint8_t topCellItem = this->curItems - 1;
   for (uint8_t i = 0; i < this->curItems && valueSum < DBL_MAX; i++) {
      double value = this->values[i];
      if (!isPositive(value))
         continue;

      double newValueSum = valueSum + value;
      if (newValueSum > DBL_MAX)
         newValueSum = DBL_MAX;

      if (value > DBL_MAX - valueSum) {
         value = DBL_MAX - valueSum;
         // This assumption holds for the new "value" as long as the
         // rounding mode is consistent.
         assert(newValueSum < DBL_MAX || valueSum + value >= DBL_MAX);
      }

      valueSum = newValueSum;

      // Find the item that occupies the largest area of the top cell.
      // Favor the item with higher index in case of a tie.

      if (topCell > 0) {
         double topPoint = (valueSum / scaledTotal) * (double)(int)graphHeight;
         assert(topPoint >= 0.0);

         if (!(topPoint > (double)(int)topCell))
            continue;

         // This code assumes the default FP rounding mode (i.e. to nearest),
         // which requires "area" to be at least (DBL_EPSILON / 2) to win.

         double area = (value / scaledTotal) * (double)(int)graphHeight;
         assert(area >= 0.0);

         if (area > topPoint - (double)(int)topCell)
            area = topPoint - (double)(int)topCell;

         if (area >= maxValue) {
            maxValue = area;
            topCellItem = i;
         }
      } else {
         // Compare "value" directly. It is possible for an "area" to
         // underflow here and still wins as the largest area.
         if (value >= maxValue) {
            maxValue = value;
            topCellItem = i;
         }
      }
   }
   return topCellItem;
}

static int8_t GraphMeterMode_needsExtraCell(unsigned int graphHeight, double scaledTotal, unsigned int y, const GraphColorAdjStack* stack, const GraphColorAdjOffset* adjOffset) {
   double areaSum = (stack->fractionSum + stack->valueSum / scaledTotal) * (double)(int)graphHeight;
   double adjOffsetVal = adjOffset ? (double)(int32_t)adjOffset->offsetVal : 0.0;
   double halfPoint = (double)(int)y + 0.5;

   // Calculate the best position for rendering this stack of items.
   // Given real numbers a, b, c and d (a <= b <= c <= d), then:
   // 1. The smallest value for (x - a)^2 + (x - b)^2 + (x - c)^2 + (x - d)^2
   // happens when x == (a + b + c + d) / 4; x is the "arithmetic mean".
   // 2. The smallest value for |y - a| + |y - b| + |y - c| + |y - d|
   // happens when b <= y <= c; y is the "median".
   // Both kinds of averages are acceptable. The arithmetic mean is chosen
   // here because it is cheaper to produce.

   // averagePoint := stack->startPoint + (areaSum / (stack->nItems * 2))
   // adjStartPoint := averagePoint - (adjOffsetVal / (stack->nItems * 2))

   // Intended to compare this but with greater precision:
   // isgreater(adjStartPoint, halfPoint)
   if (areaSum - adjOffsetVal > (halfPoint - stack->startPoint) * 2.0 * stack->nItems)
      return 1;

   if (areaSum - adjOffsetVal < (halfPoint - stack->startPoint) * 2.0 * stack->nItems)
      return 0;

   assert(stack->valueSum <= DBL_MAX);
   double stackArea = (stack->valueSum / scaledTotal) * (double)(int)graphHeight;
   double adjNCells = adjOffset ? (double)(int)adjOffset->nCells : 0.0;

   // Intended to compare this but with greater precision:
   // (stack->startPoint + (stackArea / 2) > halfPoint + (adjNCells / 2))
   if (stackArea - adjNCells > (halfPoint - stack->startPoint) * 2.0)
      return 1;

   if (stackArea - adjNCells < (halfPoint - stack->startPoint) * 2.0)
      return 0;

   return -1;
}

static void GraphMeterMode_addItemAdjOffset(GraphColorAdjOffset* adjOffset, unsigned int nCells) {
   adjOffset->offsetVal += (uint32_t)adjOffset->nCells * 2 + nCells;
   adjOffset->nCells += nCells;
}

static void GraphMeterMode_addItemAdjStack(GraphColorAdjStack* stack, double scaledTotal, double value) {
   assert(scaledTotal <= DBL_MAX);
   assert(stack->valueSum < DBL_MAX);

   stack->fractionSum += (stack->valueSum / scaledTotal) * 2.0;
   stack->valueSum += value;

   assert(stack->nItems < UINT8_MAX);
   stack->nItems++;
}

static uint16_t GraphMeterMode_makeDetailsMask(const GraphColorComputeState* prev, const GraphColorComputeState* new, double prevTopPoint, double rem, int blanksAtTopCell) {
   assert(new->nCellsPainted > prev->nCellsPainted);
   assert(rem >= 0.0);
   assert(rem < 1.0);

   double numDots = ceil(rem * 8.0);

   uint8_t blanksAtEnd;
   int8_t roundDirInAscii = 0;
   if (blanksAtTopCell >= 0) {
      assert(blanksAtTopCell < 8);
      blanksAtEnd = (uint8_t)blanksAtTopCell;
      roundDirInAscii = 1;
   } else if (prev->nCellsPainted == 0 || prevTopPoint <= (double)(int)prev->nCellsPainted || (uint8_t)numDots == 0) {
      blanksAtEnd = (uint8_t)(8 - (uint8_t)numDots) % 8;
   } else {
      // Unlike other conditions, this one rounds to nearest for visual reason.
      // In case of a tie, display the dot at lower position of the graph,
      // i.e. MSB of the "details" data.

      double distance = prevTopPoint - (double)(int)prev->nCellsPainted;
      distance = distance + rem * 0.5;

      // Tiebreaking direction that may be needed in the ASCII display mode.
      if (distance > 0.5) {
         roundDirInAscii = 1;
      } else if (distance < 0.5) {
         roundDirInAscii = -1;
      }

      distance *= 8.0;
      if ((uint8_t)numDots % 2 == 0) {
         distance -= 0.5;
      }
      distance = ceil(distance);
      assert(distance >= 0.0);
      assert(distance < INT_MAX);

      unsigned int blanksRem = 8 - (unsigned int)(int)numDots / 2;
      blanksRem -= MINIMUM(blanksRem, (unsigned int)(int)distance);
      blanksAtEnd = (uint8_t)blanksRem;
   }
   assert(blanksAtEnd < 8);

   uint8_t blanksAtStart;
   if (prev->nCellsPainted > 0) {
      blanksAtStart = (uint8_t)(8 - (uint8_t)numDots - blanksAtEnd) % 8;
   } else {
      // Always zero blanks for the first cell.
      // When an item would be painted with all cells (from the first cell to
      // the "top cell"), it is expected that the bar would be "stretched" to
      // represent the sum of the record.
      blanksAtStart = 0;
   }
   assert(blanksAtStart < 8);

   uint16_t mask = 0xFFFFU >> blanksAtStart;
   // See the code and comments of the "printCellDetails" function for how
   // special bits are used.
   bool needsTiebreak = blanksAtStart >= 2 && blanksAtStart < 4 && blanksAtStart == blanksAtEnd;

   if (new->nCellsPainted - prev->nCellsPainted == 1) {
      assert(blanksAtStart + blanksAtEnd < 8);
      if (roundDirInAscii > 0 && needsTiebreak) {
         assert((mask & 0x0800) != 0);
         mask ^= 0x0800;
      }
      mask >>= 8;
   } else if (roundDirInAscii > 0) {
      if (blanksAtStart < 4 && (uint8_t)(blanksAtStart + blanksAtEnd % 4) >= 4) {
         assert((mask & 0x0800) != 0);
         mask ^= 0x0800;
      }
   }

   mask = (uint16_t)((mask >> blanksAtEnd) << blanksAtEnd);

   if (needsTiebreak) {
      if (roundDirInAscii > 0) {
         mask |= 0x0004;
      } else if (roundDirInAscii < 0) {
         assert((mask & 0x0010) != 0);
         mask = (mask & 0xFFEF) | 0x0020;
      }
   } else if (roundDirInAscii < 0) {
      assert(blanksAtStart <= blanksAtEnd);
      if ((mask | 0x4000) == 0x7FF8) {
         // This special case is the combination of the 4 conditionals,
         // shown as asserts below.
         assert(new->nCellsPainted - prev->nCellsPainted > 1);
         assert(blanksAtEnd < 4);
         assert(blanksAtStart % 4 + blanksAtEnd >= 4);
         assert(blanksAtStart < blanksAtEnd);

         mask = (mask & 0xFFEF) | 0x0020;
      }
   }

   // The following result values are impossible as they lack special bits
   // needed for the ASCII display mode.
   assert(mask != 0x3FF8); // Should be 0x37F8 or 0x3FE8
   assert(mask != 0x7FF8); // Should be 0x77F8 or 0x7FE8
   assert(mask != 0x1FFC); // Should be 0x17FC
   assert(mask != 0x1FFE); // Should be 0x17FE

   return mask;
}

static void GraphMeterMode_paintCellsForItem(GraphDataCell* cellsStart, unsigned int increment, uint8_t itemIndex, unsigned int nCells, uint16_t mask) {
   GraphDataCell* cell = cellsStart;
   while (nCells > 0) {
      cell->c.itemNum = itemIndex + 1;
      if (nCells == 1) {
         cell->c.details = (uint8_t)mask;
      } else if (cell == cellsStart) {
         cell->c.details = mask >> 8;
      } else {
         cell->c.details = 0xFF;
      }
      nCells--;
      cell += increment;
   }
}

static void GraphMeterMode_computeColors(Meter* this, const GraphDrawContext* context, GraphDataCell* valueStart, int deltaExp, double scaledTotal, unsigned int numDots) {
   unsigned int graphHeight = (unsigned int)this->h;
   bool isPercentChart = context->isPercentChart;

   assert(deltaExp >= 0);
   assert(numDots > 0);
   assert(numDots <= graphHeight * 8);

   unsigned int increment;
   size_t firstCellIndex = GraphMeterMode_valueCellIndex(graphHeight, isPercentChart, deltaExp, 0, NULL, &increment);
   assert(firstCellIndex < context->nCellsPerValue);

   unsigned int topCell = (numDots - 1) / 8;
   const uint8_t dotAlignment = 2;
   unsigned int blanksAtTopCell = ((topCell + 1) * 8 - numDots) / dotAlignment * dotAlignment;

   bool hasPartialTopCell = false;
   if (blanksAtTopCell > 0) {
      hasPartialTopCell = true;
   } else if (!isPercentChart && topCell % 2 == 0 && topCell == ((graphHeight - 1) >> deltaExp)) {
      // This "top cell" is rendered as full in one scale, but partial in the
      // next scale. (Only happens when graphHeight is not a power of two.)
      hasPartialTopCell = true;
   }

   double topCellArea = 0.0;
   assert(this->curItems > 0);
   uint8_t topCellItem = this->curItems - 1;
   if (hasPartialTopCell) {
      // Allocate the "top cell" first. The item that acquires the "top cell"
      // will have a smaller "area" for the remainder calculation below.
      topCellArea = (8 - (int)blanksAtTopCell) / 8.0;
      topCellItem = GraphMeterMode_findTopCellItem(this, scaledTotal, topCell);
   }

   GraphColorComputeState restart = {
      .valueSum = 0.0,
      .nCellsPainted = 0,
      .nItemsPainted = 0
   };
   double thresholdHigh = 1.0;
   double thresholdLow = 0.0;
   double threshold = 0.5;
   bool rItemIsDetermined = false;
   bool rItemHasExtraCell = true;
   unsigned int nCellsToPaint = topCell + 1;
   bool isLastTiebreak = false;
   unsigned int nCellsPaintedHigh = nCellsToPaint + topCellItem + 1;
   unsigned int nCellsPaintedLow = 0;

   while (true) {
      GraphColorComputeState prev = restart;
      double nextThresholdLow = thresholdHigh;
      double nextThresholdHigh = thresholdLow;
      bool hasThresholdRange = thresholdLow < thresholdHigh;
      GraphColorAdjStack stack = {
         .startPoint = 0.0,
         .fractionSum = 0.0,
         .valueSum = 0.0,
         .nItems = 0
      };
      GraphColorAdjOffset adjSmall = {
         .offsetVal = 0,
         .nCells = 0
      };
      GraphColorAdjOffset adjLarge = adjSmall;

      while (prev.nItemsPainted <= topCellItem && prev.valueSum < DBL_MAX) {
         double value = this->values[prev.nItemsPainted];
         if (!isPositive(value)) {
            if (restart.nItemsPainted == prev.nItemsPainted) {
               restart.nItemsPainted++;
            }
            prev.nItemsPainted++;
            continue;
         }

         GraphColorComputeState new;

         new.valueSum = prev.valueSum + value;
         if (new.valueSum > DBL_MAX)
            new.valueSum = DBL_MAX;

         if (value > DBL_MAX - prev.valueSum) {
            value = DBL_MAX - prev.valueSum;
            // This assumption holds for the new "value" as long as the
            // rounding mode is consistent.
            assert(new.valueSum < DBL_MAX || prev.valueSum + value >= DBL_MAX);
         }

         double area = (value / scaledTotal) * (double)(int)graphHeight;
         assert(area >= 0.0); // "area" can be 0.0 when the division underflows
         double rem = area;

         if (prev.nItemsPainted == topCellItem)
            rem = MAXIMUM(area, topCellArea) - topCellArea;

         unsigned int nCells = (unsigned int)(int)rem;
         rem -= (int)rem;

         // Whether the item will receive an extra cell or be truncated.
         // The main method is known as the "largest remainder method".

         // An item whose remainder reaches the Droop quota may either receive
         // an extra cell or need a tiebreak (a tie caused by rounding).
         // This is the highest threshold we might need to compare with.
         bool reachesDroopQuota = rem * (double)(int)(graphHeight + 1) > (double)(int)graphHeight;
         if (reachesDroopQuota && rem < thresholdHigh)
            thresholdHigh = rem;

         bool equalsThreshold = false;
         bool isInThresholdRange = rem <= thresholdHigh && rem >= thresholdLow;

         assert(threshold > 0.0);
         assert(threshold <= 1.0);
         if (rem > threshold) {
            if (rem < nextThresholdLow) {
               nextThresholdLow = rem;
            }
            nCells++;
         } else if (rem < threshold) {
            if (rem > nextThresholdHigh) {
               nextThresholdHigh = rem;
            }
            rem = 0.0;
         } else if (hasThresholdRange) {
            assert(!rItemIsDetermined);
            nCells++;
         } else if (restart.nItemsPainted >= prev.nItemsPainted) {
            assert(restart.nItemsPainted == prev.nItemsPainted);

            // This item will be nicknamed "rItem". Whether the "rItem" will
            // receive an extra cell is determined by the rest of the loop.
            if (!rItemIsDetermined) {
               stack.startPoint = (new.valueSum / scaledTotal) * (double)(int)graphHeight;
               rem = 0.0;
            } else if (rItemHasExtraCell) {
               nCells++;
            } else {
               rem = 0.0;
            }
         } else {
            equalsThreshold = true;
            rem = 0.0;

            unsigned int y = prev.nCellsPainted - adjSmall.nCells;
            unsigned int rItemMinCells = y - restart.nCellsPainted;

            // The first cell and last cell are painted with dots aligned to the
            // bottom and top respectively. If multiple items whose remainders
            // equal the threshold and would be painted on the same cell, give
            // priority to the first or last of the items respectively.

            if (prev.nCellsPainted == 0) {
               assert(adjSmall.nCells == 0);
               rItemHasExtraCell = true;
            } else if (y + 1 >= nCellsToPaint) {
               assert(y + 1 == nCellsToPaint);
               assert(adjSmall.nCells == 0);
               assert(nCells == 0);
               rItemHasExtraCell = false;
            } else if (!rItemHasExtraCell) {
               assert(adjLarge.nCells > adjSmall.nCells);

               int8_t res = GraphMeterMode_needsExtraCell(graphHeight, scaledTotal, y, &stack, &adjLarge);
               if (res > 0 || (res < 0 && rItemMinCells <= nCells)) {
                  rItemHasExtraCell = true;
               }
            } else {
               int8_t res = GraphMeterMode_needsExtraCell(graphHeight, scaledTotal, y, &stack, &adjSmall);
               if (res == 0 || (res < 0 && (rItemMinCells > nCells || prev.nCellsPainted + 1 >= nCellsToPaint))) {
                  rItemHasExtraCell = false;
               }
            }
         }

         if (!hasThresholdRange && restart.nItemsPainted < prev.nItemsPainted) {
            GraphMeterMode_addItemAdjOffset(&adjSmall, nCells);
            GraphMeterMode_addItemAdjOffset(&adjLarge, nCells + equalsThreshold);
            GraphMeterMode_addItemAdjStack(&stack, scaledTotal, value);
         }

         if (hasPartialTopCell && prev.nItemsPainted == topCellItem)
            nCells++;

         new.nCellsPainted = prev.nCellsPainted + nCells;
         new.nItemsPainted = prev.nItemsPainted + 1;

         // Update the "restart" state if needed
         if (restart.nItemsPainted >= prev.nItemsPainted) {
            if (!isInThresholdRange) {
               restart = new;
            } else if (rItemIsDetermined) {
               restart = new;
               rItemIsDetermined = isLastTiebreak;
               rItemHasExtraCell = true;
            }
         }

         // Paint cells to the buffer
         if (hasPartialTopCell && prev.nItemsPainted == topCellItem) {
            // Re-calculate the remainder with the top cell area included
            if (rem > 0.0) {
               // Has extra cell won from the largest remainder method
               rem = area;
            } else {
               // Did not win extra cell from the remainder
               rem = MINIMUM(area, topCellArea);
            }
            rem -= (int)rem;
         }

         bool isItemOnEdge = (prev.nCellsPainted == 0 || new.nCellsPainted == nCellsToPaint);
         if (isItemOnEdge && area < (0.125 * dotAlignment))
            rem = (0.125 * dotAlignment);

         if (nCells > 0 && new.nCellsPainted <= nCellsToPaint) {
            double prevTopPoint = (prev.valueSum / scaledTotal) * (double)(int)graphHeight;
            int blanksAtTopCellArg = (new.nCellsPainted == nCellsToPaint) ? (int)blanksAtTopCell : -1;
            uint16_t mask = GraphMeterMode_makeDetailsMask(&prev, &new, prevTopPoint, rem, blanksAtTopCellArg);

            GraphDataCell* cellsStart = &valueStart[firstCellIndex + (size_t)increment * prev.nCellsPainted];
            GraphMeterMode_paintCellsForItem(cellsStart, increment, prev.nItemsPainted, nCells, mask);
         }

         prev = new;
      }

      if (hasThresholdRange) {
         if (prev.nCellsPainted == nCellsToPaint)
            break;

         // Set new threshold range
         if (prev.nCellsPainted > nCellsToPaint) {
            nCellsPaintedHigh = prev.nCellsPainted;
            assert(thresholdLow < threshold);
            thresholdLow = threshold;
         } else {
            nCellsPaintedLow = prev.nCellsPainted + 1;
            assert(thresholdHigh > nextThresholdHigh);
            thresholdHigh = nextThresholdHigh;
            nextThresholdLow = thresholdLow;
         }

         // Make new threshold value
         threshold = thresholdHigh;
         hasThresholdRange = thresholdLow < thresholdHigh;
         if (hasThresholdRange && nCellsPaintedLow < nCellsPaintedHigh) {
            // Linear interpolation
            assert(nCellsPaintedLow <= nCellsToPaint);
            threshold -= ((thresholdHigh - thresholdLow) * (double)(int)(nCellsToPaint - nCellsPaintedLow) / (double)(int)(nCellsPaintedHigh - nCellsPaintedLow));
            if (threshold < nextThresholdLow) {
               threshold = nextThresholdLow;
            }
         }
         assert(threshold <= thresholdHigh);
      } else if (restart.nItemsPainted <= topCellItem && restart.valueSum < DBL_MAX) {
         if (prev.nCellsPainted - adjSmall.nCells + adjLarge.nCells < nCellsToPaint) {
            rItemHasExtraCell = true;
            isLastTiebreak = true;
         } else if (prev.nCellsPainted >= nCellsToPaint) {
            assert(prev.nCellsPainted == nCellsToPaint);
            break;
         }
         rItemIsDetermined = true;
      } else {
         assert(restart.nCellsPainted == nCellsToPaint);
         break;
      }
   }
}

static void GraphMeterMode_recordNewValue(Meter* this, const GraphDrawContext* context) {
   uint8_t maxItems = context->maxItems;
   bool isPercentChart = context->isPercentChart;
   size_t nCellsPerValue = context->nCellsPerValue;
   if (!nCellsPerValue)
      return;

   GraphData* data = &this->drawData;
   size_t nValues = data->nValues;
   unsigned int graphHeight = (unsigned int)this->h;

   // Move previous records
   size_t valueSize = nCellsPerValue * sizeof(GraphDataCell);
   GraphDataCell* valueStart = (GraphDataCell*)data->buffer;
   valueStart = &valueStart[1 * nCellsPerValue];
   memmove(data->buffer, valueStart, (nValues - 1) * valueSize);

   valueStart = (GraphDataCell*)data->buffer;
   valueStart = &valueStart[(nValues - 1) * nCellsPerValue];

   // Compute "sum" and "total"
   double sum = Meter_computeSum(this);
   assert(sum >= 0.0);
   assert(sum <= DBL_MAX);
   double total;
   int scaleExp = 0;
   if (isPercentChart) {
      total = MAXIMUM(this->total, sum);
   } else {
      (void) frexp(sum, &scaleExp);
      if (scaleExp < 0) {
         scaleExp = 0;
      }
      // In IEEE 754 binary64 (DBL_MAX_EXP == 1024, DBL_MAX_10_EXP == 308),
      // "scaleExp" never overflows.
      assert(DBL_MAX_10_EXP < 9864);
      assert(scaleExp <= INT16_MAX);
      valueStart[0].scaleExp = (int16_t)scaleExp;
      total = ldexp(1.0, scaleExp);
   }
   if (total > DBL_MAX)
      total = DBL_MAX;

   assert(graphHeight <= UINT16_MAX / 8);
   double maxDots = (double)(int32_t)(graphHeight * 8);
   int numDots = (int) ceil((sum / total) * maxDots);
   assert(numDots >= 0);
   if (sum > 0.0 && numDots <= 0) {
      numDots = 1; // Division of (sum / total) underflows
   }

   if (maxItems == 1) {
      assert(numDots <= UINT16_MAX);
      valueStart[isPercentChart ? 0 : 1].numDots = (uint16_t)numDots;
      return;
   }

   // Clear cells
   unsigned int y = ((unsigned int)numDots + 8 - 1) / 8; // Round up
   size_t i = GraphMeterMode_valueCellIndex(graphHeight, isPercentChart, 0, y, NULL, NULL);
   if (i < nCellsPerValue) {
      memset(&valueStart[i], 0, (nCellsPerValue - i) * sizeof(*valueStart));
   }

   if (sum <= 0.0)
      return;

   int deltaExp = 0;
   double scaledTotal = total;
   while (true) {
      numDots = (int) ceil((sum / scaledTotal) * maxDots);
      if (numDots <= 0) {
         numDots = 1; // Division of (sum / scaledTotal) underflows
      }

      GraphMeterMode_computeColors(this, context, valueStart, deltaExp, scaledTotal, (unsigned int)numDots);

      if (isPercentChart || !(scaledTotal < DBL_MAX) || (1U << deltaExp) >= graphHeight) {
         break;
      }

      deltaExp++;
      scaledTotal *= 2.0;
      if (scaledTotal > DBL_MAX) {
         scaledTotal = DBL_MAX;
      }
   }
}

static void GraphMeterMode_printScale(int exponent) {
   if (exponent < 10) {
      // "1" to "512"; the (exponent < 0) case is not implemented.
      assert(exponent >= 0);
      printw("%3u", 1U << exponent);
   } else if (exponent > (int)ARRAYSIZE(unitPrefixes) * 10 + 6) {
      addstr("inf");
   } else if (exponent % 10 < 7) {
      // "1K" to "64K", "1M" to "64M", "1G" to "64G", etc.
      printw("%2u%c", 1U << (exponent % 10), unitPrefixes[exponent / 10 - 1]);
   } else {
      // "M/8" (=128K), "M/4" (=256K), "M/2" (=512K), "G/8" (=128M), etc.
      printw("%c/%u", unitPrefixes[exponent / 10], 1U << (10 - exponent % 10));
   }
}

static uint8_t GraphMeterMode_scaleCellDetails(uint8_t details, unsigned int scaleFactor) {
   // Only the "top cell" of a record may need scaling like this; the cell does
   // not use the special meaning of bit 4.
   // This algorithm assumes the "details" be printed in braille characters.
   assert(scaleFactor > 0);
   if (scaleFactor < 2) {
      return details;
   }
   if (scaleFactor < 4 && (details & 0x0F) != 0x00) {
      // Display the cell in half height (bits 0 to 3 are zero).
      // Bits 4 and 5 are set simultaneously to avoid a jaggy visual.
      uint8_t newDetails = 0x30;
      // Bit 6
      if (popCount8(details) > 4)
         newDetails |= 0x40;
      // Bit 7 (equivalent to (details >= 0x80 || popCount8(details) > 6))
      if (details >= 0x7F)
         newDetails |= 0x80;
      return newDetails;
   }
   if (details != 0x00) {
      // Display the cell in a quarter height (bits 0 to 5 are zero).
      // Bits 6 and 7 are set simultaneously.
      return 0xC0;
   }
   return 0x00;
}

static int GraphMeterMode_lookupCell(const Meter* this, const GraphDrawContext* context, int scaleExp, size_t valueIndex, unsigned int y, uint8_t* details) {
   unsigned int graphHeight = (unsigned int)this->h;
   const GraphData* data = &this->drawData;

   uint8_t maxItems = context->maxItems;
   bool isPercentChart = context->isPercentChart;
   size_t nCellsPerValue = context->nCellsPerValue;

   // Reverse the coordinate
   assert(y < graphHeight);
   y = graphHeight - 1 - y;

   uint8_t itemIndex = (uint8_t)-1;
   *details = 0x00; // Empty the cell

   if (maxItems < 1)
      goto cellIsEmpty;

   assert(valueIndex < data->nValues);
   const GraphDataCell* valueStart = (const GraphDataCell*)data->buffer;
   valueStart = &valueStart[valueIndex * nCellsPerValue];

   int deltaExp = isPercentChart ? 0 : scaleExp - valueStart[0].scaleExp;
   assert(deltaExp >= 0);

   if (maxItems == 1) {
      unsigned int numDots = valueStart[isPercentChart ? 0 : 1].numDots;

      if (numDots < 1)
         goto cellIsEmpty;

      // Scale according to exponent difference. Round up.
      numDots = deltaExp < UINT16_WIDTH ? ((numDots - 1) >> deltaExp) : 0;
      numDots++;

      if (y > (numDots - 1) / 8)
         goto cellIsEmpty;

      itemIndex = 0;
      *details = 0xFF;
      if (y == (numDots - 1) / 8) {
         const uint8_t dotAlignment = 2;
         unsigned int blanksAtTopCell = (8 - 1 - (numDots - 1) % 8) / dotAlignment * dotAlignment;
         *details <<= blanksAtTopCell;
      }
   } else {
      int deltaExpArg = deltaExp >= UINT16_WIDTH ? UINT16_WIDTH - 1 : deltaExp;

      unsigned int scaleFactor;
      size_t i = GraphMeterMode_valueCellIndex(graphHeight, isPercentChart, deltaExpArg, y, &scaleFactor, NULL);
      if (i >= nCellsPerValue)
         goto cellIsEmpty;

      if (deltaExp >= UINT16_WIDTH) {
         // Any "scaleFactor" value greater than 8 behaves the same as 8 for the
         // "scaleCellDetails" function.
         scaleFactor = 8;
      }

      const GraphDataCell* cell = &valueStart[i];
      itemIndex = cell->c.itemNum - 1;
      *details = GraphMeterMode_scaleCellDetails(cell->c.details, scaleFactor);
   }
   /* fallthrough */

cellIsEmpty:
   if (y == 0)
      *details |= 0xC0;

   if (itemIndex == (uint8_t)-1)
      return BAR_SHADOW;

   assert(itemIndex < maxItems);
   return Meter_attributes(this)[itemIndex];
}

static void GraphMeterMode_printCellDetails(uint8_t details) {
   if (details == 0x00) {
      // Use ASCII space instead. A braille blank character may display as a
      // substitute block and is less distinguishable from a cell with data.
      addch(' ');
      return;
   }
#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8) {
      // Bits 3 and 4 of "details" might carry special meaning. When the whole
      // byte contains specific bit patterns, it indicates that only half cell
      // should be displayed in the ASCII display mode. The bits are supposed
      // to be filled in the Unicode display mode.
      if ((details & 0x9C) == 0x14 || (details & 0x39) == 0x28) {
         if (details == 0x14 || details == 0x28) { // Special case
            details = 0x18;
         } else {
            details |= 0x18;
         }
      }
      // Convert GraphDataCell.c.details bit representation to Unicode braille
      // dot ordering.
      //   (Bit0) a b (Bit3)  From:        h g f e d c b a (binary)
      //   (Bit1) c d (Bit4)               | | |  X   X  |
      //   (Bit2) e f (Bit5)               | | | | \ / | |
      //   (Bit6) g h (Bit7)               | | | |  X  | |
      //                      To: 0x2800 + h g f d b e c a
      // Braille Patterns [U+2800, U+28FF] in UTF-8: [E2 A0 80, E2 A3 BF]
      char sequence[4] = "\xE2\xA0\x80";
      // Bits 6 and 7 are in the second byte of the UTF-8 sequence.
      sequence[1] |= details >> 6;
      // Bits 0 to 5 are in the third byte.
      // The algorithm is optimized for x86 and ARM.
      uint32_t n = details * 0x01010101U;
      n = (uint32_t)((n & 0x08211204U) * 0x02110408U) >> 26;
      sequence[2] |= n;
      addstr(sequence);
      return;
   }
#endif
   // ASCII display mode
   const char upperHalf = '`';
   const char lowerHalf = '.';
   const char fullCell = ':';
   char c;

   // Detect special cases where we should print only half of the cell.
   if ((details & 0x9C) == 0x14) {
      c = upperHalf;
   } else if ((details & 0x39) == 0x28) {
      c = lowerHalf;
      // End of special cases
   } else if (popCount8(details) > 4) {
      c = fullCell;
   } else {
      // Determine which half has more dots than the other.
      uint8_t inverted = details ^ 0x0F;
      int difference = (int)popCount8(inverted) - 4;
      if (difference < 0) {
         c = upperHalf;
      } else if (difference > 0) {
         c = lowerHalf;
      } else {
         // Give weight to dots closer to the top or bottom of the cell (LSB or
         // MSB, respectively) as a tiebreaker.
         // Reverse bits 0 to 3 and subtract it from bits 4 to 7.
         // The algorithm is optimized for x86 and ARM.
         uint32_t n = inverted * 0x01010101U;
         n = (uint32_t)((n & 0xF20508U) * 0x01441080U) >> 27;
         difference = (int)n - 0x0F;
         c = difference < 0 ? upperHalf : lowerHalf;
      }
   }
   addch(c);
}

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {
   // Draw the caption
   const char* caption = Meter_getCaption(this);
   attrset(CRT_colors[METER_TEXT]);
   const int captionLen = 3;
   mvaddnstr(y, x, caption, captionLen);

   // Prepare parameters for drawing
   assert(this->h >= 1);
   assert(this->h <= MAX_GRAPH_HEIGHT);
   unsigned int graphHeight = (unsigned int)this->h;

   uint8_t maxItems = Meter_maxItems(this);
   bool isPercentChart = Meter_isPercentChart(this);
   size_t nCellsPerValue = maxItems <= 1 ? maxItems : graphHeight;
   if (!isPercentChart)
      nCellsPerValue *= 2;

   GraphDrawContext context = {
      .maxItems = maxItems,
      .isPercentChart = isPercentChart,
      .nCellsPerValue = nCellsPerValue
   };

   bool needsScaleDisplay = maxItems > 0 && graphHeight >= 2;
   if (needsScaleDisplay) {
      move(y + 1, x); // Cursor position for printing the scale
   }
   x += captionLen;
   w -= captionLen;

   GraphData* data = &this->drawData;

   // Expand the graph data buffer if necessary
   assert(data->nValues <= INT_MAX);
   if (w > (int)data->nValues && MAX_METER_GRAPHDATA_VALUES > data->nValues) {
      size_t nValues = data->nValues;
      nValues = MAXIMUM(nValues + nValues / 2, (size_t)w);
      nValues = MINIMUM(nValues, MAX_METER_GRAPHDATA_VALUES);
      GraphMeterMode_reallocateGraphBuffer(this, &context, nValues);
   }

   const size_t nValues = data->nValues;
   if (nValues < 1)
      return;

   // Record new value if necessary
   const Machine* host = this->host;
   if (!timercmp(&host->realtime, &(data->time), <)) {
      int globalDelay = host->settings->delay;
      struct timeval delay = { .tv_sec = globalDelay / 10, .tv_usec = (globalDelay % 10) * 100000L };
      timeradd(&host->realtime, &delay, &(data->time));

      GraphMeterMode_recordNewValue(this, &context);
   }

   if (w <= 0)
      return;

   // Starting positions of graph data and terminal column
   if ((size_t)w > nValues) {
      x += w - nValues;
      w = nValues;
   }
   size_t i = nValues - (size_t)w;

   // Determine and print the graph scale
   int scaleExp = 0;
   if (maxItems > 0 && !isPercentChart) {
      for (unsigned int col = 0; i + col < nValues; col++) {
         const GraphDataCell* valueStart = (const GraphDataCell*)data->buffer;
         valueStart = &valueStart[(i + col) * nCellsPerValue];
         if (scaleExp < valueStart[0].scaleExp) {
            scaleExp = valueStart[0].scaleExp;
         }
      }
   }
   if (needsScaleDisplay) {
      if (isPercentChart) {
         addstr("  %");
      } else {
         GraphMeterMode_printScale(scaleExp);
      }
   }

   // Draw the actual graph
   for (unsigned int line = 0; line < graphHeight; line++) {
      for (unsigned int col = 0; i + col < nValues; col++) {
         uint8_t details;
         int colorIdx = GraphMeterMode_lookupCell(this, &context, scaleExp, i + col, line, &details);
         move(y + (int)line, x + (int)col);
         attrset(CRT_colors[colorIdx]);
         GraphMeterMode_printCellDetails(details);
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- LEDMeterMode ---------- */

static const char* const LEDMeterMode_digitsAscii[] = {
   " __ ", "    ", " __ ", " __ ", "    ", " __ ", " __ ", " __ ", " __ ", " __ ",
   "|  |", "   |", " __|", " __|", "|__|", "|__ ", "|__ ", "   |", "|__|", "|__|",
   "|__|", "   |", "|__ ", " __|", "   |", " __|", "|__|", "   |", "|__|", " __|"
};

#ifdef HAVE_LIBNCURSESW

static const char* const LEDMeterMode_digitsUtf8[] = {
   "┌──┐", "  ┐ ", "╶──┐", "╶──┐", "╷  ╷", "┌──╴", "┌──╴", "╶──┐", "┌──┐", "┌──┐",
   "│  │", "  │ ", "┌──┘", " ──┤", "└──┤", "└──┐", "├──┐", "   │", "├──┤", "└──┤",
   "└──┘", "  ╵ ", "└──╴", "╶──┘", "   ╵", "╶──┘", "└──┘", "   ╵", "└──┘", "╶──┘"
};

#endif

static const char* const* LEDMeterMode_digits;

static void LEDMeterMode_drawDigit(int x, int y, int n) {
   for (int i = 0; i < 3; i++)
      mvaddstr(y + i, x, LEDMeterMode_digits[i * 10 + n]);
}

static void LEDMeterMode_draw(Meter* this, int x, int y, int w) {
#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8)
      LEDMeterMode_digits = LEDMeterMode_digitsUtf8;
   else
#endif
      LEDMeterMode_digits = LEDMeterMode_digitsAscii;

   RichString_begin(out);
   Meter_displayBuffer(this, &out);

   int yText =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? y + 1 :
#endif
      y + 2;
   attrset(CRT_colors[LED_COLOR]);
   const char* caption = Meter_getCaption(this);
   mvaddstr(yText, x, caption);
   int xx = x + strlen(caption);
   int len = RichString_sizeVal(out);
   for (int i = 0; i < len; i++) {
      int c = RichString_getCharVal(out, i);
      if (c >= '0' && c <= '9') {
         if (xx - x + 4 > w)
            break;

         LEDMeterMode_drawDigit(xx, y, c - '0');
         xx += 4;
      } else {
         if (xx - x + 1 > w)
            break;
#ifdef HAVE_LIBNCURSESW
         const cchar_t wc = { .chars = { c, '\0' }, .attr = 0 }; /* use LED_COLOR from attrset() */
         mvadd_wch(yText, xx, &wc);
#else
         mvaddch(yText, xx, c);
#endif
         xx += 1;
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
   RichString_delete(&out);
}

static const MeterMode Meter_modes[] = {
   [0] = {
      .uiName = NULL,
      .h = 0,
      .draw = NULL,
   },
   [BAR_METERMODE] = {
      .uiName = "Bar",
      .h = 1,
      .draw = BarMeterMode_draw,
   },
   [TEXT_METERMODE] = {
      .uiName = "Text",
      .h = 1,
      .draw = TextMeterMode_draw,
   },
   [GRAPH_METERMODE] = {
      .uiName = "Graph",
      .h = DEFAULT_GRAPH_HEIGHT,
      .draw = GraphMeterMode_draw,
   },
   [LED_METERMODE] = {
      .uiName = "LED",
      .h = 3,
      .draw = LEDMeterMode_draw,
   },
};

/* Meter class and methods */

const MeterClass Meter_class = {
   .super = {
      .extends = Class(Object)
   }
};

Meter* Meter_new(const Machine* host, unsigned int param, const MeterClass* type) {
   Meter* this = xCalloc(1, sizeof(Meter));
   Object_setClass(this, type);
   this->h = 1;
   this->param = param;
   this->host = host;
   this->curItems = type->maxItems;
   this->curAttributes = NULL;
   this->values = type->maxItems ? xCalloc(type->maxItems, sizeof(double)) : NULL;
   this->total = type->total;
   this->caption = xStrdup(type->caption);
   if (Meter_initFn(this)) {
      Meter_init(this);
   }

   Meter_setMode(this, type->defaultMode);
   assert(this->mode > 0);
   return this;
}

/* Converts 'value' in kibibytes into a human readable string.
   Example output strings: "0K", "1023K", "98.7M" and "1.23G" */
int Meter_humanUnit(char* buffer, double value, size_t size) {
   size_t i = 0;

   assert(value >= 0.0);
   while (value >= ONE_K) {
      if (i >= ARRAYSIZE(unitPrefixes) - 1) {
         if (value > 9999.0) {
            return xSnprintf(buffer, size, "inf");
         }
         break;
      }

      value /= ONE_K;
      ++i;
   }

   int precision = 0;

   if (i > 0) {
      // Fraction digits for mebibytes and above
      precision = value <= 99.9 ? (value <= 9.99 ? 2 : 1) : 0;

      // Round up if 'value' is in range (99.9, 100) or (9.99, 10)
      if (precision < 2) {
         double limit = precision == 1 ? 10.0 : 100.0;
         if (value < limit) {
            value = limit;
         }
      }
   }

   return xSnprintf(buffer, size, "%.*f%c", precision, value, unitPrefixes[i]);
}

void Meter_delete(Object* cast) {
   if (!cast)
      return;

   Meter* this = (Meter*) cast;
   if (Meter_doneFn(this)) {
      Meter_done(this);
   }
   free(this->drawData.buffer);
   free(this->caption);
   free(this->values);
   free(this);
}

void Meter_setCaption(Meter* this, const char* caption) {
   free_and_xStrdup(&this->caption, caption);
}

void Meter_setMode(Meter* this, MeterModeId modeIndex) {
   if (modeIndex == this->mode) {
      assert(this->mode > 0);
      return;
   }

   uint32_t supportedModes = Meter_supportedModes(this);
   assert(supportedModes);
   assert(!(supportedModes & (1 << 0)));

   assert(LAST_METERMODE <= UINT32_WIDTH);
   if (modeIndex >= LAST_METERMODE || !(supportedModes & (1UL << modeIndex)))
      return;

   assert(modeIndex >= 1);
   if (Meter_updateModeFn(this)) {
      assert(Meter_drawFn(this));
      this->draw = Meter_drawFn(this);
      Meter_updateMode(this, modeIndex);
   } else {
      free(this->drawData.buffer);
      this->drawData.buffer = NULL;
      this->drawData.nValues = 0;

      const MeterMode* mode = &Meter_modes[modeIndex];
      this->draw = mode->draw;
      this->h = mode->h;
   }
   this->mode = modeIndex;
}

MeterModeId Meter_nextSupportedMode(const Meter* this) {
   uint32_t supportedModes = Meter_supportedModes(this);
   assert(supportedModes);

   assert(this->mode < UINT32_WIDTH);
   uint32_t modeMask = ((uint32_t)-1 << 1) << this->mode;
   uint32_t nextModes = supportedModes & modeMask;
   if (!nextModes) {
      nextModes = supportedModes;
   }

   return (MeterModeId)countTrailingZeros(nextModes);
}

ListItem* Meter_toListItem(const Meter* this, bool moving) {
   char mode[20];
   if (this->mode > 0) {
      xSnprintf(mode, sizeof(mode), " [%s]", Meter_modes[this->mode].uiName);
   } else {
      mode[0] = '\0';
   }
   char name[32];
   if (Meter_getUiNameFn(this))
      Meter_getUiName(this, name, sizeof(name));
   else
      xSnprintf(name, sizeof(name), "%s", Meter_uiName(this));
   char buffer[50];
   xSnprintf(buffer, sizeof(buffer), "%s%s", name, mode);
   ListItem* li = ListItem_new(buffer, 0);
   li->moving = moving;
   return li;
}

/* Blank meter */

static void BlankMeter_updateValues(Meter* this) {
   this->txtBuffer[0] = '\0';
}

static void BlankMeter_display(ATTR_UNUSED const Object* cast, ATTR_UNUSED RichString* out) {
}

static const int BlankMeter_attributes[] = {
   DEFAULT_COLOR
};

const MeterClass BlankMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = BlankMeter_display,
   },
   .updateValues = BlankMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = BlankMeter_attributes,
   .name = "Blank",
   .uiName = "Blank",
   .caption = ""
};
