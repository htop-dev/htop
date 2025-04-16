/*
htop - Meter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Meter.h"

#include <assert.h>
#include <limits.h> // IWYU pragma: keep
#include <math.h>
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


#ifndef UINT32_WIDTH
#define UINT32_WIDTH 32
#endif

#define GRAPH_HEIGHT 4 /* Unit: rows (lines) */

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

/* ---------- TextMeterMode ---------- */

static void TextMeterMode_draw(Meter* this, int x, int y, int w) {
   assert(x >= 0);
   assert(w <= INT_MAX - x);

   const char* caption = Meter_getCaption(this);
   if (w >= 1) {
      attrset(CRT_colors[METER_TEXT]);
      mvaddnstr(y, x, caption, w);
   }
   attrset(CRT_colors[RESET_COLOR]);

   int captionLen = strlen(caption);
   w -= captionLen;
   if (w < 1) {
      return;
   }
   x += captionLen;

   RichString_begin(out);
   Meter_displayBuffer(this, &out);
   RichString_printoffnVal(out, y, x, 0, w);
   RichString_delete(&out);
}

/* ---------- BarMeterMode ---------- */

static const char BarMeterMode_characters[] = "|#*@$%&.";

static void BarMeterMode_draw(Meter* this, int x, int y, int w) {
   assert(x >= 0);
   assert(w <= INT_MAX - x);

   // Draw the caption
   int captionLen = 3;
   const char* caption = Meter_getCaption(this);
   if (w >= captionLen) {
      attrset(CRT_colors[METER_TEXT]);
      mvaddnstr(y, x, caption, captionLen);
   }
   w -= captionLen;

   // Draw the bar borders
   if (w >= 1) {
      x += captionLen;
      attrset(CRT_colors[BAR_BORDER]);
      mvaddch(y, x, '[');
      w--;
      mvaddch(y, x + w, ']');
      w--;
   }

   if (w < 1) {
      goto end;
   }
   attrset(CRT_colors[RESET_COLOR]); // Clear the bold attribute
   x++;

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
   for (uint8_t i = 0; i < this->curItems; i++) {
      double value = this->values[i];
      if (isPositive(value) && this->total > 0.0) {
         value = MINIMUM(value, this->total);
         blockSizes[i] = ceil((value / this->total) * w);
         blockSizes[i] = MINIMUM(blockSizes[i], w - offset);
      } else {
         blockSizes[i] = 0;
      }
      int nextOffset = offset + blockSizes[i];
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
      RichString_printoffnVal(bar, y, x + offset, startPos + offset, blockSizes[i]);
      offset += blockSizes[i];
   }
   if (offset < w) {
      RichString_setAttrn(&bar, CRT_colors[BAR_SHADOW], startPos + offset, w - offset);
      RichString_printoffnVal(bar, y, x + offset, startPos + offset, w - offset);
   }

   RichString_delete(&bar);

   move(y, x + w + 1);

end:
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

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {
   assert(x >= 0);
   assert(w <= INT_MAX - x);

   // Draw the caption
   const int captionLen = 3;
   const char* caption = Meter_getCaption(this);
   if (w >= captionLen) {
      attrset(CRT_colors[METER_TEXT]);
      mvaddnstr(y, x, caption, captionLen);
   }
   w -= captionLen;

   GraphData* data = &this->drawData;

   // Expand the graph data buffer if necessary
   assert(data->nValues / 2 <= INT_MAX);
   if (w > (int)(data->nValues / 2) && MAX_METER_GRAPHDATA_VALUES > data->nValues) {
      size_t oldNValues = data->nValues;
      data->nValues = MAXIMUM(oldNValues + oldNValues / 2, (size_t)w * 2);
      data->nValues = MINIMUM(data->nValues, MAX_METER_GRAPHDATA_VALUES);
      data->values = xReallocArray(data->values, data->nValues, sizeof(*data->values));
      memmove(data->values + (data->nValues - oldNValues), data->values, oldNValues * sizeof(*data->values));
      memset(data->values, 0, (data->nValues - oldNValues) * sizeof(*data->values));
   }

   const size_t nValues = data->nValues;
   if (nValues < 1)
      goto end;

   // Record new value if necessary
   const Machine* host = this->host;
   if (!timercmp(&host->realtime, &(data->time), <)) {
      int globalDelay = host->settings->delay;
      struct timeval delay = { .tv_sec = globalDelay / 10, .tv_usec = (globalDelay % 10) * 100000L };
      timeradd(&host->realtime, &delay, &(data->time));

      memmove(&data->values[0], &data->values[1], (nValues - 1) * sizeof(*data->values));

      data->values[nValues - 1] = 0.0;
      if (this->curItems > 0) {
         assert(this->values);
         data->values[nValues - 1] = sumPositiveValues(this->values, this->curItems);
      }
   }

   if (w < 1) {
      goto end;
   }
   x += captionLen;

   // Graph drawing style (character set, etc.)
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

   // Starting positions of graph data and terminal column
   if ((size_t)w > nValues / 2) {
      x += w - nValues / 2;
      w = nValues / 2;
   }
   size_t i = nValues - (size_t)w * 2;

   // Draw the actual graph
   for (int col = 0; i < nValues - 1; i += 2, col++) {
      int pix = GraphMeterMode_pixPerRow * GRAPH_HEIGHT;
      double total = MAXIMUM(this->total, 1);
      int v1 = (int) lround(CLAMP(data->values[i] / total * pix, 1.0, pix));
      int v2 = (int) lround(CLAMP(data->values[i + 1] / total * pix, 1.0, pix));

      int colorIdx = GRAPH_1;
      for (int line = 0; line < GRAPH_HEIGHT; line++) {
         int line1 = CLAMP(v1 - (GraphMeterMode_pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, GraphMeterMode_pixPerRow);
         int line2 = CLAMP(v2 - (GraphMeterMode_pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, GraphMeterMode_pixPerRow);

         attrset(CRT_colors[colorIdx]);
         mvaddstr(y + line, x + col, GraphMeterMode_dots[line1 * (GraphMeterMode_pixPerRow + 1) + line2]);
         colorIdx = GRAPH_2;
      }
   }

end:
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
   assert(x >= 0);
   assert(w <= INT_MAX - x);

   int yText =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? y + 1 :
#endif
      y + 2;
   attrset(CRT_colors[LED_COLOR]);

   const char* caption = Meter_getCaption(this);
   if (w >= 1) {
      mvaddnstr(yText, x, caption, w);
   }

   int captionLen = strlen(caption);
   if (w <= captionLen) {
      goto end;
   }
   int xx = x + captionLen;

#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8)
      LEDMeterMode_digits = LEDMeterMode_digitsUtf8;
   else
#endif
      LEDMeterMode_digits = LEDMeterMode_digitsAscii;

   RichString_begin(out);
   Meter_displayBuffer(this, &out);

   int len = RichString_sizeVal(out);
   for (int i = 0; i < len; i++) {
      int c = RichString_getCharVal(out, i);
      if (c >= '0' && c <= '9') {
         if (xx > x + w - 4)
            break;

         LEDMeterMode_drawDigit(xx, y, c - '0');
         xx += 4;
      } else {
         if (xx > x + w - 1)
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
   RichString_delete(&out);

end:
   attrset(CRT_colors[RESET_COLOR]);
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
      .h = GRAPH_HEIGHT,
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
   free(this->drawData.values);
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
      free(this->drawData.values);
      this->drawData.values = NULL;
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
