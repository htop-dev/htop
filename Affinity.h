#ifndef HEADER_Affinity
#define HEADER_Affinity
/*
htop - Affinity.h
(C) 2004-2011 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifdef HAVE_LIBHWLOC
#if __linux__
#define HTOP_HWLOC_CPUBIND_FLAG HWLOC_CPUBIND_THREAD
#else
#define HTOP_HWLOC_CPUBIND_FLAG HWLOC_CPUBIND_PROCESS
#endif
#endif

#include "Process.h"
#include "ProcessList.h"

typedef struct Affinity_ {
   ProcessList* pl;
   int size;
   int used;
   int* cpus;
} Affinity;

Affinity* Affinity_new(ProcessList* pl);

void Affinity_delete(Affinity* this);

void Affinity_add(Affinity* this, int id);

#ifdef HAVE_LIBHWLOC

Affinity* Affinity_get(Process* proc, ProcessList* pl);

bool Affinity_set(Process* proc, Arg arg);

#elif HAVE_LINUX_AFFINITY

Affinity* Affinity_get(Process* proc, ProcessList* pl);

bool Affinity_set(Process* proc, Arg arg);

#endif

#endif
