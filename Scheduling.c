/*
htop - Scheduling.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Scheduling.h"

#ifdef SCHEDULER_SUPPORT

#include <assert.h>
#include <stddef.h>

#include "FunctionBar.h"
#include "ListItem.h"
#include "Macros.h"
#include "Object.h"
#include "Panel.h"
#include "XUtils.h"


static const SchedulingPolicy policies[] = {
   [SCHED_OTHER] = { "Other",      SCHED_OTHER, false },
#ifdef SCHED_BATCH
   [SCHED_BATCH] = { "Batch",      SCHED_BATCH, false },
#endif
#ifdef SCHED_IDLE
   [SCHED_IDLE]  = { "Idle",       SCHED_IDLE,  false },
#endif
   [SCHED_FIFO]  = { "FiFo",       SCHED_FIFO,  true },
   [SCHED_RR]    = { "RoundRobin", SCHED_RR,    true },
};

#ifdef SCHED_RESET_ON_FORK
static bool reset_on_fork = false;
#endif


Panel* Scheduling_newPolicyPanel(int preSelectedPolicy) {
   Panel* this = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Select ", "Cancel "));
   Panel_setHeader(this, "New policy:");

#ifdef SCHED_RESET_ON_FORK
   Panel_add(this, (Object*) ListItem_new(reset_on_fork ? "Reset on fork: on" : "Reset on fork: off", -1));
#endif

   for (unsigned i = 0; i < ARRAYSIZE(policies); i++) {
      if (!policies[i].name)
         continue;

      Panel_add(this, (Object*) ListItem_new(policies[i].name, policies[i].id));
      if (policies[i].id == preSelectedPolicy)
         Panel_setSelected(this, i);
   }

   return this;
}

void Scheduling_togglePolicyPanelResetOnFork(Panel* schedPanel) {
#ifdef SCHED_RESET_ON_FORK
   reset_on_fork = !reset_on_fork;

   ListItem* item = (ListItem*) Panel_get(schedPanel, 0);

   free_and_xStrdup(&item->value, reset_on_fork ? "Reset on fork: on" : "Reset on fork: off");
#else
   (void)schedPanel;
#endif
}

Panel* Scheduling_newPriorityPanel(int policy, int preSelectedPriority) {
   if (policy < 0 || (unsigned)policy >= ARRAYSIZE(policies) || policies[policy].name == NULL)
      return NULL;

   if (!policies[policy].prioritySupport)
      return NULL;

   int min = sched_get_priority_min(policy);
   if (min < 0)
      return NULL;
   int max = sched_get_priority_max(policy);
   if (max < 0 )
      return NULL;

   Panel* this = Panel_new(0, 0, 0, 0, Class(ListItem), true, FunctionBar_newEnterEsc("Select ", "Cancel "));
   Panel_setHeader(this, "Priority:");

   for (int i = min; i <= max; i++) {
      char buf[16];
      xSnprintf(buf, sizeof(buf), "%d", i);
      Panel_add(this, (Object*) ListItem_new(buf, i));
      if (i == preSelectedPriority)
         Panel_setSelected(this, i);
   }

   return this;
}

static bool Scheduling_setPolicy(Process* p, Arg arg) {
   const SchedulingArg* sarg = arg.v;
   int policy = sarg->policy;

   assert(policy >= 0);
   assert((unsigned)policy < ARRAYSIZE(policies));
   assert(policies[policy].name);

   const struct sched_param param = { .sched_priority = policies[policy].prioritySupport ? sarg->priority : 0 };

   #ifdef SCHED_RESET_ON_FORK
   if (reset_on_fork)
      policy &= SCHED_RESET_ON_FORK;
   #endif

   int r = sched_setscheduler(Process_getPid(p), policy, &param);

   /* POSIX says on success the previous scheduling policy should be returned,
    * but Linux always returns 0. */
   return r != -1;
}

bool Scheduling_rowSetPolicy(Row* row, Arg arg) {
   Process* p = (Process*) row;
   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));
   return Scheduling_setPolicy(p, arg);
}

const char* Scheduling_formatPolicy(int policy) {
#ifdef SCHED_RESET_ON_FORK
   policy = policy & ~SCHED_RESET_ON_FORK;
#endif

   switch (policy) {
      case SCHED_OTHER:
         return "OTHER";
      case SCHED_FIFO:
         return "FIFO";
      case SCHED_RR:
         return "RR";
#ifdef SCHED_BATCH
      case SCHED_BATCH:
         return "BATCH";
#endif
#ifdef SCHED_IDLE
      case SCHED_IDLE:
         return "IDLE";
#endif
#ifdef SCHED_DEADLINE
      case SCHED_DEADLINE:
         return "EDF";
#endif
      default:
         return "???";
   }
}

void Scheduling_readProcessPolicy(Process* proc) {
   proc->scheduling_policy = sched_getscheduler(Process_getPid(proc));
}
#endif  /* SCHEDULER_SUPPORT */
