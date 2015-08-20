/*
htop - Meter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

#include "RichString.h"
#include "Object.h"
#include "CRT.h"
#include "StringUtils.h"
#include "ListItem.h"
#include "Settings.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h>

#define METER_BUFFER_LEN 256

#define GRAPH_DELAY (DEFAULT_DELAY/2)

/*{
#include "ListItem.h"

#include <sys/time.h>

typedef struct Meter_ Meter;

typedef void(*Meter_Init)(Meter*);
typedef void(*Meter_Done)(Meter*);
typedef void(*Meter_UpdateMode)(Meter*, int);
typedef void(*Meter_SetValues)(Meter*, char*, int);
typedef void(*Meter_Draw)(Meter*, int, int, int);

typedef struct MeterClass_ {
   ObjectClass super;
   const Meter_Init init;
   const Meter_Done done;
   const Meter_UpdateMode updateMode;
   const Meter_Draw draw;
   const Meter_SetValues setValues;
   const int defaultMode;
   const double total;
   const int* attributes;
   const char* name;
   const char* uiName;
   const char* caption;
   const char* description;
   const char maxItems;
   char curItems;
} MeterClass;

#define As_Meter(this_)                ((MeterClass*)((this_)->super.klass))
#define Meter_initFn(this_)            As_Meter(this_)->init
#define Meter_init(this_)              As_Meter(this_)->init((Meter*)(this_))
#define Meter_done(this_)              As_Meter(this_)->done((Meter*)(this_))
#define Meter_updateModeFn(this_)      As_Meter(this_)->updateMode
#define Meter_updateMode(this_, m_)    As_Meter(this_)->updateMode((Meter*)(this_), m_)
#define Meter_drawFn(this_)            As_Meter(this_)->draw
#define Meter_doneFn(this_)            As_Meter(this_)->done
#define Meter_setValues(this_, c_, i_) As_Meter(this_)->setValues((Meter*)(this_), c_, i_)
#define Meter_defaultMode(this_)       As_Meter(this_)->defaultMode
#define Meter_getItems(this_)          As_Meter(this_)->curItems
#define Meter_setItems(this_, n_)      As_Meter(this_)->curItems = (n_)
#define Meter_attributes(this_)        As_Meter(this_)->attributes
#define Meter_name(this_)              As_Meter(this_)->name
#define Meter_uiName(this_)            As_Meter(this_)->uiName

struct Meter_ {
   Object super;
   Meter_Draw draw;
   
   char* caption;
   int mode;
   int param;
   void* drawData;
   int h;
   struct ProcessList_* pl;
   double* values;
   double total;
};

typedef struct MeterMode_ {
   Meter_Draw draw;
   const char* uiName;
   int h;
} MeterMode;

typedef enum {
   CUSTOM_METERMODE = 0,
   BAR_METERMODE,
   TEXT_METERMODE,
   GRAPH_METERMODE,
   LED_METERMODE,
   LAST_METERMODE
} MeterModeId;

typedef struct GraphData_ {
   struct timeval time;
   double values[METER_BUFFER_LEN];
} GraphData;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

MeterClass Meter_class = {
   .super = {
      .extends = Class(Object)
   }
};

Meter* Meter_new(struct ProcessList_* pl, int param, MeterClass* type) {
   Meter* this = calloc(1, sizeof(Meter));
   Object_setClass(this, type);
   this->h = 1;
   this->param = param;
   this->pl = pl;
   char maxItems = type->maxItems;
   if (maxItems == 0) {
      maxItems = 1;
   }
   type->curItems = maxItems;
   this->values = calloc(maxItems, sizeof(double));
   this->total = type->total;
   this->caption = strdup(type->caption);
   if (Meter_initFn(this))
      Meter_init(this);
   Meter_setMode(this, type->defaultMode);
   return this;
}

void Meter_delete(Object* cast) {
   if (!cast)
      return;
   Meter* this = (Meter*) cast;
   if (Meter_doneFn(this)) {
      Meter_done(this);
   }
   if (this->drawData)
      free(this->drawData);
   free(this->caption);
   free(this->values);
   free(this);
}

void Meter_setCaption(Meter* this, const char* caption) {
   free(this->caption);
   this->caption = strdup(caption);
}

static inline void Meter_displayBuffer(Meter* this, char* buffer, RichString* out) {
   if (Object_displayFn(this)) {
      Object_display(this, out);
   } else {
      RichString_write(out, CRT_colors[Meter_attributes(this)[0]], buffer);
   }
}

void Meter_setMode(Meter* this, int modeIndex) {
   if (modeIndex > 0 && modeIndex == this->mode)
      return;
   if (!modeIndex)
      modeIndex = 1;
   assert(modeIndex < LAST_METERMODE);
   if (Meter_defaultMode(this) == CUSTOM_METERMODE) {
      this->draw = Meter_drawFn(this);
      if (Meter_updateModeFn(this))
         Meter_updateMode(this, modeIndex);
   } else {
      assert(modeIndex >= 1);
      if (this->drawData)
         free(this->drawData);
      this->drawData = NULL;

      MeterMode* mode = Meter_modes[modeIndex];
      this->draw = mode->draw;
      this->h = mode->h;
   }
   this->mode = modeIndex;
}

ListItem* Meter_toListItem(Meter* this, bool moving) {
   char mode[21];
   if (this->mode)
      snprintf(mode, 20, " [%s]", Meter_modes[this->mode]->uiName);
   else
      mode[0] = '\0';
   char number[11];
   if (this->param > 0)
      snprintf(number, 10, " %d", this->param);
   else
      number[0] = '\0';
   char buffer[51];
   snprintf(buffer, 50, "%s%s%s", Meter_uiName(this), number, mode);
   ListItem* li = ListItem_new(buffer, 0);
   li->moving = moving;
   return li;
}

/* ---------- TextMeterMode ---------- */

static void TextMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[METER_BUFFER_LEN];
   Meter_setValues(this, buffer, METER_BUFFER_LEN - 1);
   (void) w;

   attrset(CRT_colors[METER_TEXT]);
   mvaddstr(y, x, this->caption);
   int captionLen = strlen(this->caption);
   x += captionLen;
   mvhline(y, x, ' ', CRT_colors[DEFAULT_COLOR]);
   attrset(CRT_colors[RESET_COLOR]);
   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);
   RichString_printVal(out, y, x);
   RichString_end(out);
}

/* ---------- BarMeterMode ---------- */

static char BarMeterMode_characters[] = "|#*@$%&";

static void BarMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[METER_BUFFER_LEN];
   Meter_setValues(this, buffer, METER_BUFFER_LEN - 1);

   w -= 2;
   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, this->caption, captionLen);
   x += captionLen;
   w -= captionLen;
   attrset(CRT_colors[BAR_BORDER]);
   mvaddch(y, x, '[');
   mvaddch(y, x + w, ']');
   
   w--;
   x++;

   if (w < 1) {
      attrset(CRT_colors[RESET_COLOR]);
      return;
   }
   char bar[w + 1];
   
   int blockSizes[10];
   for (int i = 0; i < w; i++)
      bar[i] = ' ';

   const size_t barOffset = w - MIN((int)strlen(buffer), w);
   snprintf(bar + barOffset, w - barOffset + 1, "%s", buffer);

   // First draw in the bar[] buffer...
   int offset = 0;
   int items = Meter_getItems(this);
   for (int i = 0; i < items; i++) {
      double value = this->values[i];
      value = MAX(value, 0);
      value = MIN(value, this->total);
      if (value > 0) {
         blockSizes[i] = ceil((value/this->total) * w);
      } else {
         blockSizes[i] = 0;
      }
      int nextOffset = offset + blockSizes[i];
      // (Control against invalid values)
      nextOffset = MIN(MAX(nextOffset, 0), w);
      for (int j = offset; j < nextOffset; j++)
         if (bar[j] == ' ') {
            if (CRT_colorScheme == COLORSCHEME_MONOCHROME) {
               bar[j] = BarMeterMode_characters[i];
            } else {
               bar[j] = '|';
            }
         }
      offset = nextOffset;
   }

   // ...then print the buffer.
   offset = 0;
   for (int i = 0; i < items; i++) {
      attrset(CRT_colors[Meter_attributes(this)[i]]);
      mvaddnstr(y, x + offset, bar + offset, blockSizes[i]);
      offset += blockSizes[i];
      offset = MAX(offset, 0);
      offset = MIN(offset, w);
   }
   if (offset < w) {
      attrset(CRT_colors[BAR_SHADOW]);
      mvaddnstr(y, x + offset, bar + offset, w - offset);
   }

   move(y, x + w + 1);
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- GraphMeterMode ---------- */

#ifdef HAVE_LIBNCURSESW

#define PIXPERROW_UTF8 4
static const char* GraphMeterMode_dotsUtf8[] = {
   /*00*/"⠀", /*01*/"⢀", /*02*/"⢠", /*03*/"⢰", /*04*/ "⢸",
   /*10*/"⡀", /*11*/"⣀", /*12*/"⣠", /*13*/"⣰", /*14*/ "⣸",
   /*20*/"⡄", /*21*/"⣄", /*22*/"⣤", /*23*/"⣴", /*24*/ "⣼",
   /*30*/"⡆", /*31*/"⣆", /*32*/"⣦", /*33*/"⣶", /*34*/ "⣾",
   /*40*/"⡇", /*41*/"⣇", /*42*/"⣧", /*43*/"⣷", /*44*/ "⣿"
};

#endif

#define PIXPERROW_ASCII 2
static const char* GraphMeterMode_dotsAscii[] = {
   /*00*/" ", /*01*/".", /*02*/":",
   /*10*/".", /*11*/".", /*12*/":",
   /*20*/":", /*21*/":", /*22*/":"
};

static const char** GraphMeterMode_dots;
static int GraphMeterMode_pixPerRow;

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {

   if (!this->drawData) this->drawData = calloc(1, sizeof(GraphData));
    GraphData* data = (GraphData*) this->drawData;
   const int nValues = METER_BUFFER_LEN;

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

   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, this->caption, captionLen);
   x += captionLen;
   w -= captionLen;
   
   struct timeval now;
   gettimeofday(&now, NULL);
   if (!timercmp(&now, &(data->time), <)) {
      struct timeval delay = { .tv_sec = (int)(CRT_delay/10), .tv_usec = (CRT_delay-((int)(CRT_delay/10)*10)) * 100000 };
      timeradd(&now, &delay, &(data->time));

      for (int i = 0; i < nValues - 1; i++)
         data->values[i] = data->values[i+1];
   
      char buffer[nValues];
      Meter_setValues(this, buffer, nValues - 1);
   
      double value = 0.0;
      int items = Meter_getItems(this);
      for (int i = 0; i < items; i++)
         value += this->values[i];
      value /= this->total;
      data->values[nValues - 1] = value;
   }
   
   for (int i = nValues - (w*2) + 2, k = 0; i < nValues; i+=2, k++) {
      const double dot = (1.0 / (GraphMeterMode_pixPerRow * 4));
      int v1 = MIN(GraphMeterMode_pixPerRow * 4, MAX(1, data->values[i] / dot));
      int v2 = MIN(GraphMeterMode_pixPerRow * 4, MAX(1, data->values[i+1] / dot));

      int colorIdx = GRAPH_1;
      for (int line = 0; line < 4; line++) {
         int line1 = MIN(GraphMeterMode_pixPerRow, MAX(0, v1 - (GraphMeterMode_pixPerRow * (3 - line))));
         int line2 = MIN(GraphMeterMode_pixPerRow, MAX(0, v2 - (GraphMeterMode_pixPerRow * (3 - line))));

         attrset(CRT_colors[colorIdx]);
         mvaddstr(y+line, x+k, GraphMeterMode_dots[line1 * (GraphMeterMode_pixPerRow + 1) + line2]);
         colorIdx = GRAPH_2;
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- LEDMeterMode ---------- */

static const char* LEDMeterMode_digitsAscii[] = {
   " __ ","    "," __ "," __ ","    "," __ "," __ "," __ "," __ "," __ ",
   "|  |","   |"," __|"," __|","|__|","|__ ","|__ ","   |","|__|","|__|",
   "|__|","   |","|__ "," __|","   |"," __|","|__|","   |","|__|"," __|"
};

#ifdef HAVE_LIBNCURSESW

static const char* LEDMeterMode_digitsUtf8[] = {
   "┌──┐","  ┐ ","╶──┐","╶──┐","╷  ╷","┌──╴","┌──╴","╶──┐","┌──┐","┌──┐",
   "│  │","  │ ","┌──┘"," ──┤","└──┤","└──┐","├──┐","   │","├──┤","└──┤",
   "└──┘","  ╵ ","└──╴","╶──┘","   ╵","╶──┘","└──┘","   ╵","└──┘"," ──┘"
};

#endif

static const char** LEDMeterMode_digits;

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

   char buffer[METER_BUFFER_LEN];
   Meter_setValues(this, buffer, METER_BUFFER_LEN - 1);
   
   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);

   int yText =
#ifdef HAVE_LIBNCURSESW
	   CRT_utf8 ? y+1 :
#endif
	   y+2;
   attrset(CRT_colors[LED_COLOR]);
   mvaddstr(yText, x, this->caption);
   int xx = x + strlen(this->caption);
   int len = RichString_sizeVal(out);
   for (int i = 0; i < len; i++) {
      char c = RichString_getCharVal(out, i);
      if (c >= '0' && c <= '9') {
         LEDMeterMode_drawDigit(xx, y, c-48);
         xx += 4;
      } else {
         mvaddch(yText, xx, c);
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
   .h = 4,
   .draw = GraphMeterMode_draw,
};

static MeterMode LEDMeterMode = {
   .uiName = "LED",
   .h = 3,
   .draw = LEDMeterMode_draw,
};

MeterMode* Meter_modes[] = {
   NULL,
   &BarMeterMode,
   &TextMeterMode,
   &GraphMeterMode,
   &LEDMeterMode,
   NULL
};

/* Blank meter */

static void BlankMeter_setValues(Meter* this, char* buffer, int size) {
   (void) this; (void) buffer; (void) size;
}

static void BlankMeter_display(Object* cast, RichString* out) {
   (void) cast;
   RichString_prune(out);
}

int BlankMeter_attributes[] = {
   DEFAULT_COLOR
};

MeterClass BlankMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = BlankMeter_display,
   },
   .setValues = BlankMeter_setValues,
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = BlankMeter_attributes,
   .name = "Blank",
   .uiName = "Blank",
   .caption = ""
};
