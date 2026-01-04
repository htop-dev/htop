/*
htop - ScriptOutputScreen.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ScriptOutputScreen.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "Panel.h"
#include "ProvideCurses.h"
#include "XUtils.h"


ScriptOutputScreen* ScriptOutputScreen_new(const Process* process) {
   ScriptOutputScreen* this = xCalloc(1, sizeof(ScriptOutputScreen));
   Object_setClass(this, Class(ScriptOutputScreen));
   // this fd needs to be set later
   this->read_fd = -1;
   this->data_head = NULL;
   this->data_tail = &this->data_head;
   return (ScriptOutputScreen*) InfoScreen_init(&this->super, process, NULL, LINES - 2, " ");
}

void ScriptOutputScreen_SetFd(ScriptOutputScreen* this, int fd) {
   this->read_fd = fd;
}

void ScriptOutputScreen_delete(Object* this) {
   // free the linked list and close fd
   assert(Object_isA((const Object*) this, (const ObjectClass*) &ScriptOutputScreen_class));
   Node* walk = ((ScriptOutputScreen*)this)->data_head;
   while (walk) {
      free(walk->line);
      Node* next = walk->next;
      free(walk);
      walk = next;
   }
   close(((ScriptOutputScreen*)this)->read_fd);
   free(InfoScreen_done((InfoScreen*)this));
}

static void ScriptOutputScreen_scan(InfoScreen* super) {
   Panel* panel = super->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);

   char buffer[8192];
   ScriptOutputScreen* sos = ((ScriptOutputScreen*)super);
   assert(Object_isA((const Object*) sos, (const ObjectClass*) &ScriptOutputScreen_class));

   // redraw existing stuff in the screen first
   Node* walk = sos->data_head;
   while (walk) {
      InfoScreen_addLine(super, walk->line);
      walk = walk->next;
   }

   for (;;) {
      ssize_t res = read(sos->read_fd, buffer, sizeof(buffer) - 1);
      if (res < 0) {
         if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            continue;

         break;
      }

      if (res == 0) {
         break;
      }

      if (res > 0) {
         int start = 0;
         int num_tabs = 0;
         for (int i = 0; i <= res; i++) {
            num_tabs += (buffer[i] == '\t');
            // split line when find \n or exhaust buffer
            if (i == res || buffer[i] == '\n') {
               buffer[i] = '\0';
               char* str;
               if (num_tabs > 0) {
                  // manually replace all \t with TABSIZE spaces
                  str = xMalloc((num_tabs * TABSIZE + i - start + 1) * sizeof(char));
                  int index = 0;
                  for (int j = start; j <= i; j++) {
                     if (buffer[j] == '\t') {
                        for (int k = 0; k < TABSIZE; k++) {
                           str[index++] = ' ';
                        }
                     } else {
                        str[index++] = buffer[j];
                     }
                  }
               } else {
                  str = buffer + start;
               }
               InfoScreen_addLine(super, str);
               // store line for next redraw
               *(sos->data_tail) = xMalloc(sizeof(Node));
               (*sos->data_tail)->line = xStrdup(str);
               (*sos->data_tail)->next = NULL;
               *(&sos->data_tail) = &((*sos->data_tail)->next);

               if (num_tabs > 0)
                  free(str);
               start = i + 1;
               num_tabs = 0;
            }
         }
      }
   }
   Panel_setSelected(panel, idx);
}

static void ScriptOutputScreen_draw(InfoScreen* this ) {
   InfoScreen_drawTitled(this, "Output of script for process %d - %s", Process_getPid(this->process), Process_getCommand(this->process));
}

const InfoScreenClass ScriptOutputScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = ScriptOutputScreen_delete
   },
   .scan = ScriptOutputScreen_scan,
   .draw = ScriptOutputScreen_draw
};
