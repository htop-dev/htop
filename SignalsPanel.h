#ifndef HEADER_SignalsPanel
#define HEADER_SignalsPanel
/*
htop - SignalsPanel.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

typedef struct SignalItem_ {
   const char* name;
   int number;
} SignalItem;

Panel* SignalsPanel_new();

#endif
