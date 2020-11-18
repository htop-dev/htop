#ifndef HEADER_Affinity
#define HEADER_Affinity
/*
htop - Affinity.h
(C) 2004-2011 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ProcessList.h"

#if defined(HAVE_LIBHWLOC) || defined(HAVE_LINUX_AFFINITY)
#include <stdbool.h>

#include "Object.h"
#include "Process.h"
#endif


#if defined(HAVE_LIBHWLOC) && defined(HAVE_LINUX_AFFINITY)
#error hwloc and linux affinity are mutual exclusive.
#endif


typedef struct Affinity_ {
   ProcessList* pl;
   int size;
   int used;
   int* cpus;
} Affinity;

Affinity* Affinity_new(ProcessList* pl);

void Affinity_delete(Affinity* this);

void Affinity_add(Affinity* this, int id);

#if defined(HAVE_LIBHWLOC) || defined(HAVE_LINUX_AFFINITY)

Affinity* Affinity_get(Process* proc, ProcessList* pl);

bool Affinity_set(Process* proc, Arg arg);

#endif /* HAVE_LIBHWLOC || HAVE_LINUX_AFFINITY */

#endif
