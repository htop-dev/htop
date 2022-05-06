#ifndef HEADER_SignalsPanel
#define HEADER_SignalsPanel
/*
htop - SignalsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#ifndef HTOP_SOLARIS
#include <signal.h>
#endif

#include "Panel.h"


typedef struct SignalItem_ {
   const char* name;
   int number;
} SignalItem;

#define SIGNALSPANEL_INITSELECTEDSIGNAL SIGTERM

Panel* SignalsPanel_new(int preSelectedSignal);

#endif
