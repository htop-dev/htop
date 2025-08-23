/*
htop - CommandScreen.c
(C) 2017,2020 ryenus
(C) 2020,2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CommandScreen.h"

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "CRT.h"
#include "Macros.h"
#include "Panel.h"
#include "ProvideCurses.h"


static int CommandScreen_scanAscii(InfoScreen* this, const char* p, size_t total, char* line) {
   int line_offset = 0, line_size = 0, last_spc_offset = -1;
   for (size_t i = 0; i < total; i++, line_offset++) {
      assert(line_offset >= 0 && (size_t)line_offset <= total);
      char c = line[line_offset] = p[i];
      if (c == ' ') {
         last_spc_offset = line_offset;
      }

      if (line_offset == COLS) {
         line_size = (last_spc_offset == -1) ? line_offset : last_spc_offset;
         line[line_size] = '\0';
         InfoScreen_addLine(this, line);

         line_offset -= line_size;
         last_spc_offset = -1;
         memcpy(line, p + i - line_offset, line_offset + 1);
      }
   }

   return line_offset;
}

#ifdef HAVE_LIBNCURSESW

static int CommandScreen_scanWide(InfoScreen* this, const char* p, size_t total, char* line) {
   mbstate_t state;
   memset(&state, 0, sizeof(state));
   int line_cols = 0, line_offset = 0, line_size = 0, width = 1;
   int last_spc_cols = -1, last_spc_offset = -1;
   for (size_t i = 0, bytes = 1; i < total; bytes = 1, width = 1) {
      assert(line_offset >= 0 && (size_t)line_offset <= total);
      unsigned char c = (unsigned char)p[i];
      if (c < 0x80) { // skip mbrtowc for ASCII characters
         line[line_offset] = c;
         if (c == ' ') {
            last_spc_offset = line_offset;
            last_spc_cols = line_cols;
         }
      } else {
         wchar_t wc;
         bytes = mbrtowc(&wc, p + i, total - i, &state);
         if (bytes != (size_t)-1 && bytes != (size_t)-2) {
            width = wcwidth(wc);
            width = MAXIMUM(width, 1);
         } else {
            bytes = 1;
         }
         memcpy(line + line_offset, p + i, bytes);
      }

      i += bytes;
      line_offset += bytes;
      line_cols += width;
      if (line_cols < COLS) {
         continue;
      }

      if (last_spc_offset >= 0) {  // wrap by last space
         line_size = last_spc_offset;
         line_cols -= last_spc_cols;
         last_spc_offset = last_spc_cols = -1;
      } else if (line_cols > COLS) {  // wrap current wide char to next line
         line_size = line_offset - (int) bytes;
         line_cols = width;
      } else { // wrap by current character
         line_size = line_offset;
         line_cols = 0;
      }

      line[line_size] = '\0';
      InfoScreen_addLine(this, line);
      line_offset -= line_size;
      if (line_offset > 0) {
         memcpy(line, p + i - line_offset, line_offset + 1);
      }
   }

   return line_offset;
}

#endif // HAVE_LIBNCURSESW

static void CommandScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);

   const char* p = Process_getCommand(this->process);
   assert(p != NULL);
   size_t total = strlen(p);
   char line[total + 1];

   int line_offset =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? CommandScreen_scanWide(this, p, total, line) :
#endif
      CommandScreen_scanAscii(this, p, total, line);

   assert(line_offset >= 0 && (size_t)line_offset <= total);
   if (line_offset > 0) {
      line[line_offset] = '\0';
      InfoScreen_addLine(this, line);
   }

   Panel_setSelected(panel, MAXIMUM(idx, 0));
}

static void CommandScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Command of process %d - %s", Process_getPid(this->process), Process_getCommand(this->process));
}

const InfoScreenClass CommandScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = CommandScreen_delete
   },
   .scan = CommandScreen_scan,
   .draw = CommandScreen_draw
};

CommandScreen* CommandScreen_new(Process* process) {
   CommandScreen* this = AllocThis(CommandScreen);
   return (CommandScreen*) InfoScreen_init(&this->super, process, NULL, LINES - 2, " ");
}

void CommandScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}
