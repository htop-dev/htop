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
   char line[COLS + 1];
   int line_offset = 0, last_spc = -1, len;
   for (; *p != '\0'; p++, line_offset++) {
      assert(line_offset >= 0 && (size_t)line_offset < sizeof(line));
      line[line_offset] = *p;
      if (*p == ' ') {
         last_spc = line_offset;
      }

      if (line_offset == COLS) {
         len = (last_spc == -1) ? line_offset : last_spc;
         line[len] = '\0';
         InfoScreen_addLine(this, line);

         line_offset -= len;
         last_spc = -1;
         memcpy(line, p - line_offset, line_offset + 1);
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
