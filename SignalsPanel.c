/*
htop - SignalsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "SignalsPanel.h"
// the above contains #include <signal.h> so do not add that here again (breaks Solaris build)

#include <stdbool.h>

#include "FunctionBar.h"
#include "ListItem.h"
#include "Object.h"
#include "Panel.h"
#include "Platform.h"
#include "XUtils.h"


Panel* SignalsPanel_new(int preSelectedSignal) {
   Panel* this = Panel_new(1, 1, 1, 1, Class(ListItem), true, FunctionBar_newEnterEsc("Send   ", "Cancel "));
   int defaultPosition = 15;
   unsigned int i;
   for (i = 0; i < Platform_numberOfSignals; i++) {
      Panel_set(this, i, (Object*) ListItem_new(Platform_signals[i].name, Platform_signals[i].number));
      // signal 15 is not always the 15th signal in the table
      if (Platform_signals[i].number == preSelectedSignal) {
         defaultPosition = i;
      }
   }
   #if (defined(SIGRTMIN) && defined(SIGRTMAX))
   if (SIGRTMAX - SIGRTMIN <= 100) {
      static char buf[16];
      for (int sig = SIGRTMIN; sig <= SIGRTMAX; i++, sig++) {
         int n = sig - SIGRTMIN;
         xSnprintf(buf, sizeof(buf), "%2d SIGRTMIN%-+3d", sig, n);
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
