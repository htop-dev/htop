/*
htop - TraceScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TraceScreen.h"

#include "CRT.h"
#include "ProcessList.h"
#include "ListItem.h"
#include "IncSet.h"
#include "String.h"
#include "FunctionBar.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/*{
#include "Process.h"
#include "Panel.h"

typedef struct TraceScreen_ {
   Process* process;
   Panel* display;
   bool tracing;
} TraceScreen;

}*/

static const char* TraceScreenFunctions[] = {"Search ", "Filter ", "AutoScroll ", "Stop Tracing   ", "Done   ", NULL};

static const char* TraceScreenKeys[] = {"F3", "F4", "F8", "F9", "Esc"};

static int TraceScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(8), KEY_F(9), 27};

TraceScreen* TraceScreen_new(Process* process) {
   TraceScreen* this = (TraceScreen*) malloc(sizeof(TraceScreen));
   this->process = process;
   FunctionBar* fuBar = FunctionBar_new(TraceScreenFunctions, TraceScreenKeys, TraceScreenEvents);
   this->display = Panel_new(0, 1, COLS, LINES-2, false, Class(ListItem), fuBar);
   this->tracing = true;
   return this;
}

void TraceScreen_delete(TraceScreen* this) {
   Panel_delete((Object*)this->display);
   free(this);
}

static void TraceScreen_draw(TraceScreen* this, IncSet* inc) {
   attrset(CRT_colors[PANEL_HEADER_FOCUS]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "Trace of process %d - %s", this->process->pid, this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   IncSet_drawBar(inc);
}

static inline void addLine(const char* line, Vector* lines, Panel* panel, const char* incFilter) {
   Vector_add(lines, (Object*) ListItem_new(line, 0));
   if (!incFilter || String_contains_i(line, incFilter))
      Panel_add(panel, (Object*)Vector_get(lines, Vector_size(lines)-1));
}

static inline void appendLine(const char* line, Vector* lines, Panel* panel, const char* incFilter) {
   ListItem* last = (ListItem*)Vector_get(lines, Vector_size(lines)-1);
   ListItem_append(last, line);
   if (incFilter && Panel_get(panel, Panel_size(panel)-1) != (Object*)last && String_contains_i(line, incFilter))
      Panel_add(panel, (Object*)last);
}

void TraceScreen_run(TraceScreen* this) {
   char buffer[1001];
   int fdpair[2];
   int err = pipe(fdpair);
   if (err == -1) return;
   int child = fork();
   if (child == -1) return;
   if (child == 0) {
      dup2(fdpair[1], STDERR_FILENO);
      int ok = fcntl(fdpair[1], F_SETFL, O_NONBLOCK);
      if (ok != -1) {
         sprintf(buffer, "%d", this->process->pid);
         execlp("strace", "strace", "-p", buffer, NULL);
      }
      const char* message = "Could not execute 'strace'. Please make sure it is available in your $PATH.";
      ssize_t written = write(fdpair[1], message, strlen(message));
      (void) written;
      exit(1);
   }
   fcntl(fdpair[0], F_SETFL, O_NONBLOCK);
   FILE* strace = fdopen(fdpair[0], "r");
   Panel* panel = this->display;
   int fd_strace = fileno(strace);
   CRT_disableDelay();
   bool contLine = false;
   bool follow = false;
   bool looping = true;

   FunctionBar* bar = panel->defaultBar;
   IncSet* inc = IncSet_new(bar);

   Vector* lines = Vector_new(panel->items->type, true, DEFAULT_SIZE);

   TraceScreen_draw(this, inc);
   
   while (looping) {

      Panel_draw(panel, true);
      const char* incFilter = IncSet_filter(inc);

      if (inc->active)
         move(LINES-1, CRT_cursorX);
      int ch = getch();
      
      if (ch == ERR) {
         fd_set fds;
         FD_ZERO(&fds);
//         FD_SET(STDIN_FILENO, &fds);
         FD_SET(fd_strace, &fds);
         struct timeval tv;
         tv.tv_sec = 0; tv.tv_usec = 500;
         int ready = select(fd_strace+1, &fds, NULL, NULL, &tv);
         int nread = 0;
         if (ready > 0 && FD_ISSET(fd_strace, &fds))
            nread = fread(buffer, 1, 1000, strace);
         if (nread && this->tracing) {
            char* line = buffer;
            buffer[nread] = '\0';
            for (int i = 0; i < nread; i++) {
               if (buffer[i] == '\n') {
                  buffer[i] = '\0';
                  if (contLine) {
                     appendLine(line, lines, panel, incFilter);
                     contLine = false;
                  } else {
                     addLine(line, lines, panel, incFilter);
                  }
                  line = buffer+i+1;
               }
            }
            if (line < buffer+nread) {
               addLine(line, lines, panel, incFilter);
               buffer[nread] = '\0';
               contLine = true;
            }
            if (follow)
               Panel_setSelected(panel, Panel_size(panel)-1);
         }
      }
            
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
               follow = false;
               ch = 0;
            } if (mevent.y == LINES - 1)
               ch = IncSet_synthesizeEvent(inc, mevent.x);
      }
      
      if (inc->active) {
         IncSet_handleKey(inc, ch, panel, IncSet_getListItemValue, lines);
         continue;
      }
      
      switch(ch) {
      case ERR:
         continue;
      case KEY_HOME:
         Panel_setSelected(panel, 0);
         break;
      case KEY_END:
         Panel_setSelected(panel, Panel_size(panel)-1);
         break;
      case KEY_F(3):
      case '/':
         IncSet_activate(inc, INC_SEARCH, panel);
         break;
      case KEY_F(4):
      case '\\':
         IncSet_activate(inc, INC_FILTER, panel);
         break;
      case 'f':
      case KEY_F(8):
         follow = !follow;
         if (follow)
            Panel_setSelected(panel, Panel_size(panel)-1);
         break;
      case 't':
      case KEY_F(9):
         this->tracing = !this->tracing;
         FunctionBar_setLabel(bar, KEY_F(9), this->tracing?"Stop Tracing   ":"Resume Tracing ");
         TraceScreen_draw(this, inc);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         TraceScreen_draw(this, inc);
         break;
      default:
         follow = false;
         Panel_onKey(panel, ch);
      }
   }
   
   IncSet_delete(inc);
   Vector_delete(lines);
   
   kill(child, SIGTERM);
   waitpid(child, NULL, 0);
   fclose(strace);
   CRT_enableDelay();
}
