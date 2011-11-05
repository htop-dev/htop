
#include "SignalsPanel.h"
#include "Panel.h"
#include "ListItem.h"
#include "RichString.h"

#include "debug.h"
#include <assert.h>

#include <ctype.h>

/*{

typedef struct SignalsPanel_ {
   Panel super;
   ListItem** signals;
} SignalsPanel;

}*/

#ifndef SIGNAL_COUNT
#define SIGNAL_COUNT 34
#endif

static void SignalsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   SignalsPanel* this = (SignalsPanel*) object;
   Panel_done(super);
   free(this->signals);
   free(this);
}

static ListItem** Signal_getSignalTable() {
   ListItem** signals = malloc(sizeof(ListItem*) * SIGNAL_COUNT);
   signals[0] = ListItem_new(" 0 Cancel", 0);
   signals[1] = ListItem_new(" 1 SIGHUP", 1);
   signals[2] = ListItem_new(" 2 SIGINT", 2);
   signals[3] = ListItem_new(" 3 SIGQUIT", 3);
   signals[4] = ListItem_new(" 4 SIGILL", 4);
   signals[5] = ListItem_new(" 5 SIGTRAP", 5);
   signals[6] = ListItem_new(" 6 SIGABRT", 6);
   signals[7] = ListItem_new(" 6 SIGIOT", 6);
   signals[8] = ListItem_new(" 7 SIGBUS", 7);
   signals[9] = ListItem_new(" 8 SIGFPE", 8);
   signals[10] = ListItem_new(" 9 SIGKILL", 9);
   signals[11] = ListItem_new("10 SIGUSR1", 10);
   signals[12] = ListItem_new("11 SIGSEGV", 11);
   signals[13] = ListItem_new("12 SIGUSR2", 12);
   signals[14] = ListItem_new("13 SIGPIPE", 13);
   signals[15] = ListItem_new("14 SIGALRM", 14);
   signals[16] = ListItem_new("15 SIGTERM", 15);
   signals[17] = ListItem_new("16 SIGSTKFLT", 16);
   signals[18] = ListItem_new("17 SIGCHLD", 17);
   signals[19] = ListItem_new("18 SIGCONT", 18);
   signals[20] = ListItem_new("19 SIGSTOP", 19);
   signals[21] = ListItem_new("20 SIGTSTP", 20);
   signals[22] = ListItem_new("21 SIGTTIN", 21);
   signals[23] = ListItem_new("22 SIGTTOU", 22);
   signals[24] = ListItem_new("23 SIGURG", 23);
   signals[25] = ListItem_new("24 SIGXCPU", 24);
   signals[26] = ListItem_new("25 SIGXFSZ", 25);
   signals[27] = ListItem_new("26 SIGVTALRM", 26);
   signals[28] = ListItem_new("27 SIGPROF", 27);
   signals[29] = ListItem_new("28 SIGWINCH", 28);
   signals[30] = ListItem_new("29 SIGIO", 29);
   signals[31] = ListItem_new("29 SIGPOLL", 29);
   signals[32] = ListItem_new("30 SIGPWR", 30);
   signals[33] = ListItem_new("31 SIGSYS", 31);
   return signals;
}

SignalsPanel* SignalsPanel_new(int x, int y, int w, int h) {
   SignalsPanel* this = (SignalsPanel*) malloc(sizeof(SignalsPanel));
   Panel* super = (Panel*) this;
   Panel_init(super, x, y, w, h, LISTITEM_CLASS, true);
   ((Object*)this)->delete = SignalsPanel_delete;

   this->signals = Signal_getSignalTable();
   for(int i = 0; i < SIGNAL_COUNT; i++)
      Panel_set(super, i, (Object*) this->signals[i]);
   SignalsPanel_reset(this);
   return this;
}

void SignalsPanel_reset(SignalsPanel* this) {
   Panel* super = (Panel*) this;

   Panel_setHeader(super, "Send signal:");
   Panel_setSelected(super, 16); // 16th item is SIGTERM
}
