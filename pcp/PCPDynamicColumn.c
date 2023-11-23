/*
htop - PCPDynamicColumn.c
(C) 2021-2023 Sohaib Mohammed
(C) 2021-2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/PCPDynamicColumn.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Macros.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessTable.h"
#include "RichString.h"
#include "XUtils.h"

#include "linux/CGroupUtils.h"
#include "pcp/Metric.h"
#include "pcp/PCPProcess.h"


static bool PCPDynamicColumn_addMetric(PCPDynamicColumns* columns, PCPDynamicColumn* column) {
   if (!column->super.name[0])
      return false;

   size_t bytes = 16 + strlen(column->super.name);
   char* metricName = xMalloc(bytes);
   xSnprintf(metricName, bytes, "htop.column.%s", column->super.name);

   column->metricName = metricName;
   column->id = columns->offset + columns->cursor;
   columns->cursor++;

   Platform_addMetric(column->id, metricName);
   return true;
}

static void PCPDynamicColumn_parseMetric(PCPDynamicColumns* columns, PCPDynamicColumn* column, const char* path, unsigned int line, char* value) {
   /* pmLookupText */
   if (!column->super.description)
      Metric_lookupText(value, &column->super.description);

   /* lookup a dynamic metric with this name, else create */
   if (PCPDynamicColumn_addMetric(columns, column) == false)
      return;

   /* derived metrics in all dynamic columns for simplicity */
   char* error;
   if (pmRegisterDerivedMetric(column->metricName, value, &error) < 0) {
      char* note;
      xAsprintf(&note,
                "%s: failed to parse expression in %s at line %u\n%s\n",
                pmGetProgname(), path, line, error);
      free(error);
      errno = EINVAL;
      CRT_fatalError(note);
      free(note);
   }
}

// Ensure a valid name for use in a PCP metric name and in htoprc
static bool PCPDynamicColumn_validateColumnName(char* key, const char* path, unsigned int line) {
   char* p = key;
   char* end = strrchr(key, ']');

   if (end) {
      *end = '\0';
   } else {
      fprintf(stderr,
                "%s: no closing brace on column name at %s line %u\n\"%s\"",
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
                "%s: invalid column name at %s line %u\n\"%s\"",
                pmGetProgname(), path, line, key);
      return false;
   }
   return true;
}

// Ensure a column name has not been defined previously
static bool PCPDynamicColumn_uniqueName(char* key, PCPDynamicColumns* columns) {
   return DynamicColumn_search(columns->table, key, NULL) == NULL;
}

static PCPDynamicColumn* PCPDynamicColumn_new(PCPDynamicColumns* columns, const char* name) {
   PCPDynamicColumn* column = xCalloc(1, sizeof(*column));
   String_safeStrncpy(column->super.name, name, sizeof(column->super.name));
   column->super.enabled = false;
   column->percent = false;
   column->instances = false;
   column->defaultEnabled = true;

   size_t id = columns->count + LAST_PROCESSFIELD;
   Hashtable_put(columns->table, id, column);
   columns->count++;

   return column;
}

static void PCPDynamicColumn_parseFile(PCPDynamicColumns* columns, const char* path) {
   FILE* file = fopen(path, "r");
   if (!file)
      return;

   PCPDynamicColumn* column = NULL;
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
      if (!trimmed || !trimmed[0] || trimmed[0] == '#') {
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
      if (key[0] == '[') {  /* new section heading - i.e. new column */
         ok = PCPDynamicColumn_validateColumnName(key + 1, path, lineno);
         if (ok)
            ok = PCPDynamicColumn_uniqueName(key + 1, columns);
         if (ok)
            column = PCPDynamicColumn_new(columns, key + 1);
      } else if (value && column && String_eq(key, "caption")) {
         free_and_xStrdup(&column->super.caption, value);
      } else if (value && column && String_eq(key, "heading")) {
         free_and_xStrdup(&column->super.heading, value);
      } else if (value && column && String_eq(key, "description")) {
         free_and_xStrdup(&column->super.description, value);
      } else if (value && column && String_eq(key, "width")) {
         column->super.width = strtoul(value, NULL, 10);
      } else if (value && column && String_eq(key, "format")) {
         free_and_xStrdup(&column->format, value);
      } else if (value && column && String_eq(key, "instances")) {
         if (String_eq(value, "True") || String_eq(value, "true"))
            column->instances = true;
      } else if (value && column && (String_eq(key, "default") || String_eq(key, "enabled"))) {
         if (String_eq(value, "False") || String_eq(value, "false"))
            column->defaultEnabled = false;
      } else if (value && column && String_eq(key, "metric")) {
         PCPDynamicColumn_parseMetric(columns, column, path, lineno, value);
      }
      String_freeArray(config);
      free(value);
      free(key);
   }
   fclose(file);
}

static void PCPDynamicColumn_scanDir(PCPDynamicColumns* columns, char* path) {
   DIR* dir = opendir(path);
   if (!dir)
      return;

   struct dirent* dirent;
   while ((dirent = readdir(dir)) != NULL) {
      if (dirent->d_name[0] == '.')
         continue;

      char* file = String_cat(path, dirent->d_name);
      PCPDynamicColumn_parseFile(columns, file);
      free(file);
   }
   closedir(dir);
}

void PCPDynamicColumns_init(PCPDynamicColumns* columns) {
   const char* share = pmGetConfig("PCP_SHARE_DIR");
   const char* sysconf = pmGetConfig("PCP_SYSCONF_DIR");
   const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
   const char* override = getenv("PCP_HTOP_DIR");
   const char* home = getenv("HOME");
   char* path;

   if (!xdgConfigHome && !home) {
      const struct passwd* pw = getpwuid(getuid());
      if (pw)
         home = pw->pw_dir;
   }

   columns->table = Hashtable_new(0, true);

   /* developer paths - PCP_HTOP_DIR=./pcp ./pcp-htop */
   if (override) {
      path = String_cat(override, "/columns/");
      PCPDynamicColumn_scanDir(columns, path);
      free(path);
   }

   /* next, search in home directory alongside htoprc */
   if (xdgConfigHome)
      path = String_cat(xdgConfigHome, "/htop/columns/");
   else if (home)
      path = String_cat(home, "/.config/htop/columns/");
   else
      path = NULL;
   if (path) {
      PCPDynamicColumn_scanDir(columns, path);
      free(path);
   }

   /* next, search in the system columns directory */
   path = String_cat(sysconf, "/htop/columns/");
   PCPDynamicColumn_scanDir(columns, path);
   free(path);

   /* next, try the readonly system columns directory */
   path = String_cat(share, "/htop/columns/");
   PCPDynamicColumn_scanDir(columns, path);
   free(path);
}

void PCPDynamicColumn_done(PCPDynamicColumn* this) {
   DynamicColumn_done(&this->super);
   free(this->metricName);
   free(this->format);
}

static void PCPDynamicColumns_free(ATTR_UNUSED ht_key_t key, void* value, ATTR_UNUSED void* data) {
   PCPDynamicColumn* column = (PCPDynamicColumn*) value;
   PCPDynamicColumn_done(column);
}

void PCPDynamicColumns_done(Hashtable* table) {
   Hashtable_foreach(table, PCPDynamicColumns_free, NULL);
}

static void PCPDynamicColumn_setupWidth(ATTR_UNUSED ht_key_t key, void* value, ATTR_UNUSED void* data) {
   PCPDynamicColumn* column = (PCPDynamicColumn*) value;

   /* calculate column size based on config file and metric units */
   const pmDesc* desc = Metric_desc(column->id);

   if (column->instances || desc->type == PM_TYPE_STRING) {
      column->super.width = column->width;
      if (column->super.width == 0)
         column->super.width = -16;
      return;
   }

   if (column->format) {
      if (strcmp(column->format, "percent") == 0) {
         column->super.width = 5;
         return;
      }
      if (strcmp(column->format, "process") == 0) {
         column->super.width = Process_pidDigits;
         return;
      }
   }

   if (column->width) {
      column->super.width = column->width;
      return;
   }

   pmUnits units = desc->units;
   if (units.dimSpace && units.dimTime)
      column->super.width = 11; // Row_printRate
   else if (units.dimSpace)
      column->super.width = 5;  // Row_printBytes
   else if (units.dimCount && units.dimTime)
      column->super.width = 11;  // Row_printCount
   else if (units.dimTime)
      column->super.width = 8;  // Row_printTime
   else
      column->super.width = 11; // Row_printCount
}

void PCPDynamicColumns_setupWidths(PCPDynamicColumns* columns) {
   Hashtable_foreach(columns->table, PCPDynamicColumn_setupWidth, NULL);
}

/* normalize output units to bytes and seconds */
static int PCPDynamicColumn_normalize(const pmDesc* desc, const pmAtomValue* ap, double* value) {
   /* form normalized units based on the original metric units */
   pmUnits units = desc->units;
   if (units.dimTime)
      units.scaleTime = PM_TIME_SEC;
   if (units.dimSpace)
      units.scaleSpace = PM_SPACE_BYTE;
   if (units.dimCount)
      units.scaleCount = PM_COUNT_ONE;

   pmAtomValue atom;
   int sts, type = desc->type;
   if ((sts = pmConvScale(type, ap, &desc->units, &atom, &units)) < 0)
      return sts;

   switch (type) {
      case PM_TYPE_32:
         *value = (double) atom.l;
         break;
      case PM_TYPE_U32:
         *value = (double) atom.ul;
         break;
      case PM_TYPE_64:
         *value = (double) atom.ll;
         break;
      case PM_TYPE_U64:
         *value = (double) atom.ull;
         break;
      case PM_TYPE_FLOAT:
         *value = (double) atom.f;
         break;
      case PM_TYPE_DOUBLE:
         *value = atom.d;
         break;
      default:
         return PM_ERR_CONV;
   }

   return 0;
}

void PCPDynamicColumn_writeAtomValue(PCPDynamicColumn* column, RichString* str, const struct Settings_* settings, int metric, int instance, const pmDesc* desc, const void* atom) {
   const pmAtomValue* atomvalue = (const pmAtomValue*) atom;
   char buffer[DYNAMIC_MAX_COLUMN_WIDTH + /*space*/ 1 + /*null*/ 1];
   int attr = CRT_colors[DEFAULT_COLOR];
   int width = column->super.width;
   int n;

   if (!width || abs(width) > DYNAMIC_MAX_COLUMN_WIDTH)
      width = DYNAMIC_DEFAULT_COLUMN_WIDTH;
   int abswidth = abs(width);
   if (abswidth > DYNAMIC_MAX_COLUMN_WIDTH) {
      abswidth = DYNAMIC_MAX_COLUMN_WIDTH;
      width = -abswidth;
   }

   if (atomvalue == NULL) {
      n = xSnprintf(buffer, sizeof(buffer), "%*.*s ", width, abswidth, "N/A");
      RichString_appendnAscii(str, CRT_colors[PROCESS_SHADOW], buffer, n);
      return;
   }

   /* deal with instance names and metrics with string values first */
   if (column->instances || desc->type == PM_TYPE_STRING) {
      char* value = NULL;
      char* dupd1 = NULL;
      if (column->instances) {
         attr = CRT_colors[DYNAMIC_GRAY];
         Metric_externalName(metric, instance, &dupd1);
         value = dupd1;
      } else {
         attr = CRT_colors[DYNAMIC_GREEN];
         value = atomvalue->cp;
      }
      if (column->format && value) {
         char* dupd2 = NULL;
         if (strcmp(column->format, "command") == 0)
            attr = CRT_colors[PROCESS_COMM];
         else if (strcmp(column->format, "process") == 0)
            attr = CRT_colors[PROCESS_SHADOW];
         else if (strcmp(column->format, "device") == 0 && strncmp(value, "/dev/", 5) == 0)
            value += 5;
         else if (strcmp(column->format, "cgroup") == 0 && (dupd2 = CGroup_filterName(value)))
            value = dupd2;
         n = xSnprintf(buffer, sizeof(buffer), "%*.*s ", width, abswidth, value);
         if (dupd2)
            free(dupd2);
      } else if (value) {
         n = xSnprintf(buffer, sizeof(buffer), "%*.*s ", width, abswidth, value);
      } else {
         n = xSnprintf(buffer, sizeof(buffer), "%*.*s ", width, abswidth, "N/A");
      }
      if (dupd1)
         free(dupd1);
      RichString_appendnAscii(str, attr, buffer, n);
      return;
   }

   /* deal with any numeric value - first, normalize units to bytes/seconds */
   double value;
   if (PCPDynamicColumn_normalize(desc, atomvalue, &value) < 0) {
      n = xSnprintf(buffer, sizeof(buffer), "%*.*s ", width, abswidth, "no conv");
      RichString_appendnAscii(str, CRT_colors[METER_VALUE_ERROR], buffer, n);
      return;
   }

   if (column->format) {
      if (strcmp(column->format, "percent") == 0) {
         n = Row_printPercentage(value, buffer, sizeof(buffer), width, &attr);
         RichString_appendnAscii(str, attr, buffer, n);
         return;
      }
      if (strcmp(column->format, "process") == 0) {
         n = xSnprintf(buffer, sizeof(buffer), "%*d ", Row_pidDigits, (int)value);
         RichString_appendnAscii(str, attr, buffer, n);
         return;
      }
   }

   /* width overrides unit suffix and coloring; too complex for a corner case */
   if (column->width) {
      if (value - (unsigned long long)value > 0)  /* display floating point */
         n = xSnprintf(buffer, sizeof(buffer), "%*.2f ", width, value);
      else   /* display as integer */
         n = xSnprintf(buffer, sizeof(buffer), "%*llu ", width, (unsigned long long)value);
      RichString_appendnAscii(str, CRT_colors[PROCESS], buffer, n);
      return;
   }

   bool coloring = settings->highlightMegabytes;
   pmUnits units = desc->units;
   if (units.dimSpace && units.dimTime)
      Row_printRate(str, value, coloring);
   else if (units.dimSpace)
      Row_printBytes(str, value, coloring);
   else if (units.dimCount)
      Row_printCount(str, value, coloring);
   else if (units.dimTime)
      Row_printTime(str, value / 10 /* hundreds of a second */, coloring);
   else
      Row_printCount(str, value, 0);  /* e.g. PID */
}

void PCPDynamicColumn_writeField(PCPDynamicColumn* this, const Process* proc, RichString* str) {
   const Settings* settings = proc->super.host->settings;
   const PCPProcess* pp = (const PCPProcess*) proc;
   const pmDesc* desc = Metric_desc(this->id);
   pid_t pid = Process_getPid(proc);

   pmAtomValue atom;
   pmAtomValue* ap = &atom;
   if (!Metric_instance(this->id, pid, pp->offset, ap, desc->type))
      ap = NULL;

   PCPDynamicColumn_writeAtomValue(this, str, settings, this->id, pid, desc, ap);
}

int PCPDynamicColumn_compareByKey(const PCPProcess* p1, const PCPProcess* p2, ProcessField key) {
   const Process* proc = &p1->super;
   const Settings* settings = proc->super.host->settings;
   const PCPDynamicColumn* column = Hashtable_get(settings->dynamicColumns, key);

   if (!column)
      return -1;

   size_t metric = column->id;
   unsigned int type = Metric_type(metric);

   pmAtomValue atom1 = {0}, atom2 = {0};
   if (!Metric_instance(metric, Process_getPid(&p1->super), p1->offset, &atom1, type) ||
       !Metric_instance(metric, Process_getPid(&p2->super), p2->offset, &atom2, type)) {
      if (type == PM_TYPE_STRING) {
         free(atom1.cp);
         free(atom2.cp);
      }
      return -1;
   }

   switch (type) {
      case PM_TYPE_STRING: {
         int cmp = SPACESHIP_NULLSTR(atom2.cp, atom1.cp);
         free(atom2.cp);
         free(atom1.cp);
         return cmp;
      }
      case PM_TYPE_32:
         return SPACESHIP_NUMBER(atom2.l, atom1.l);
      case PM_TYPE_U32:
         return SPACESHIP_NUMBER(atom2.ul, atom1.ul);
      case PM_TYPE_64:
         return SPACESHIP_NUMBER(atom2.ll, atom1.ll);
      case PM_TYPE_U64:
         return SPACESHIP_NUMBER(atom2.ull, atom1.ull);
      case PM_TYPE_FLOAT:
         return compareRealNumbers(atom2.f, atom1.f);
      case PM_TYPE_DOUBLE:
         return compareRealNumbers(atom2.d, atom1.d);
      default:
         break;
   }

   return -1;
}
