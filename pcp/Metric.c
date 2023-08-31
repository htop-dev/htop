/*
htop - Metric.c
(C) 2020-2021 htop dev team
(C) 2020-2021 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/Metric.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "XUtils.h"

#include "pcp/Platform.h"


extern Platform* pcp;

const pmDesc* Metric_desc(Metric metric) {
   return &pcp->descs[metric];
}

int Metric_type(Metric metric) {
   return pcp->descs[metric].type;
}

pmAtomValue* Metric_values(Metric metric, pmAtomValue* atom, int count, int type) {
   if (pcp->result == NULL)
      return NULL;

   pmValueSet* vset = pcp->result->vset[metric];
   if (!vset || vset->numval <= 0)
      return NULL;

   /* extract requested number of values as requested type */
   const pmDesc* desc = &pcp->descs[metric];
   for (int i = 0; i < vset->numval; i++) {
      if (i == count)
         break;
      const pmValue* value = &vset->vlist[i];
      int sts = pmExtractValue(vset->valfmt, value, desc->type, &atom[i], type);
      if (sts < 0) {
         if (pmDebugOptions.appl0)
            fprintf(stderr, "Error: cannot extract metric value: %s\n",
                            pmErrStr(sts));
         memset(&atom[i], 0, sizeof(pmAtomValue));
      }
   }
   return atom;
}

int Metric_instanceCount(Metric metric) {
   pmValueSet* vset = pcp->result->vset[metric];
   if (vset)
      return vset->numval;
   return 0;
}

int Metric_instanceOffset(Metric metric, int inst) {
   pmValueSet* vset = pcp->result->vset[metric];
   if (!vset || vset->numval <= 0)
      return 0;

   /* search for optimal offset for subsequent inst lookups to begin */
   for (int i = 0; i < vset->numval; i++) {
      if (inst == vset->vlist[i].inst)
         return i;
   }
   return 0;
}

static pmAtomValue* Metric_extract(Metric metric, int inst, int offset, pmValueSet* vset, pmAtomValue* atom, int type) {

   /* extract value (using requested type) of given metric instance */
   const pmDesc* desc = &pcp->descs[metric];
   const pmValue* value = &vset->vlist[offset];
   int sts = pmExtractValue(vset->valfmt, value, desc->type, atom, type);
   if (sts < 0) {
      if (pmDebugOptions.appl0)
         fprintf(stderr, "Error: cannot extract %s instance %d value: %s\n",
                         pcp->names[metric], inst, pmErrStr(sts));
      memset(atom, 0, sizeof(pmAtomValue));
   }
   return atom;
}

pmAtomValue* Metric_instance(Metric metric, int inst, int offset, pmAtomValue* atom, int type) {

   pmValueSet* vset = pcp->result->vset[metric];
   if (!vset || vset->numval <= 0)
      return NULL;

   /* fast-path using heuristic offset based on expected location */
   if (offset >= 0 && offset < vset->numval && inst == vset->vlist[offset].inst)
      return Metric_extract(metric, inst, offset, vset, atom, type);

   /* slow-path using a linear search for the requested instance */
   for (int i = 0; i < vset->numval; i++) {
      if (inst == vset->vlist[i].inst)
         return Metric_extract(metric, inst, i, vset, atom, type);
   }
   return NULL;
}

/*
 * Iterate over a set of instances (incl PM_IN_NULL)
 * returning the next instance identifier and offset.
 *
 * Start it off by passing offset -1 into the routine.
 */
bool Metric_iterate(Metric metric, int* instp, int* offsetp) {
   if (!pcp->result)
      return false;

   pmValueSet* vset = pcp->result->vset[metric];
   if (!vset || vset->numval <= 0)
      return false;

   int offset = *offsetp;
   offset = (offset < 0) ? 0 : offset + 1;
   if (offset > vset->numval - 1)
      return false;

   *offsetp = offset;
   *instp = vset->vlist[offset].inst;
   return true;
}

/* Switch on/off a metric for value fetching (sampling) */
void Metric_enable(Metric metric, bool enable) {
   pcp->fetch[metric] = enable ? pcp->pmids[metric] : PM_ID_NULL;
}

bool Metric_enabled(Metric metric) {
   return pcp->fetch[metric] != PM_ID_NULL;
}

void Metric_enableThreads(void) {
   pmValueSet* vset = xCalloc(1, sizeof(pmValueSet));
   vset->vlist[0].inst = PM_IN_NULL;
   vset->vlist[0].value.lval = 1;
   vset->valfmt = PM_VAL_INSITU;
   vset->numval = 1;
   vset->pmid = pcp->pmids[PCP_CONTROL_THREADS];

   pmResult* result = xCalloc(1, sizeof(pmResult));
   result->vset[0] = vset;
   result->numpmid = 1;

   int sts = pmStore(result);
   if (sts < 0 && pmDebugOptions.appl0)
      fprintf(stderr, "Error: cannot enable threads: %s\n", pmErrStr(sts));

   pmFreeResult(result);
}

bool Metric_fetch(struct timeval* timestamp) {
   if (pcp->result) {
      pmFreeResult(pcp->result);
      pcp->result = NULL;
   }
   int sts, count = 0;
   do {
      sts = pmFetch(pcp->totalMetrics, pcp->fetch, &pcp->result);
   } while (sts == PM_ERR_IPC && ++count < 3);
   if (sts < 0) {
      if (pmDebugOptions.appl0)
         fprintf(stderr, "Error: cannot fetch metric values: %s\n",
                 pmErrStr(sts));
      return false;
   }
   if (timestamp)
      *timestamp = pcp->result->timestamp;
   return true;
}

void Metric_externalName(Metric metric, int inst, char** externalName) {
   const pmDesc* desc = &pcp->descs[metric];
   /* ignore a failure here - its safe to do so */
   (void)pmNameInDom(desc->indom, inst, externalName);
}

int Metric_lookupText(const char* metric, char** desc) {
   pmID pmid;
   int sts;

   sts = pmLookupName(1, &metric, &pmid);
   if (sts < 0)
      return sts;

   if (pmLookupText(pmid, PM_TEXT_ONELINE, desc) >= 0)
      (*desc)[0] = toupper((*desc)[0]); /* UI consistency */
   return 0;
}
