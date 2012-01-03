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
#include "FunctionBar.h"

typedef struct TraceScreen_ {
   Process* process;
   Panel* display;
   FunctionBar* bar;
   bool tracing;
} TraceScreen;

}*/

static const char* tsFunctions[] = {"AutoScroll ", "Stop Tracing   ", "Done   ", NULL};

static const char* tsKeys[] = {"F4", "F5", "Esc"};

static int tsEvents[] = {KEY_F(4), KEY_F(5), 27};

TraceScreen* TraceScreen_new(Process* process) {
   TraceScreen* this = (TraceScreen*) malloc(sizeof(TraceScreen));
   this->process = process;
   this->display = Panel_new(0, 1, COLS, LINES-2, LISTITEM_CLASS, true, ListItem_compare);
   this->bar = FunctionBar_new(tsFunctions, tsKeys, tsEvents);
   this->tracing = true;
   return this;
}

void TraceScreen_delete(TraceScreen* this) {
   Panel_delete((Object*)this->display);
   FunctionBar_delete((Object*)this->bar);
   free(this);
}

static void TraceScreen_draw(TraceScreen* this) {
   attrset(CRT_colors[PANEL_HEADER_FOCUS]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "Trace of process %d - %s", this->process->pid, this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   FunctionBar_draw(this->bar, NULL);
}

void TraceScreen_run(TraceScreen* this) {
//   if (this->process->pid == getpid()) return;
   char buffer[1001];
   int fdpair[2];
   int err = pipe(fdpair);
   if (err == -1) return;
   int child = fork();
   if (child == -1) return;
   if (child == 0) {
      dup2(fdpair[1], STDERR_FILENO);
      fcntl(fdpair[1], F_SETFL, O_NONBLOCK);
      sprintf(buffer, "%d", this->process->pid);
      execlp("strace", "strace", "-p", buffer, NULL);
      const char* message = "Could not execute 'strace'. Please make sure it is available in your $PATH.";
      write(fdpair[1], message, strlen(message));
      exit(1);
   }
   fcntl(fdpair[0], F_SETFL, O_NONBLOCK);
   FILE* strace = fdopen(fdpair[0], "r");
   Panel* panel = this->display;
   int fd_strace = fileno(strace);
   TraceScreen_draw(this);
   CRT_disableDelay();
   bool contLine = false;
   bool follow = false;
   bool looping = true;
   while (looping) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(fd_strace, &fds);
      struct timeval tv;
      tv.tv_sec = 0; tv.tv_usec = 500;
      int ready = select(fd_strace+1, &fds, NULL, NULL, &tv);
      int nread = 0;
      if (ready > 0)
         nread = fread(buffer, 1, 1000, strace);
      if (nread && this->tracing) {
         char* line = buffer;
         buffer[nread] = '\0';
         for (int i = 0; i < nread; i++) {
            if (buffer[i] == '\n') {
               buffer[i] = '\0';
               if (contLine) {
                  ListItem_append((ListItem*)Panel_get(panel,
                     Panel_size(panel)-1), line);
                  contLine = false;
               } else {
                  Panel_add(panel, (Object*) ListItem_new(line, 0));
               }
               line = buffer+i+1;
            }
         }
         if (line < buffer+nread) {
            Panel_add(panel, (Object*) ListItem_new(line, 0));
            buffer[nread] = '\0';
            contLine = true;
         }
         if (follow)
            Panel_setSelected(panel, Panel_size(panel)-1);
         Panel_draw(panel, true);
      }
      int ch = getch();
      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
               follow = false;
               ch = 0;
            } if (mevent.y == LINES - 1)
               ch = FunctionBar_synthesizeEvent(this->bar, mevent.x);
      }
      switch(ch) {
      case ERR:
         continue;
      case KEY_F(5):
         this->tracing = !this->tracing;
         FunctionBar_setLabel(this->bar, KEY_F(5), this->tracing?"Stop Tracing   ":"Resume Tracing ");
         TraceScreen_draw(this);
         break;
      case KEY_HOME:
         Panel_setSelected(panel, 0);
         break;
      case KEY_END:
         Panel_setSelected(panel, Panel_size(panel)-1);
         break;
      case 'f':
      case KEY_F(4):
         follow = !follow;
         if (follow)
            Panel_setSelected(panel, Panel_size(panel)-1);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         TraceScreen_draw(this);
         break;
      default:
         follow = false;
         Panel_onKey(panel, ch);
      }
      Panel_draw(panel, true);
   }
   kill(child, SIGTERM);
   waitpid(child, NULL, 0);
   fclose(strace);
   CRT_enableDelay();
}
