#ifndef HEADER_Scheduling
#define HEADER_Scheduling
/*
htop - Scheduling.h
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <sched.h>
#include <stdbool.h>

#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "Row.h"


#if defined(HAVE_SCHED_SETSCHEDULER) && defined(HAVE_SCHED_GETSCHEDULER)
#define SCHEDULER_SUPPORT

typedef struct {
   const char* name;
   int id;
   bool prioritySupport;
} SchedulingPolicy;

#define SCHEDULINGPANEL_INITSELECTEDPOLICY SCHED_OTHER
#define SCHEDULINGPANEL_INITSELECTEDPRIORITY 50

Panel* Scheduling_newPolicyPanel(int preSelectedPolicy);
void Scheduling_togglePolicyPanelResetOnFork(Panel* schedPanel);

Panel* Scheduling_newPriorityPanel(int policy, int preSelectedPriority);


typedef struct {
   int policy;
   int priority;
} SchedulingArg;

bool Scheduling_rowSetPolicy(Row* proc, Arg arg);

const char* Scheduling_formatPolicy(int policy);

void Scheduling_readProcessPolicy(Process* proc);

#endif

#endif  /* HEADER_Scheduling */
