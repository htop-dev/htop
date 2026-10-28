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

#include "CRT.h"
#include "FunctionBar.h"
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

static void CommandScreen_copyCommand(const Process* process) {
   const char* command = Process_getCommand(process);
   if (!command) return;
   
   char copyCmd[1024];
   bool success = false;
   
#ifdef __APPLE__
   snprintf(copyCmd, sizeof(copyCmd), "printf '%%s' '%s' | pbcopy", command);
   success = (system(copyCmd) == 0);
#elif defined(__linux__) || defined(__unix__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
   snprintf(copyCmd, sizeof(copyCmd), "printf '%%s' '%s' | xclip -selection clipboard 2>/dev/null", command);
   if (system(copyCmd) != 0) {
      snprintf(copyCmd, sizeof(copyCmd), "printf '%%s' '%s' | xsel --clipboard 2>/dev/null", command);
      success = (system(copyCmd) == 0);
   } else {
      success = true;
   }
#endif
   
   // Show feedback message
   attrset(CRT_colors[FUNCTION_BAR]);
   mvhline(LINES - 1, 0, ' ', COLS);
   if (success) {
      mvaddstr(LINES - 1, 0, "Command copied to clipboard");
   } else {
      mvaddstr(LINES - 1, 0, "Copy failed - clipboard not available");
   }
   attrset(CRT_colors[DEFAULT_COLOR]);
   refresh();
}

static bool CommandScreen_onKey(InfoScreen* super, int ch) {
   switch (ch) {
      case KEY_F(7):
         CommandScreen_copyCommand(super->process);
         return true;
      default:
         return false;
   }
}

const InfoScreenClass CommandScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = CommandScreen_delete
   },
   .scan = CommandScreen_scan,
   .draw = CommandScreen_draw,
   .onKey = CommandScreen_onKey
};

CommandScreen* CommandScreen_new(Process* process) {
   CommandScreen* this = AllocThis(CommandScreen);
   
   static const char* const CommandScreenFunctions[] = {"Search ", "Filter ", "Refresh", "Copy   ", "Done   ", NULL};
   static const char* const CommandScreenKeys[] = {"F3", "F4", "F5", "F7", "Esc"};
   static const int CommandScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(5), KEY_F(7), 27};
   
   FunctionBar* bar = FunctionBar_new(CommandScreenFunctions, CommandScreenKeys, CommandScreenEvents);
   return (CommandScreen*) InfoScreen_init(&this->super, process, bar, LINES - 2, " ");
}

void CommandScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}
