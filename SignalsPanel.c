/*
htop - SignalsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "SignalsPanel.h"

#include "ListItem.h"
#include "RichString.h"

#include <stdlib.h>
#include <assert.h>

#include <ctype.h>

/*{
#include "Panel.h"

typedef struct SignalItem_ {
   const char* name;
   int number;
} SignalItem;

typedef struct SignalsPanel_ {
   Panel super;
} SignalsPanel;

}*/

static void SignalsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   SignalsPanel* this = (SignalsPanel*) object;
   Panel_done(super);
   free(this);
}

static SignalItem signals[] = {
   { .name = " 0 Cancel",    .number = 0 },
   { .name = " 1 SIGHUP",    .number = 1 },
   { .name = " 2 SIGINT",    .number = 2 },
   { .name = " 3 SIGQUIT",   .number = 3 },
   { .name = " 4 SIGILL",    .number = 4 },
   { .name = " 5 SIGTRAP",   .number = 5 },
   { .name = " 6 SIGABRT",   .number = 6 },
   { .name = " 6 SIGIOT",    .number = 6 },
   { .name = " 7 SIGBUS",    .number = 7 },
   { .name = " 8 SIGFPE",    .number = 8 },
   { .name = " 9 SIGKILL",   .number = 9 },
   { .name = "10 SIGUSR1",   .number = 10 },
   { .name = "11 SIGSEGV",   .number = 11 },
   { .name = "12 SIGUSR2",   .number = 12 },
   { .name = "13 SIGPIPE",   .number = 13 },
   { .name = "14 SIGALRM",   .number = 14 },
   { .name = "15 SIGTERM",   .number = 15 },
   { .name = "16 SIGSTKFLT", .number = 16 },
   { .name = "17 SIGCHLD",   .number = 17 },
   { .name = "18 SIGCONT",   .number = 18 },
   { .name = "19 SIGSTOP",   .number = 19 },
   { .name = "20 SIGTSTP",   .number = 20 },
   { .name = "21 SIGTTIN",   .number = 21 },
   { .name = "22 SIGTTOU",   .number = 22 },
   { .name = "23 SIGURG",    .number = 23 },
   { .name = "24 SIGXCPU",   .number = 24 },
   { .name = "25 SIGXFSZ",   .number = 25 },
   { .name = "26 SIGVTALRM", .number = 26 },
   { .name = "27 SIGPROF",   .number = 27 },
   { .name = "28 SIGWINCH",  .number = 28 },
   { .name = "29 SIGIO",     .number = 29 },
   { .name = "29 SIGPOLL",   .number = 29 },
   { .name = "30 SIGPWR",    .number = 30 },
   { .name = "31 SIGSYS",    .number = 31 },
};

SignalsPanel* SignalsPanel_new(int x, int y, int w, int h) {
   SignalsPanel* this = (SignalsPanel*) malloc(sizeof(SignalsPanel));
   Panel* super = (Panel*) this;
   Panel_init(super, x, y, w, h, LISTITEM_CLASS, true);
   ((Object*)this)->delete = SignalsPanel_delete;

   for(unsigned int i = 0; i < sizeof(signals)/sizeof(SignalItem); i++)
      Panel_set(super, i, (Object*) ListItem_new(signals[i].name, signals[i].number));
   SignalsPanel_reset(this);
   return this;
}

void SignalsPanel_reset(SignalsPanel* this) {
   Panel* super = (Panel*) this;

   Panel_setHeader(super, "Send signal:");
   Panel_setSelected(super, 16); // 16th item is SIGTERM
}
