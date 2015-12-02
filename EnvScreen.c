#include "EnvScreen.h"

#include "CRT.h"
#include "IncSet.h"
#include "ListItem.h"
#include "StringUtils.h"

#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

/*{
#include "ProcessList.h"
#include "Panel.h"
#include "FunctionBar.h"

typedef struct EnvScreen_ {
   Process* process;
   Panel* display;
   FunctionBar* bar;
} EnvScreen;
}*/

static const char* EnvScreenFunctions[] = {"Search ", "Filter ", "Refresh", "Done   ", NULL};

static const char* EnvScreenKeys[] = {"F3", "F4", "F5", "Esc"};

static int EnvScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(5), 27};

EnvScreen* EnvScreen_new(Process* process) {
   EnvScreen* this = malloc(sizeof(EnvScreen));
   this->process = process;
   FunctionBar* bar = FunctionBar_new(EnvScreenFunctions, EnvScreenKeys, EnvScreenEvents);
   this->display = Panel_new(0, 1, COLS, LINES-3, false, Class(ListItem), bar);
   return this;
}

void EnvScreen_delete(EnvScreen* this) {
   Panel_delete((Object*)this->display);
   free(this);
}

static void EnvScreen_draw(EnvScreen* this, IncSet* inc) {
   attrset(CRT_colors[METER_TEXT]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "environment of process %d - %s", this->process->pid, this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   Panel_draw(this->display, true);
   IncSet_drawBar(inc);
}

static inline void addLine(const char* line, Vector* lines, Panel* panel, const char* incFilter) {
   Vector_add(lines, (Object*) ListItem_new(line, 0));
   if (!incFilter || String_contains_i(line, incFilter))
      Panel_add(panel, (Object*)Vector_get(lines, Vector_size(lines)-1));
}

static void EnvScreen_scan(EnvScreen* this, Vector* lines, IncSet* inc) {
   Panel* panel = this->display;
   int idx = MAX(Panel_getSelectedIndex(panel), 0);
   uid_t uid = getuid();

   Panel_prune(panel);

   if (uid == 0 || uid == this->process->st_uid) {
       long argmax = sysconf(_SC_ARG_MAX);
       char* buf = malloc(argmax);
       size_t bufsz = argmax;
       if (buf) {
           int mib[3];
           mib[0] = CTL_KERN;
           mib[1] = KERN_PROCARGS2;
           mib[2] = this->process->pid;
           bufsz = argmax;
           if (sysctl(mib, 3, buf, &bufsz, 0, 0) == 0) {
               if (bufsz > sizeof(int)) {
                   char *p = buf, *endp = buf + bufsz;
                   int argc = *(int*)p;
                   p += sizeof(int);

                   // skip exe
                   p = strchr(p, 0)+1;

                   // skip padding
                   while(!*p && p < endp)
                       ++p;

                   // skip argv
                   for (; argc-- && p < endp; p = strrchr(p, 0)+1)
                       ;

                   // skip padding
                   while(!*p && p < endp)
                       ++p;

                   for (; *p && p < endp; p = strrchr(p, 0)+1)
                       addLine(p, lines, panel, IncSet_filter(inc));
               }
               else {
                   addLine("Could not allocate memory.", lines, panel, IncSet_filter(inc));
               }
           }
           else {
               addLine("sysctl(KERN_PROCARGS2) failed.", lines, panel, IncSet_filter(inc));
           }
           free(buf);
       }
       else {
           addLine("Out of memory.", lines, panel, IncSet_filter(inc));
       }
   }
   else {
       addLine("Process belongs to different user.", lines, panel, IncSet_filter(inc));
   }

   Vector_insertionSort(lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}

void EnvScreen_run(EnvScreen* this) {
   Panel* panel = this->display;
   Panel_setHeader(panel, " ");

   FunctionBar* bar = panel->defaultBar;
   IncSet* inc = IncSet_new(bar);

   Vector* lines = Vector_new(panel->items->type, true, DEFAULT_SIZE);

   EnvScreen_scan(this, lines, inc);
   EnvScreen_draw(this, inc);

   bool looping = true;
   while (looping) {

      Panel_draw(panel, true);

      if (inc->active)
         move(LINES-1, CRT_cursorX);
      int ch = getch();

      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         int ok = getmouse(&mevent);
         if (ok == OK)
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV);
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
      case KEY_F(3):
      case '/':
         IncSet_activate(inc, INC_SEARCH, panel);
         break;
      case KEY_F(4):
      case '\\':
         IncSet_activate(inc, INC_FILTER, panel);
         break;
      case KEY_F(5):
         clear();
         EnvScreen_scan(this, lines, inc);
         EnvScreen_draw(this, inc);
         break;
      case '\014': // Ctrl+L
         clear();
         EnvScreen_draw(this, inc);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         EnvScreen_draw(this, inc);
         break;
      default:
         Panel_onKey(panel, ch);
      }
   }

   Vector_delete(lines);
   IncSet_delete(inc);
}
