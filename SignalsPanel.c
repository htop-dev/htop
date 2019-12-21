/*
htop - SignalsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "SignalsPanel.h"
#include "Platform.h"

#include "ListItem.h"
#include "RichString.h"

#include <stdlib.h>
#include <assert.h>
#include <signal.h>

#include <ctype.h>


Panel* SignalsPanel_new() {
   Panel* this = Panel_new(1, 1, 1, 1, true, Class(ListItem), FunctionBar_newEnterEsc("Send   ", "Cancel "));
   const int defaultSignal = SIGTERM;
   int defaultPosition = 15;
   unsigned int i;
   for (i = 0; i < Platform_numberOfSignals; i++) {
      Panel_set(this, i, (Object*) ListItem_new(Platform_signals[i].name, Platform_signals[i].number));
      // signal 15 is not always the 15th signal in the table
      if (Platform_signals[i].number == defaultSignal) {
         defaultPosition = i;
      }
   }
   #if (defined(SIGRTMIN) && defined(SIGRTMAX))
   if (SIGRTMAX - SIGRTMIN <= 100) {
      static char buf[16];
      for (int sig = SIGRTMIN; sig <= SIGRTMAX; i++, sig++) {
         int n = sig - SIGRTMIN;
         xSnprintf(buf, 16, "%2d SIGRTMIN%-+3d", sig, n);
         if (n == 0) {
            buf[11] = '\0';
         }
         Panel_set(this, i, (Object*) ListItem_new(buf, sig));
      }
   }
   #endif
   Panel_setHeader(this, "Send signal:");
   Panel_setSelected(this, defaultPosition);
   return this;
}
