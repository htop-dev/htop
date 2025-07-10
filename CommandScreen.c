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
#include <string.h>

#include "Macros.h"
#include "Panel.h"
#include "ProvideCurses.h"


static void CommandScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAXIMUM(Panel_getSelectedIndex(panel), 0);
   Panel_prune(panel);

   const char* p = Process_getCommand(this->process);

   size_t line_maxlen = COLS < 40 ? 40 : COLS;
   size_t line_offset = 0;
   size_t last_space = 0;
   char* line = xCalloc(line_maxlen + 1, sizeof(char));

   for (; *p != '\0'; p++) {
      if (line_offset >= line_maxlen) {
         assert(line_offset <= line_maxlen);
         assert(last_space <= line_maxlen);

         size_t line_len = last_space <= 0 ? line_offset : last_space;
         char tmp = line[line_len];
         line[line_len] = '\0';
         InfoScreen_addLine(this, line);
         line[line_len] = tmp;

         assert(line_len <= line_offset);
         line_offset -= line_len;
         memmove(line, line + line_len, line_offset);

         last_space = 0;
      }

      line[line_offset++] = *p;
      if (*p == ' ') {
         last_space = line_offset;
      }
   }

   if (line_offset > 0) {
      line[line_offset] = '\0';
      InfoScreen_addLine(this, line);
   }

   free(line);

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
