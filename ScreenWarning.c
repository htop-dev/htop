/*
htop - ScreenWarning.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ScreenWarning.h"

#include "CRT.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


#define WARNING_TIMEOUT 5000


static char* warnMsg;
static int warnMsgLen;
static uint64_t warnTime;
static bool shown;

void ScreenWarning_add(const char* fmt, ...) {
   free(warnMsg);

   va_list vl;
   va_start(vl, fmt);
   int r = vasprintf(&warnMsg, fmt, vl);
   va_end(vl);

   assert(r >= 0);
   assert(*warnMsg);
   warnMsgLen = r;

   shown = false;
}

#define DIV_ROUNDUP(a, b) (assert((b) != 0), ((a) + ((b) - 1)) / (b))

static void draw(void) {
   const int width_padding = 2 * 5;  // " [!] "
   const int box_width = MINIMUM(warnMsgLen + width_padding, MINIMUM(4 * COLS / 5, 80));
   const int box_height = 2 + DIV_ROUNDUP(warnMsgLen, box_width - width_padding);

   const int col_start = (COLS - box_width) / 2;
   const int line_start = LINES - box_height - (LINES < 20 ? 1 : 2);

   RichString_begin(rs);

   RichString_writeAscii(&rs, CRT_colors[PROCESS_D_STATE], " [!]");
   for (int i = 0; i <= box_width - width_padding; i++) {
      switch (i % 3) {
         case 0:
         case 1:
            RichString_appendChr(&rs, 0, ' ', 1);
            break;
         case 2:
            RichString_appendChr(&rs, CRT_colors[METER_VALUE_NOTICE], '-', 1);
            break;
      }
   }
   RichString_appendChr(&rs, 0, ' ', 1);
   RichString_appendAscii(&rs, CRT_colors[PROCESS_D_STATE], "[!] ");
   RichString_printVal(rs, line_start, col_start);

   {
      const char* remainMsg = warnMsg;
      int remainMsgLen = warnMsgLen;
      for (int i = 0; i < box_height - 2; i++) {

         RichString_rewind(&rs, RichString_sizeVal(rs));

         RichString_writeAscii(&rs, CRT_colors[PROCESS_D_STATE], " [!]");
         RichString_appendChr(&rs, 0, ' ', 1);
         int charsToPrint = MINIMUM(remainMsgLen, box_width - width_padding);
         RichString_appendnAscii(&rs, CRT_colors[METER_VALUE_NOTICE], remainMsg, charsToPrint);
         remainMsg += charsToPrint;
         remainMsgLen -= charsToPrint;
         RichString_appendChr(&rs, 0, ' ', 1 + box_width - width_padding - charsToPrint);
         RichString_appendAscii(&rs, CRT_colors[PROCESS_D_STATE], "[!] ");

         RichString_printVal(rs, line_start + i + 1, col_start);
      }
      assert(*remainMsg == '\0');
      assert(remainMsgLen == 0);
   }

   RichString_rewind(&rs, RichString_sizeVal(rs));

   RichString_writeAscii(&rs, CRT_colors[PROCESS_D_STATE], " [!]");
   for (int i = 0; i <= box_width - width_padding; i++) {
      switch (i % 3) {
         case 0:
         case 1:
            RichString_appendChr(&rs, 0, ' ', 1);
            break;
         case 2:
            RichString_appendChr(&rs, CRT_colors[METER_VALUE_NOTICE], '-', 1);
            break;
      }
   }
   RichString_appendChr(&rs, 0, ' ', 1);
   RichString_appendAscii(&rs, CRT_colors[PROCESS_D_STATE], "[!] ");
   RichString_printVal(rs, line_start + box_height - 1, col_start);

   RichString_delete(&rs);
}

void ScreenWarning_display(void) {
   if (!warnMsg)
      return;

   uint64_t now;
   Platform_gettime_monotonic(&now);

   if (shown && now >= (warnTime + WARNING_TIMEOUT)) {
      free(warnMsg);
      warnMsg = NULL;
      return;
   }

   draw();

   if (!shown) {
      warnTime = now;
      shown = true;
   }
}
