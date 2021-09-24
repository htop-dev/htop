/*
htop - PCPDynamicMeter.c
(C) 2021 htop dev team
(C) 2021 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/
#include "config.h" // IWYU pragma: keep

#include "pcp/PCPDynamicMeter.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pcp/pmapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Macros.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"

#include "pcp/PCPMetric.h"


static PCPDynamicMetric* PCPDynamicMeter_lookupMetric(PCPDynamicMeters* meters, PCPDynamicMeter* meter, const char* name) {
   size_t bytes = 16 + strlen(meter->super.name) + strlen(name);
   char* metricName = xMalloc(bytes);
   xSnprintf(metricName, bytes, "htop.meter.%s.%s", meter->super.name, name);

   PCPDynamicMetric* metric;
   for (size_t i = 0; i < meter->totalMetrics; i++) {
      metric = &meter->metrics[i];
      if (String_eq(metric->name, metricName)) {
         free(metricName);
         return metric;
      }
   }

   /* not an existing metric in this meter - add it */
   size_t n = meter->totalMetrics + 1;
   meter->metrics = xReallocArray(meter->metrics, n, sizeof(PCPDynamicMetric));
   meter->totalMetrics = n;
   metric = &meter->metrics[n - 1];
   memset(metric, 0, sizeof(PCPDynamicMetric));
   metric->name = metricName;
   metric->label = String_cat(name, ": ");
   metric->id = meters->offset + meters->cursor;
   meters->cursor++;

   Platform_addMetric(metric->id, metricName);

   return metric;
}

static void PCPDynamicMeter_parseMetric(PCPDynamicMeters* meters, PCPDynamicMeter* meter, const char* path, unsigned int line, char* key, char* value) {
   PCPDynamicMetric* metric;
   char* p;

   if ((p = strchr(key, '.')) == NULL)
      return;
   *p++ = '\0'; /* end the name, p is now the attribute, e.g. 'label' */

   if (String_eq(p, "metric")) {
      /* lookup a dynamic metric with this name, else create */
      metric = PCPDynamicMeter_lookupMetric(meters, meter, key);

      /* use derived metrics in dynamic meters for simplicity */
      char* error;
      if (pmRegisterDerivedMetric(metric->name, value, &error) < 0) {
         char* note;
         xAsprintf(&note,
                   "%s: failed to parse expression in %s at line %u\n%s\n%s",
                   pmGetProgname(), path, line, error, pmGetProgname());
         free(error);
         errno = EINVAL;
         CRT_fatalError(note);
         free(note);
      }
   } else {
      /* this is a property of a dynamic metric - the metric expression */
      /* may not have been observed yet - i.e. we allow for any ordering */
      metric = PCPDynamicMeter_lookupMetric(meters, meter, key);
      if (String_eq(p, "color")) {
         if (String_eq(value, "gray"))
            metric->color = DYNAMIC_GRAY;
         else if (String_eq(value, "darkgray"))
            metric->color = DYNAMIC_DARKGRAY;
         else if (String_eq(value, "red"))
            metric->color = DYNAMIC_RED;
         else if (String_eq(value, "green"))
            metric->color = DYNAMIC_GREEN;
         else if (String_eq(value, "blue"))
            metric->color = DYNAMIC_BLUE;
         else if (String_eq(value, "cyan"))
            metric->color = DYNAMIC_CYAN;
         else if (String_eq(value, "magenta"))
            metric->color = DYNAMIC_MAGENTA;
         else if (String_eq(value, "yellow"))
            metric->color = DYNAMIC_YELLOW;
         else if (String_eq(value, "white"))
            metric->color = DYNAMIC_WHITE;
      } else if (String_eq(p, "label")) {
         char* label = String_cat(value, ": ");
         free_and_xStrdup(&metric->label, label);
         free(label);
      } else if (String_eq(p, "suffix")) {
         free_and_xStrdup(&metric->suffix, value);
      }
   }
}

// Ensure a valid name for use in a PCP metric name and in htoprc
static bool PCPDynamicMeter_validateMeterName(char* key, const char* path, unsigned int line) {
   char* p = key;
   char* end = strrchr(key, ']');

   if (end) {
      *end = '\0';
   } else {
      fprintf(stderr,
              "%s: no closing brace on meter name at %s line %u\n\"%s\"\n",
              pmGetProgname(), path, line, key);
      return false;
   }

   while (*p) {
      if (p == key) {
         if (!isalpha(*p) && *p != '_')
            break;
      } else {
         if (!isalnum(*p) && *p != '_')
            break;
      }
      p++;
   }
   if (*p != '\0') { /* badness */
      fprintf(stderr,
              "%s: invalid meter name at %s line %u\n\"%s\"\n",
              pmGetProgname(), path, line, key);
      return false;
   }
   return true;
}

// Ensure a meter name has not been defined previously
static bool PCPDynamicMeter_uniqueName(char* key, PCPDynamicMeters* meters) {
   return !DynamicMeter_search(meters->table, key, NULL);
}

static PCPDynamicMeter* PCPDynamicMeter_new(PCPDynamicMeters* meters, const char* name) {
   PCPDynamicMeter* meter = xCalloc(1, sizeof(*meter));
   String_safeStrncpy(meter->super.name, name, sizeof(meter->super.name));
   Hashtable_put(meters->table, ++meters->count, meter);
   return meter;
}

static void PCPDynamicMeter_parseFile(PCPDynamicMeters* meters, const char* path) {
   FILE* file = fopen(path, "r");
   if (!file)
      return;

   PCPDynamicMeter* meter = NULL;
   unsigned int lineno = 0;
   bool ok = true;
   for (;;) {
      char* line = String_readLine(file);
      if (!line)
         break;
      lineno++;

      /* cleanup whitespace, skip comment lines */
      char* trimmed = String_trim(line);
      free(line);
      if (trimmed[0] == '#' || trimmed[0] == '\0') {
         free(trimmed);
         continue;
      }

      size_t n;
      char** config = String_split(trimmed, '=', &n);
      free(trimmed);
      if (config == NULL)
         continue;

      char* key = String_trim(config[0]);
      char* value = n > 1 ? String_trim(config[1]) : NULL;
      if (key[0] == '[') {  /* new section heading - i.e. new meter */
         ok = PCPDynamicMeter_validateMeterName(key + 1, path, lineno);
         if (ok)
            ok = PCPDynamicMeter_uniqueName(key + 1, meters);
         if (ok)
            meter = PCPDynamicMeter_new(meters, key + 1);
      } else if (!ok) {
         ;  /* skip this one, we're looking for a new header */
      } else if (value && meter && String_eq(key, "caption")) {
         char* caption = String_cat(value, ": ");
         if (caption) {
            free_and_xStrdup(&meter->super.caption, caption);
            free(caption);
            caption = NULL;
         }
      } else if (value && meter && String_eq(key, "description")) {
         free_and_xStrdup(&meter->super.description, value);
      } else if (value && meter && String_eq(key, "type")) {
         if (String_eq(config[1], "bar"))
            meter->super.type = BAR_METERMODE;
         else if (String_eq(config[1], "text"))
            meter->super.type = TEXT_METERMODE;
         else if (String_eq(config[1], "graph"))
            meter->super.type = GRAPH_METERMODE;
         else if (String_eq(config[1], "led"))
            meter->super.type = LED_METERMODE;
      } else if (value && meter && String_eq(key, "maximum")) {
         meter->super.maximum = strtod(value, NULL);
      } else if (value && meter) {
         PCPDynamicMeter_parseMetric(meters, meter, path, lineno, key, value);
      }
      String_freeArray(config);
      free(value);
      free(key);
   }
   fclose(file);
}

static void PCPDynamicMeter_scanDir(PCPDynamicMeters* meters, char* path) {
   DIR* dir = opendir(path);
   if (!dir)
      return;

   struct dirent* dirent;
   while ((dirent = readdir(dir)) != NULL) {
      if (dirent->d_name[0] == '.')
         continue;

      char* file = String_cat(path, dirent->d_name);
      PCPDynamicMeter_parseFile(meters, file);
      free(file);
   }
   closedir(dir);
}

void PCPDynamicMeters_init(PCPDynamicMeters* meters) {
   const char* share = pmGetConfig("PCP_SHARE_DIR");
   const char* sysconf = pmGetConfig("PCP_SYSCONF_DIR");
   const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
   const char* override = getenv("PCP_HTOP_DIR");
   const char* home = getenv("HOME");
   char* path;

   meters->table = Hashtable_new(0, true);

   /* developer paths - PCP_HTOP_DIR=./pcp ./pcp-htop */
   if (override) {
      path = String_cat(override, "/meters/");
      PCPDynamicMeter_scanDir(meters, path);
      free(path);
   }

   /* next, search in home directory alongside htoprc */
   if (xdgConfigHome)
      path = String_cat(xdgConfigHome, "/htop/meters/");
   else if (home)
      path = String_cat(home, "/.config/htop/meters/");
   else
      path = NULL;
   if (path) {
      PCPDynamicMeter_scanDir(meters, path);
      free(path);
   }

   /* next, search in the system meters directory */
   path = String_cat(sysconf, "/htop/meters/");
   PCPDynamicMeter_scanDir(meters, path);
   free(path);

   /* next, try the readonly system meters directory */
   path = String_cat(share, "/htop/meters/");
   PCPDynamicMeter_scanDir(meters, path);
   free(path);
}

static void PCPDynamicMeter_free(ATTR_UNUSED ht_key_t key, void* value, ATTR_UNUSED void* data) {
   PCPDynamicMeter* meter = (PCPDynamicMeter*) value;
   for (size_t i = 0; i < meter->totalMetrics; i++) {
      free(meter->metrics[i].name);
      free(meter->metrics[i].label);
      free(meter->metrics[i].suffix);
   }
   free(meter->metrics);
   free(meter->super.caption);
   free(meter->super.description);
}

void PCPDynamicMeters_done(Hashtable* table) {
   Hashtable_foreach(table, PCPDynamicMeter_free, NULL);
}

void PCPDynamicMeter_enable(PCPDynamicMeter* this) {
   for (size_t i = 0; i < this->totalMetrics; i++)
      PCPMetric_enable(this->metrics[i].id, true);
}

void PCPDynamicMeter_updateValues(PCPDynamicMeter* this, Meter* meter) {
   char* buffer = meter->txtBuffer;
   size_t size = sizeof(meter->txtBuffer);
   size_t bytes = 0;

   for (size_t i = 0; i < this->totalMetrics; i++) {
      if (i > 0 && bytes < size - 1)
         buffer[bytes++] = '/';  /* separator */

      PCPDynamicMetric* metric = &this->metrics[i];
      const pmDesc* desc = PCPMetric_desc(metric->id);
      pmAtomValue atom, raw;

      if (!PCPMetric_values(metric->id, &raw, 1, desc->type)) {
         bytes--; /* clear the separator */
         continue;
      }

      pmUnits conv = desc->units;  /* convert to canonical units */
      if (conv.dimSpace)
         conv.scaleSpace = PM_SPACE_KBYTE;
      if (conv.dimTime)
         conv.scaleTime = PM_TIME_SEC;
      if (desc->type == PM_TYPE_STRING)
         atom = raw;
      else if (pmConvScale(desc->type, &raw, &desc->units, &atom, &conv) < 0) {
         bytes--; /* clear the separator */
         continue;
      }

      size_t saved = bytes;
      switch (desc->type) {
         case PM_TYPE_STRING:
            bytes += xSnprintf(buffer + bytes, size - bytes, "%s", atom.cp);
            free(atom.cp);
            break;
         case PM_TYPE_32:
            bytes += conv.dimSpace ?
               Meter_humanUnit(buffer + bytes, atom.l, size - bytes) :
               xSnprintf(buffer + bytes, size - bytes, "%d", atom.l);
            break;
         case PM_TYPE_U32:
            bytes += conv.dimSpace ?
               Meter_humanUnit(buffer + bytes, atom.ul, size - bytes) :
               xSnprintf(buffer + bytes, size - bytes, "%u", atom.ul);
            break;
         case PM_TYPE_64:
            bytes += conv.dimSpace ?
               Meter_humanUnit(buffer + bytes, atom.ll, size - bytes) :
               xSnprintf(buffer + bytes, size - bytes, "%lld", (long long) atom.ll);
            break;
         case PM_TYPE_U64:
            bytes += conv.dimSpace ?
               Meter_humanUnit(buffer + bytes, atom.ull, size - bytes) :
               xSnprintf(buffer + bytes, size - bytes, "%llu", (unsigned long long) atom.ull);
            break;
         case PM_TYPE_FLOAT:
            bytes += conv.dimSpace ?
               Meter_humanUnit(buffer + bytes, atom.f, size - bytes) :
               xSnprintf(buffer + bytes, size - bytes, "%.2f", (double) atom.f);
            break;
         case PM_TYPE_DOUBLE:
            bytes += conv.dimSpace ?
               Meter_humanUnit(buffer + bytes, atom.d, size - bytes) :
               xSnprintf(buffer + bytes, size - bytes, "%.2f", atom.d);
            break;
         default:
            break;
      }
      if (saved != bytes && metric->suffix)
         bytes += xSnprintf(buffer + bytes, size - bytes, "%s", metric->suffix);
   }
   if (!bytes)
      xSnprintf(buffer, size, "no data");
}

void PCPDynamicMeter_display(PCPDynamicMeter* this, ATTR_UNUSED const Meter* meter, RichString* out) {
   int nodata = 1;

   for (size_t i = 0; i < this->totalMetrics; i++) {
      PCPDynamicMetric* metric = &this->metrics[i];
      const pmDesc* desc = PCPMetric_desc(metric->id);
      pmAtomValue atom, raw;
      char buffer[64];

      if (!PCPMetric_values(metric->id, &raw, 1, desc->type))
         continue;

      pmUnits conv = desc->units;  /* convert to canonical units */
      if (conv.dimSpace)
         conv.scaleSpace = PM_SPACE_KBYTE;
      if (conv.dimTime)
         conv.scaleTime = PM_TIME_SEC;
      if (desc->type == PM_TYPE_STRING)
         atom = raw;
      else if (pmConvScale(desc->type, &raw, &desc->units, &atom, &conv) < 0)
         continue;

      nodata = 0;  /* we will use this metric so *some* data will be added */

      if (i > 0)
         RichString_appendnAscii(out, CRT_colors[metric->color], " ", 1);

      if (metric->label)
         RichString_appendAscii(out, CRT_colors[METER_TEXT], metric->label);

      int len = 0;
      switch (desc->type) {
         case PM_TYPE_STRING:
            len = xSnprintf(buffer, sizeof(buffer), "%s", atom.cp);
            free(atom.cp);
            break;
         case PM_TYPE_32:
            len = conv.dimSpace ?
               Meter_humanUnit(buffer, atom.l, sizeof(buffer)) :
               xSnprintf(buffer, sizeof(buffer), "%d", atom.l);
            break;
         case PM_TYPE_U32:
            len = conv.dimSpace ?
               Meter_humanUnit(buffer, atom.ul, sizeof(buffer)) :
               xSnprintf(buffer, sizeof(buffer), "%u", atom.ul);
            break;
         case PM_TYPE_64:
            len = conv.dimSpace ?
               Meter_humanUnit(buffer, atom.ll, sizeof(buffer)) :
               xSnprintf(buffer, sizeof(buffer), "%lld", (long long) atom.ll);
            break;
         case PM_TYPE_U64:
            len = conv.dimSpace ?
               Meter_humanUnit(buffer, atom.ull, sizeof(buffer)) :
               xSnprintf(buffer, sizeof(buffer), "%llu", (unsigned long long) atom.ull);
            break;
         case PM_TYPE_FLOAT:
            len = conv.dimSpace ?
               Meter_humanUnit(buffer, atom.f, sizeof(buffer)) :
               xSnprintf(buffer, sizeof(buffer), "%.2f", (double) atom.f);
            break;
         case PM_TYPE_DOUBLE:
            len = conv.dimSpace ?
               Meter_humanUnit(buffer, atom.d, sizeof(buffer)) :
               xSnprintf(buffer, sizeof(buffer), "%.2f", atom.d);
            break;
         default:
            break;
      }
      if (len) {
         RichString_appendnAscii(out, CRT_colors[metric->color], buffer, len);
         if (metric->suffix)
            RichString_appendAscii(out, CRT_colors[METER_TEXT], metric->suffix);
      }
   }
   if (nodata)
      RichString_writeAscii(out, CRT_colors[METER_VALUE_ERROR], "no data");
}
