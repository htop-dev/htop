/*
htop - Meter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Meter.h"

#include <assert.h>
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

Meter* Meter_new(const struct ProcessList_* pl, unsigned int param, const MeterClass* type) {
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

int Meter_humanUnit(char* buffer, unsigned long int value, size_t size) {
   const char* prefix = "KMGTPEZY";
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

static inline void Meter_displayBuffer(const Meter* this, RichString* out) {
   if (Object_displayFn(this)) {
      Object_display(this, out);
   } else {
      RichString_writeWide(out, CRT_colors[Meter_attributes(this)[0]], this->txtBuffer);
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
   const char* caption = Meter_getCaption(this);
   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, caption, captionLen);
   x += captionLen;
   w -= captionLen;
   attrset(CRT_colors[BAR_BORDER]);
   mvaddch(y, x, '[');
   w--;
   mvaddch(y, x + MAXIMUM(w, 0), ']');
   w--;
   attrset(CRT_colors[RESET_COLOR]);

   x++;

   if (w < 1)
      return;

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
   int blockSizeSum = 0;

   // First draw in the bar[] buffer...
   int offset = 0;
   for (uint8_t i = 0; i < this->curItems; i++) {
      double value = this->values[i];
      value = CLAMP(value, 0.0, this->total);
      if (value > 0) {
         blockSizes[i] = ceil((value / this->total) * w);
      } else {
         blockSizes[i] = 0;
      }

      if (Meter_comprisedValues(this)) {
         blockSizes[i] = MAXIMUM(blockSizes[i] - blockSizeSum, 0);
         blockSizeSum += blockSizes[i];
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

   RichString_delete(&bar);

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

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {
   const ProcessList* pl = this->pl;

   if (!this->drawData) {
      this->drawData = xCalloc(1, sizeof(GraphData));
   }
   GraphData* data = this->drawData;
   const int nValues = METER_GRAPHDATA_SIZE;

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

   const char* caption = Meter_getCaption(this);
   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, caption, captionLen);
   x += captionLen;
   w -= captionLen;

   if (!timercmp(&pl->realtime, &(data->time), <)) {
      int globalDelay = this->pl->settings->delay;
      struct timeval delay = { .tv_sec = globalDelay / 10, .tv_usec = (globalDelay % 10) * 100000L };
      timeradd(&pl->realtime, &delay, &(data->time));

      for (int i = 0; i < nValues - 1; i++)
         data->values[i] = data->values[i + 1];

      if (Meter_comprisedValues(this)) {
         data->values[nValues - 1] = (this->curItems > 0) ? this->values[this->curItems - 1] : 0.0;
      } else {
         double value = 0.0;
         for (uint8_t i = 0; i < this->curItems; i++)
            value += !isnan(this->values[i]) ? this->values[i] : 0;
         data->values[nValues - 1] = value;
      }
   }

   int i = nValues - (w * 2), k = 0;
   if (i < 0) {
      k = -i / 2;
      i = 0;
   }
   for (; i < nValues - 1; i += 2, k++) {
      int pix = GraphMeterMode_pixPerRow * GRAPH_HEIGHT;
      if (this->total < 1)
         this->total = 1;
      int v1 = CLAMP((int) lround(data->values[i] / this->total * pix), 1, pix);
      int v2 = CLAMP((int) lround(data->values[i + 1] / this->total * pix), 1, pix);

      int colorIdx = GRAPH_1;
      for (int line = 0; line < GRAPH_HEIGHT; line++) {
         int line1 = CLAMP(v1 - (GraphMeterMode_pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, GraphMeterMode_pixPerRow);
         int line2 = CLAMP(v2 - (GraphMeterMode_pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, GraphMeterMode_pixPerRow);

         attrset(CRT_colors[colorIdx]);
         mvaddstr(y + line, x + k, GraphMeterMode_dots[line1 * (GraphMeterMode_pixPerRow + 1) + line2]);
         colorIdx = GRAPH_2;
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
   .maxItems = 0,
   .total = 100.0,
   .attributes = BlankMeter_attributes,
   .name = "Blank",
   .uiName = "Blank",
   .caption = ""
};
