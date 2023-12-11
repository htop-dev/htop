#ifndef HEADER_Affinity
#define HEADER_Affinity
/*
htop - Affinity.h
(C) 2004-2011 Hisham H. Muhammad
(C) 2020,2023 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Machine.h"

#if defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY)
#include <stdbool.h>

#include "Object.h"
#include "Row.h"
#endif


#if defined(HAVE_LIBHWLOC) && defined(HAVE_AFFINITY)
#error hwloc and affinity support are mutual exclusive.
#endif


typedef struct Affinity_ {
   Machine* host;
   unsigned int size;
   unsigned int used;
   unsigned int* cpus;
} Affinity;

Affinity* Affinity_new(Machine* host);

void Affinity_delete(Affinity* this);

void Affinity_add(Affinity* this, unsigned int id);

#if defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY)

Affinity* Affinity_rowGet(const Row* row, Machine* host);

bool Affinity_rowSet(Row* row, Arg arg);

#endif /* HAVE_LIBHWLOC || HAVE_AFFINITY */

#endif
