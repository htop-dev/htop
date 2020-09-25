#include "CommandScreen.h"

#include "config.h"
#include "CRT.h"
#include "IncSet.h"
#include "ListItem.h"
#include "Platform.h"
#include "StringUtils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static void CommandScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAXIMUM(Panel_getSelectedIndex(panel), 0);
   Panel_prune(panel);

   const char* p = this->process->comm;
   char* line = xMalloc(COLS + 1);
   int line_offset = 0, last_spc = -1, len;
   for (; *p != '\0'; p++, line_offset++) {
      line[line_offset] = *p;
      if (*p == ' ') last_spc = line_offset;

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

   free(line);
   Panel_setSelected(panel, idx);
}

static void CommandScreen_draw(InfoScreen* this) {
   char* title = xMalloc(COLS + 1);
   int len = snprintf(title, COLS + 1, "Command of process %d - %s", this->process->pid, this->process->comm);
   if (len > COLS) {
      memset(&title[COLS - 3], '.', 3);
   }

   InfoScreen_drawTitled(this, "%s", title);
   free(title);
}

InfoScreenClass CommandScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = CommandScreen_delete
   },
   .scan = CommandScreen_scan,
   .draw = CommandScreen_draw
};

CommandScreen* CommandScreen_new(Process* process) {
   CommandScreen* this = AllocThis(CommandScreen);
   return (CommandScreen*) InfoScreen_init(&this->super, process, NULL, LINES - 3, " ");
}

void CommandScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}
