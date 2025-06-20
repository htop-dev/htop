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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>

#include "Macros.h"
#include "Panel.h"
#include "ProvideCurses.h"


static void CommandScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAXIMUM(Panel_getSelectedIndex(panel), 0);
   Panel_prune(panel);

   const char* p = Process_getCommand(this->process);
   char line[COLS + 1];
   int line_offset = 0, line_size = 0, last_spc_cols = 0, last_spc_offset = -1;
   size_t i = 0, total = strlen(p);

   if (CRT_utf8) {
      mbstate_t state;
      memset(&state, 0, sizeof(state));
      int line_cols = 0;
      while (i < total) {
         assert(line_offset >= 0 && (size_t)line_offset < sizeof(line));
         wchar_t wc;
         size_t bytes = mbrtowc(&wc, p + i, total - i, &state);
         int width = wcwidth(wc);
         if (p[i] == ' ') {
            last_spc_offset = line_offset;
            last_spc_cols = line_cols;
         }

         if (bytes == (size_t)-1 || bytes == (size_t)-2) {
            wc = L'\xFFFD';
            bytes = 1;
         }

         memcpy(line + line_offset, p + i, bytes);
         i += bytes;
         line_offset += bytes;
         line_cols += width;
         if (line_cols < COLS) {
            continue;
         }

         if (last_spc_offset >= 0) {
            line_size = last_spc_offset;
            line_cols -= last_spc_cols;
            last_spc_offset = -1;
            last_spc_cols = 0;
         } else if (line_cols > COLS) {
            line_size = line_offset - (int) bytes;
            line_cols = width;
         } else {
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
   } else {
      for (; i < total; i++, line_offset++) {
         assert(line_offset >= 0 && (size_t)line_offset < sizeof(line));
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
   }

   if (line_offset > 0) {
      line[line_offset] = '\0';
      InfoScreen_addLine(this, line);
   }

   Panel_setSelected(panel, idx);
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
