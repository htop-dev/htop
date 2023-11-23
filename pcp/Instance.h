#ifndef HEADER_Instance
#define HEADER_Instance
/*
htop - Instance.h
(C) 2022-2023 htop dev team
(C) 2022-2023 Sohaib Mohammed
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Hashtable.h"
#include "Object.h"
#include "Platform.h"
#include "Row.h"


typedef struct Instance_ {
   Row super;

   char* name; /* external instance name */
   const struct InDomTable_* indom;  /* instance domain */

   /* default result offset to use for searching metrics with instances */
   unsigned int offset;
} Instance;

#define InDom_getId(i_)          ((i_)->indom->id)
#define Instance_getId(i_)       ((i_)->super.id)
#define Instance_setId(i_, id_)  ((i_)->super.id = (id_))

extern const RowClass Instance_class;

Instance* Instance_new(const Machine* host, const struct InDomTable_* indom);

void Instance_done(Instance* this);

#endif
