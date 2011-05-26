/*
htop - SignalItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "SignalItem.h"
#include "String.h"
#include "Object.h"
#include "RichString.h"
#include <string.h>

#include "debug.h"

#define SIGNAL_COUNT 34

/*{

typedef struct Signal_ {
   Object super;
   const char* name;
   int number;
} Signal;

}*/

#ifdef DEBUG
char* SIGNAL_CLASS = "Signal";
#else
#define SIGNAL_CLASS NULL
#endif

static void Signal_delete(Object* cast) {
   Signal* this = (Signal*)cast;
   assert (this != NULL);
   // names are string constants, so we're not deleting them.
   free(this);
}

static void Signal_display(Object* cast, RichString* out) {
   Signal* this = (Signal*)cast;
   assert (this != NULL);
   
   char buffer[31];
   snprintf(buffer, 30, "%2d %s", this->number, this->name);
   RichString_write(out, CRT_colors[DEFAULT_COLOR], buffer);
}

static Signal* Signal_new(const char* name, int number) {
   Signal* this = malloc(sizeof(Signal));
   Object_setClass(this, SIGNAL_CLASS);
   ((Object*)this)->display = Signal_display;
   ((Object*)this)->delete = Signal_delete;
   this->name = name;
   this->number = number;
   return this;
}

int Signal_getSignalCount() {
   return SIGNAL_COUNT;
}

Signal** Signal_getSignalTable() {
   Signal** signals = malloc(sizeof(Signal*) * SIGNAL_COUNT);
   signals[0] = Signal_new("Cancel", 0);
   signals[1] = Signal_new("SIGHUP", 1);
   signals[2] = Signal_new("SIGINT", 2);
   signals[3] = Signal_new("SIGQUIT", 3);
   signals[4] = Signal_new("SIGILL", 4);
   signals[5] = Signal_new("SIGTRAP", 5);
   signals[6] = Signal_new("SIGABRT", 6);
   signals[7] = Signal_new("SIGIOT", 6);
   signals[8] = Signal_new("SIGBUS", 7);
   signals[9] = Signal_new("SIGFPE", 8);
   signals[10] = Signal_new("SIGKILL", 9);
   signals[11] = Signal_new("SIGUSR1", 10);
   signals[12] = Signal_new("SIGSEGV", 11);
   signals[13] = Signal_new("SIGUSR2", 12);
   signals[14] = Signal_new("SIGPIPE", 13);
   signals[15] = Signal_new("SIGALRM", 14);
   signals[16] = Signal_new("SIGTERM", 15);
   signals[17] = Signal_new("SIGSTKFLT", 16);
   signals[18] = Signal_new("SIGCHLD", 17);
   signals[19] = Signal_new("SIGCONT", 18);
   signals[20] = Signal_new("SIGSTOP", 19);
   signals[21] = Signal_new("SIGTSTP", 20);
   signals[22] = Signal_new("SIGTTIN", 21);
   signals[23] = Signal_new("SIGTTOU", 22);
   signals[24] = Signal_new("SIGURG", 23);
   signals[25] = Signal_new("SIGXCPU", 24);
   signals[26] = Signal_new("SIGXFSZ", 25);
   signals[27] = Signal_new("SIGVTALRM", 26);
   signals[28] = Signal_new("SIGPROF", 27);
   signals[29] = Signal_new("SIGWINCH", 28);
   signals[30] = Signal_new("SIGIO", 29);
   signals[31] = Signal_new("SIGPOLL", 29);
   signals[32] = Signal_new("SIGPWR", 30);
   signals[33] = Signal_new("SIGSYS", 31);
   return signals;
}
