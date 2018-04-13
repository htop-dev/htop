/*
htop - Meter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Meter.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"

#define GRAPH_HEIGHT 4 /* Unit: rows (lines) */

const MeterClass Meter_class = {
   .super = {
      .extends = Class(Object)
   }
};

Meter* Meter_new(const struct ProcessList_* pl, int param, const MeterClass* type) {
   Meter* this = xCalloc(1, sizeof(Meter));
   Object_setClass(this, type);
   this->h = 1;
   this->param = param;
   this->pl = pl;
   this->curItems = type->maxItems;
   this->curAttributes = NULL;
   this->values = type->maxItems ? xCalloc(type->maxItems, sizeof(double)) : NULL;
   this->total = type->total;
   this->caption = xStrdup(type->caption);
   if (Meter_initFn(this)) {
      Meter_init(this);
   }
   Meter_setMode(this, type->defaultMode);
   return this;
}

static const char* Meter_prefixes = "KMGTPEZY";

int Meter_humanUnit(char* buffer, unsigned long int value, size_t size) {
   const char* prefix = Meter_prefixes;
   unsigned long int powi = 1;
   unsigned int powj = 1, precision = 2;

   for (;;) {
      if (value / 1024 < powi)
         break;

      if (prefix[1] == '\0')
         break;

      powi *= 1024;
      ++prefix;
   }

   if (*prefix == 'K')
      precision = 0;

   for (; precision > 0; precision--) {
      powj *= 10;
      if (value / powi < powj)
         break;
   }

   return snprintf(buffer, size, "%.*f%c", precision, (double) value / powi, *prefix);
}

void Meter_delete(Object* cast) {
   if (!cast)
      return;

   Meter* this = (Meter*) cast;
   if (Meter_doneFn(this)) {
      Meter_done(this);
   }
   free(this->drawData);
   free(this->caption);
   free(this->values);
   free(this);
}

void Meter_setCaption(Meter* this, const char* caption) {
   free_and_xStrdup(&this->caption, caption);
}

static inline void Meter_displayBuffer(const Meter* this, const char* buffer, RichString* out) {
   if (Object_displayFn(this)) {
      Object_display(this, out);
   } else {
      RichString_writeWide(out, CRT_colors[Meter_attributes(this)[0]], buffer);
   }
}

void Meter_setMode(Meter* this, int modeIndex) {
   if (modeIndex > 0 && modeIndex == this->mode) {
      return;
   }

   if (!modeIndex) {
      modeIndex = 1;
   }

   assert(modeIndex < LAST_METERMODE);
   if (Meter_defaultMode(this) == CUSTOM_METERMODE) {
      this->draw = Meter_drawFn(this);
      if (Meter_updateModeFn(this)) {
         Meter_updateMode(this, modeIndex);
      }
   } else {
      assert(modeIndex >= 1);
      free(this->drawData);
      this->drawData = NULL;

      const MeterMode* mode = Meter_modes[modeIndex];
      this->draw = mode->draw;
      this->h = mode->h;
   }
   this->mode = modeIndex;
}

ListItem* Meter_toListItem(const Meter* this, bool moving) {
   char mode[20];
   if (this->mode) {
      xSnprintf(mode, sizeof(mode), " [%s]", Meter_modes[this->mode]->uiName);
   } else {
      mode[0] = '\0';
   }
   char number[10];
   if (this->param > 0) {
      xSnprintf(number, sizeof(number), " %d", this->param);
   } else {
      number[0] = '\0';
   }
   char buffer[50];
   xSnprintf(buffer, sizeof(buffer), "%s%s%s", Meter_uiName(this), number, mode);
   ListItem* li = ListItem_new(buffer, 0);
   li->moving = moving;
   return li;
}

/* ---------- GraphData ---------- */

static GraphData* GraphData_new(size_t nValues, size_t nItems, bool isPercentGraph) {
   // colors[] is designed to be two-dimensional, but without a column of row
   // pointers (unlike var[m][n] declaration).
   // GraphMeterMode_draw() will print this table in 90-deg counter-clockwise
   // rotated form.
   unsigned int colorRowSize;
   if (nItems <= 1) { // 1 or less item
      colorRowSize = 1;
   } else if (isPercentGraph) { // Percent graph: a linear row of color cells.
      colorRowSize = GRAPH_HEIGHT;
   } else { // Non-percentage & dynamic scale: a binary tree of cells.
      colorRowSize = 2 * GRAPH_HEIGHT;
   }
   GraphData* data = xCalloc(1, sizeof(GraphData) +
                                sizeof(double) * nValues +
                                sizeof(double) * (nItems + 2) * 2 +
                                sizeof(int) * (nValues * colorRowSize));
   data->values = (double*)(data + 1);
   // values[nValues + 1]
   // It's intentional that values[nValues] and stack1[0] share the same cell;
   // the cell's value will be always 0.0.
   data->stack1 = (double*)(data->values + nValues);
   // stack1[nItems + 2]
   data->stack2 = (double*)(data->stack1 + (nItems + 2));
   // stack2[nItems + 2]
   data->colors = (int*)(data->stack2 + (nItems + 2));
   // colors[nValues * colorRowSize]

   // Initialize colors[].
   data->colorRowSize = colorRowSize;
   for (unsigned int i = 0; i < (nValues * colorRowSize); i++) {
      data->colors[i] = BAR_SHADOW;
   }
   return data;
}

static int GraphData_getColor(GraphData* this, int vIndex, int h, int scaleExp) {
   // level step  _________index_________  (tree form)
   //     3    8 |_______________8_______|
   //     2    4 |_______4_______|___10__|
   //     1    2 |___2___|___6___|___10__|
   //     0    1 |_1_|_3_|_5_|_7_|_9_|_11|

   // level step  _________index_________  (linear form (percent graph or
   //     0    1 |_0_|_1_|_2_|_3_|_4_|_5_|  one item))

   if (this->colorRowSize > GRAPH_HEIGHT) {
      const int maxLevel = ((int) log2(GRAPH_HEIGHT - 1)) + 1;
      int exp;
      (void) frexp(MAXIMUM(this->values[vIndex], this->values[vIndex + 1]), &exp);
      int level = MINIMUM((scaleExp - exp), maxLevel);
      assert(level >= 0);
      if ((h << (level + 1)) + 1 >= this->colorRowSize) {
         return BAR_SHADOW;
      }
      for (int j = 1 << level; ; j >>= 1) {
         assert(j > 0);
         size_t offset = (h << (level + 1)) + j;
         if (offset < this->colorRowSize) {
            return this->colors[vIndex * this->colorRowSize + offset];
         }
      }
   } else if (this->colorRowSize == GRAPH_HEIGHT) {
      return this->colors[vIndex * this->colorRowSize + h];
   } else {
      assert(this->colorRowSize == 1);
      return this->colors[vIndex * this->colorRowSize];
   }
}

/* ---------- TextMeterMode ---------- */

static void TextMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[GRAPH_NUM_RECORDS];
   Meter_updateValues(this, buffer, sizeof(buffer));

   attrset(CRT_colors[METER_TEXT]);
   mvaddnstr(y, x, this->caption, w - 1);
   attrset(CRT_colors[RESET_COLOR]);

   int captionLen = strlen(this->caption);
   x += captionLen;
   w -= captionLen;
   if (w <= 0)
      return;

   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);
   RichString_printoffnVal(out, y, x, 0, w - 1);
   RichString_end(out);
}

/* ---------- BarMeterMode ---------- */

static const char BarMeterMode_characters[] = "|#*@$%&.";

static void BarMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[GRAPH_NUM_RECORDS];
   Meter_updateValues(this, buffer, sizeof(buffer));

   w -= 2;
   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, this->caption, captionLen);
   x += captionLen;
   w -= captionLen;
   attrset(CRT_colors[BAR_BORDER]);
   mvaddch(y, x, '[');
   mvaddch(y, x + MAXIMUM(w, 0), ']');
   attrset(CRT_colors[RESET_COLOR]);

   w--;
   x++;

   if (w < 1)
      return;

   // The text in the bar is right aligned;
   // Pad with maximal spaces and then calculate needed starting position offset
   RichString_begin(bar);
   RichString_appendChr(&bar, 0, ' ', w);
   RichString_appendWide(&bar, 0, buffer);
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
   int items = this->curItems;
   double total = this->total;
   if (total <= 0.0) {
      double sum = 0.0;
      for (int i = 0; i < items; i++) {
         sum += this->values[i];
      }
      if (sum > -(this->total)) {
         this->total = -sum;
      }
      total = -(this->total);
   }
   for (uint8_t i = 0; i < this->curItems; i++) {
      double value = this->values[i];
      value = CLAMP(value, 0.0, total);
      if (value > 0) {
         blockSizes[i] = ceil((value / total) * w);
      } else {
         blockSizes[i] = 0;
      }
      int nextOffset = offset + blockSizes[i];
      // (Control against invalid values)
      nextOffset = CLAMP(nextOffset, 0, w);
      for (int j = offset; j < nextOffset; j++)
         if (RichString_getCharVal(bar, startPos + j) == ' ') {
            if (CRT_colorScheme == COLORSCHEME_MONOCHROME) {
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

   RichString_end(bar);

   move(y, x + w + 1);
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- GraphMeterMode ---------- */

#ifdef HAVE_LIBNCURSESW

#define PIXPERROW_UTF8 4
static const char* const GraphMeterMode_dotsUtf8[] = {
   /*00*/" ", /*01*/"⢀", /*02*/"⢠", /*03*/"⢰", /*04*/ "⢸",
   /*10*/"⡀", /*11*/"⣀", /*12*/"⣠", /*13*/"⣰", /*14*/ "⣸",
   /*20*/"⡄", /*21*/"⣄", /*22*/"⣤", /*23*/"⣴", /*24*/ "⣼",
   /*30*/"⡆", /*31*/"⣆", /*32*/"⣦", /*33*/"⣶", /*34*/ "⣾",
   /*40*/"⡇", /*41*/"⣇", /*42*/"⣧", /*43*/"⣷", /*44*/ "⣿"
};

#endif

#define PIXPERROW_ASCII 2
static const char* const GraphMeterMode_dotsAscii[] = {
   /*00*/" ", /*01*/".", /*02*/":",
   /*10*/".", /*11*/".", /*12*/":",
   /*20*/":", /*21*/":", /*22*/":"
};

static void GraphMeterMode_printScale(unsigned int scaleExp) {
   const char* divisors = "842";
   if (scaleExp > 86) { // > 99 yotta
      return;
   } else if (scaleExp < 10) { // <= 512
      printw("%3d", 1 << scaleExp);
      return;
   } else if (scaleExp % 10 <= 6) { // {1|2|4|8|16|32|64}{K|M|G|T|P|E|Z|Y}
      printw("%2d%c", 1 << (scaleExp % 10),
                      Meter_prefixes[(scaleExp / 10) - 1]);
      return;
   } else {
      // Output like "128K" is more than 3 chars. We express in fractions like
      // "M/8" instead. Likewise for "G/4" (=256M), "T/2" (=512G) etc.
      printw("%c/%c", Meter_prefixes[(scaleExp / 10)],
                      divisors[(scaleExp % 10) - 7]);
      return;
   }
}

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {
   bool isPercentGraph = (this->total > 0.0);
   if (!this->drawData) {
      this->drawData = (void*) GraphData_new(GRAPH_NUM_RECORDS,
                                             Meter_getMaxItems(this),
                                             isPercentGraph);
   }
   GraphData* data = this->drawData;

   const char* const* GraphMeterMode_dots;
   int GraphMeterMode_pixPerRow;
#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8) {
      GraphMeterMode_dots = GraphMeterMode_dotsUtf8;
      GraphMeterMode_pixPerRow = PIXPERROW_UTF8;
   } else
#endif
   {
      GraphMeterMode_dots = GraphMeterMode_dotsAscii;
      GraphMeterMode_pixPerRow = PIXPERROW_ASCII;
   }

   struct timeval now;
   gettimeofday(&now, NULL);
   if (!timercmp(&now, &(data->time), <)) {
      int globalDelay = this->pl->settings->delay;
      struct timeval delay = { .tv_sec = globalDelay / 10, .tv_usec = (globalDelay - ((globalDelay / 10) * 10)) * 100000 };
      timeradd(&now, &delay, &(data->time));

      for (int i = 0; i < GRAPH_NUM_RECORDS - 1; i++) {
         data->values[i] = data->values[i + 1];
         memcpy(&(data->colors[i * data->colorRowSize]),
                &(data->colors[(i + 1) * data->colorRowSize]),
                sizeof(*data->colors) * data->colorRowSize);
      }

      char buffer[GRAPH_NUM_RECORDS];
      Meter_updateValues(this, buffer, sizeof(buffer));

      int items = this->curItems;

      double *stack1 = data->stack1;
      double *stack2 = data->stack2;
      stack1[0] = stack2[0] = 0.0;
      for (int i = 0; i < items; i++) {
         stack1[i + 1] = stack2[i + 1];
         stack2[i + 1] = stack2[i] + this->values[i];
      }
      // May not assume this->total be constant. (Example: Swap meter when user
      // does swapon/swapoff.)
      stack1[items + 1] = stack2[items + 1];
      stack2[items + 1] = this->total;
      data->values[GRAPH_NUM_RECORDS - 1] = stack2[items];
      double scale;
      if (isPercentGraph) {
         data->values[GRAPH_NUM_RECORDS - 1] /= this->total;
         if (stack1[items + 1] > 0) {
            scale = this->total / stack1[items + 1];
            for (int i = 0; i < items; i++) {
               stack1[i + 1] = stack1[i + 1] * scale;
            }
         }
         scale = this->total;
      } else {
         int exp;
         (void) frexp(MAXIMUM(stack1[items], stack2[items]), &exp);
         scale = ldexp(1.0, exp);
      }

      // Determine the dominant color per cell in the graph.
      // O(GRAPH_HEIGHT + items) (linear time)
      for (int step = 1; ; step <<= 1) {
         int stack1Start = 0, stack2Start = 0;
         double low, high = 0.0;
         for (int h = 0; ; h += step) {
            size_t offset = (data->colorRowSize <= GRAPH_HEIGHT) ? h : (h * 2) + step;
            if (offset >= data->colorRowSize)
               break;

            low = high;
            high = scale * (h + step) / GRAPH_HEIGHT;
            assert(low >= scale * (h) / GRAPH_HEIGHT);

            double maxArea = 0.0;
            int color = BAR_SHADOW;
            for (int i = MINIMUM(stack1Start, stack2Start); ; i++) {
               if (stack1[i] < high)
                  stack1Start = i;
               if (stack2[i] < high)
                  stack2Start = i;
               if (i >= items)
                  break; // No more items
               if (stack1[i] >= high && stack2[i] >= high) {
                  // This cell is finished. Rest of values are out of this.
                  break;
               }
               // Skip items that have no area in this cell.
               if (stack1[i] >= high)
                  i = MAXIMUM(i, stack2Start);
               if (stack2[i] >= high)
                  i = MAXIMUM(i, stack1Start);
               double area;
               area = CLAMP(stack1[i + 1], low, high) +
                      CLAMP(stack2[i + 1], low, high);
               area -= (CLAMP(stack1[i], low, high) +
                        CLAMP(stack2[i], low, high));
               if (area > maxArea) {
                  maxArea = area;
                  color = Meter_attributes(this)[i];
               }
            }
            data->colors[(GRAPH_NUM_RECORDS - 2) * data->colorRowSize + offset] = color;
         }
         if (data->colorRowSize <= GRAPH_HEIGHT) {
            break;
         } else if (step >= 2 * GRAPH_HEIGHT) {
            break;
         }
      }
   }

   // How many values (and columns) we can draw for this graph.
   const int captionLen = 3;
   w -= captionLen;
   int index = GRAPH_NUM_RECORDS - (w * 2) + 2;
   int col = 0;
   if (index < 0) {
      col = -index / 2;
      index = 0;
   }

   // If it's not percent graph, determine the scale.
   int exp = 0;
   double scale = 1.0;
   if (this->total < 0.0) {
      double max = 1.0 - (DBL_EPSILON / FLT_RADIX); // For minimum scale 1.0.
      for (int j = GRAPH_NUM_RECORDS - 1; j >= index; j--) {
         max = (data->values[j] > max) ? data->values[j] : max;
      }
      (void) frexp(max, &exp);
      scale = ldexp(1.0, exp);
   }

   // Print caption and scale
   move(y, x);
   attrset(CRT_colors[METER_TEXT]);
   if (GRAPH_HEIGHT > 1 && this->total < 0.0) {
      GraphMeterMode_printScale(exp);
      move(y + 1, x);
   }
   addnstr(this->caption, captionLen);
   x += captionLen;

   // Print the graph
   for (; index < GRAPH_NUM_RECORDS - 1; index += 2, col++) {
      int pix = GraphMeterMode_pixPerRow * GRAPH_HEIGHT;
      int v1 = CLAMP((int) lround(data->values[index] / scale * pix), 1, pix);
      int v2 = CLAMP((int) lround(data->values[index + 1] / scale * pix), 1, pix);

      // Vertical bars from bottom up
      for (int h = 0; h < GRAPH_HEIGHT; h++) {
         int line = GRAPH_HEIGHT - 1 - h;
         int col1 = CLAMP(v1 - (GraphMeterMode_pixPerRow * h), 0, GraphMeterMode_pixPerRow);
         int col2 = CLAMP(v2 - (GraphMeterMode_pixPerRow * h), 0, GraphMeterMode_pixPerRow);
         attrset(CRT_colors[GraphData_getColor(data, index, h, exp)]);
         mvaddstr(y + line, x + col, GraphMeterMode_dots[col1 * (GraphMeterMode_pixPerRow + 1) + col2]);
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
   "└──┘", "  ╵ ", "└──╴", "╶──┘", "   ╵", "╶──┘", "└──┘", "   ╵", "└──┘", " ──┘"
};

#endif

static const char* const* LEDMeterMode_digits;

static void LEDMeterMode_drawDigit(int x, int y, int n) {
   for (int i = 0; i < 3; i++)
      mvaddstr(y+i, x, LEDMeterMode_digits[i * 10 + n]);
}

static void LEDMeterMode_draw(Meter* this, int x, int y, int w) {
   (void) w;

#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8)
      LEDMeterMode_digits = LEDMeterMode_digitsUtf8;
   else
#endif
      LEDMeterMode_digits = LEDMeterMode_digitsAscii;

   char buffer[GRAPH_NUM_RECORDS];
   Meter_updateValues(this, buffer, sizeof(buffer));

   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);

   int yText =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? y + 1 :
#endif
      y + 2;
   attrset(CRT_colors[LED_COLOR]);
   mvaddstr(yText, x, this->caption);
   int xx = x + strlen(this->caption);
   int len = RichString_sizeVal(out);
   for (int i = 0; i < len; i++) {
      int c = RichString_getCharVal(out, i);
      if (c >= '0' && c <= '9') {
         LEDMeterMode_drawDigit(xx, y, c - '0');
         xx += 4;
      } else {
#ifdef HAVE_LIBNCURSESW
         out.chptr[i].attr = 0; /* use LED_COLOR from attrset() */
         mvadd_wch(yText, xx, &out.chptr[i]);
#else
         mvaddch(yText, xx, c);
#endif
         xx += 1;
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
   RichString_end(out);
}

static MeterMode BarMeterMode = {
   .uiName = "Bar",
   .h = 1,
   .draw = BarMeterMode_draw,
};

static MeterMode TextMeterMode = {
   .uiName = "Text",
   .h = 1,
   .draw = TextMeterMode_draw,
};

static MeterMode GraphMeterMode = {
   .uiName = "Graph",
   .h = GRAPH_HEIGHT,
   .draw = GraphMeterMode_draw,
};

static MeterMode LEDMeterMode = {
   .uiName = "LED",
   .h = 3,
   .draw = LEDMeterMode_draw,
};

const MeterMode* const Meter_modes[] = {
   NULL,
   &BarMeterMode,
   &TextMeterMode,
   &GraphMeterMode,
   &LEDMeterMode,
   NULL
};

/* Blank meter */

static void BlankMeter_updateValues(ATTR_UNUSED Meter* this, char* buffer, size_t size) {
   if (size > 0) {
      *buffer = 0;
   }
}

static void BlankMeter_display(ATTR_UNUSED const Object* cast, RichString* out) {
   RichString_prune(out);
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
   .maxItems = 0,
   .total = 1.0,
   .attributes = BlankMeter_attributes,
   .name = "Blank",
   .uiName = "Blank",
   .caption = ""
};
