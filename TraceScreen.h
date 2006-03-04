/*
htop - TraceScreen.h
(C) 2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/
#ifndef HEADER_TraceScreen
#define HEADER_TraceScreen

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#include "ProcessList.h"
#include "ListBox.h"
#include "FunctionBar.h"

typedef struct TraceScreen_ {
   Process* process;
   ListBox* display;
   FunctionBar* bar;
   bool tracing;
} TraceScreen;

TraceScreen* TraceScreen_new(Process* process);

void TraceScreen_delete(TraceScreen* this);

void TraceScreen_draw(TraceScreen* this);

void TraceScreen_run(TraceScreen* this);

#endif

