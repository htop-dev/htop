#ifndef HEADER_PCPGenericDataList
#define HEADER_PCPGenericDataList
/*
htop - PCPGenericDataList.h
(C) 2022 Sohaib Mohammed
(C) 2022 htop dev team
(C) 2022 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "GenericDataList.h"


typedef struct PCPGenericDataList_ {
   GenericDataList super;
} PCPGenericDataList;

void GenericDataList_goThroughEntries(GenericDataList* super, bool pauseUpdate);

GenericDataList* GenericDataList_addPlatformList(GenericDataList* super);

void GenericDataList_removePlatformList(GenericDataList* super);

#endif
