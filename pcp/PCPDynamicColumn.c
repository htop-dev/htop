/*
htop - PCPDynamicColumn.c
(C) 2021 htop dev team
(C) 2021 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/
#include "config.h" // IWYU pragma: keep

#include "pcp/PCPDynamicColumn.h"

#include <math.h>

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "PCPProcess.h"
#include "Platform.h"
#include "ProcessList.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"


static PCPDynamicColumnMetric* PCPDynamicColumn_lookupMetric(PCPDynamicColumns* columns, PCPDynamicColumn* column, const char* name) {
   size_t bytes = 8 + strlen(column->super.name) + strlen(name);
   char* metricName = xMalloc(bytes);
   xSnprintf(metricName, bytes, "htop.%s.%s", column->super.name, name);

   PCPDynamicColumnMetric* metric;
   for (unsigned int i = 0; i < column->totalMetrics; i++) {
      metric = &column->metrics[i];
      if (String_eq(metric->name, metricName)) {
         free(metricName);
         return metric;
      }
   }

   /* not an existing metric in this column - add it */
   unsigned int n = column->totalMetrics + 1;
   column->metrics = xReallocArray(column->metrics, n, sizeof(PCPDynamicColumnMetric));
   column->totalMetrics = n;
   metric = &column->metrics[n-1];
   memset(metric, 0, sizeof(PCPDynamicColumnMetric));
   metric->name = metricName;
   metric->label = String_cat(name, ": ");
   metric->id = columns->offset + columns->cursor;
   columns->cursor++;

   Platform_addMetric(metric->id, metricName);

   return metric;
}

static void PCPDynamicColumn_parseMetric(PCPDynamicColumns* columns, PCPDynamicColumn* column, const char* path, unsigned int line, char* key, char* value) {
   PCPDynamicColumnMetric* metric;
   char* p;

   if ((p = strchr(key, '.')) == NULL)
      return;
   *p++ = '\0'; /* end the name, p is now the attribute, e.g. 'label' */

   if (String_eq(p, "metric")) {
      /* lookup a dynamic metric with this name, else create */
      metric = PCPDynamicColumn_lookupMetric(columns, column, key);

      /* use derived metrics in dynamic columns for simplicity */
      char* error;
      if (pmRegisterDerivedMetric(metric->name, value, &error) < 0) {
         char* note;
         xAsprintf(&note,
                   "%s: failed to parse expression in %s at line %u\n%s\n",
                   pmGetProgname(), path, line, error);
         free(error);
         errno = EINVAL;
         CRT_fatalError(note);
         free(note);
      }
   } else {
      /* this is a property of a dynamic metric - the metric expression */
      /* may not have been observed yet - i.e. we allow for any ordering */
      metric = PCPDynamicColumn_lookupMetric(columns, column, key);
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
static void PCPDynamicColumn_validateColumnName(char* key, const char* path, unsigned int line) {
   char* p = key;
   char* end = strrchr(key, ']');

   if (end) {
      *end = '\0';
   } else {
      char* note;
      xAsprintf(&note,
                "%s: no closing brace on column....... name at %s line %u\n\"%s\"",
                pmGetProgname(), path, line, key);
      errno = EINVAL;
      CRT_fatalError(note);
      free(note);
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
      char* note;
      xAsprintf(&note,
                "%s: invalid column....... name at %s line %u\n\"%s\"",
                pmGetProgname(), path, line, key);
      errno = EINVAL;
      CRT_fatalError(note);
      free(note);
   } else { /* overwrite closing brace */
      *p = '\0';
   }
}

static PCPDynamicColumn* PCPDynamicColumn_new(PCPDynamicColumns* columns, const char* name) {
   PCPDynamicColumn* column = xCalloc(1, sizeof(*column));
   String_safeStrncpy(column->super.name, name, sizeof(column->super.name));
   Hashtable_put(columns->table, ++columns->count, column);
   return column;
}

static void PCPDynamicColumn_parseFile(PCPDynamicColumns* columns, const char* path) {
   FILE* file = fopen(path, "r");
   if (!file)
      return;

   PCPDynamicColumn* column = NULL;
   unsigned int lineno = 0;
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
      if (key[0] == '[') {  /* new section heading - i.e. new column */
         PCPDynamicColumn_validateColumnName(key+1, path, lineno);
         column = PCPDynamicColumn_new(columns, key+1);
      } else if (value && String_eq(key, "caption")) {
         free_and_xStrdup(&column->super.caption, value);
      } else if (value && String_eq(key, "description")) {
         free_and_xStrdup(&column->super.description, value);
      } else if (value && String_eq(key, "width")) {
         column->super.width = strtoul(value, NULL, 10);
      } else if (value && String_eq(key, "type")) { // FIXME
         if (String_eq(config[1], "bar"))
             column->super.type = BAR_METERMODE;
         else if (String_eq(config[1], "text"))
             column->super.type = TEXT_METERMODE;
         else if (String_eq(config[1], "graph"))
             column->super.type = GRAPH_METERMODE;
         else if (String_eq(config[1], "led"))
             column->super.type = LED_METERMODE;
      } else if (value && String_eq(key, "maximum")) {
         column->super.maximum = strtod(value, NULL);
      } else if (value) {
         PCPDynamicColumn_parseMetric(columns, column, path, lineno, key, value);
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
   const char* sysconf = pmGetConfig("PCP_SYSCONF_DIR");
   const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
   const char* home = getenv("HOME");
   char* path;

   columns->table = Hashtable_new(0, true);

   /* search in the users home directory first of all */
   if (xdgConfigHome) {
      path = String_cat(xdgConfigHome, "/htop/columns/");
   } else {
      if (!home)
         home = "";
      path = String_cat(home, "/.config/htop/columns/");
   }
   PCPDynamicColumn_scanDir(columns, path);
   free(path);

   /* secondly search in the system columns directory */
   path = String_cat(sysconf, "/htop/columns/");
   PCPDynamicColumn_scanDir(columns, path);
   free(path);

   /* check the working directory, as a final option */
   char cwd[PATH_MAX];
   if (getcwd(cwd, sizeof(cwd)) != NULL) {
      path = String_cat(cwd, "/pcp/columns/");
      PCPDynamicColumn_scanDir(columns, path);
      free(path);
   }
}

void PCPDynamicColumn_writeField(PCPDynamicColumn* this, const Process* proc, RichString* str, int field) {
   const PCPProcess* pp = (const PCPProcess*) proc;
   char buffer[256];
   size_t size = sizeof(buffer);
   int width = this->super.width;
   PCPDynamicColumnMetric* metric = &this->metrics[0];
   const pmDesc* desc = Metric_desc(metric->id);
   int attr = CRT_colors[DEFAULT_COLOR];
   int index = field-LAST_STATIC_PROCESSFIELD-1;

   attr = CRT_colors[metric->color];
   switch (desc->type) {
      case PM_TYPE_STRING:
         Process_printLeftAlignedField(str, attr, pp->dc[index].cp, width);
         break;
      case PM_TYPE_32:
         xSnprintf(buffer, size, "%*d", width, pp->dc[index].l);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_U32:
         xSnprintf(buffer, size, "%*u", width, pp->dc[index].ul);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_64:
         xSnprintf(buffer, size, "%*lld", width, (long long) pp->dc[index].ll);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_U64:
         xSnprintf(buffer, size, "%*llu", width, (unsigned long long) pp->dc[index].ull);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_FLOAT:
         xSnprintf(buffer, size, "%*.2f", width, (double) pp->dc[index].f);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_DOUBLE:
         xSnprintf(buffer, size, "%*.2f", width, pp->dc[index].d);
         RichString_appendAscii(str, attr, buffer);
         break;
      default:
         RichString_appendAscii(str, CRT_colors[METER_VALUE_ERROR], "no data");
         break;
   }
 }

int PCPDynamicColumn_compareByKey(const PCPProcess* p1, const PCPProcess* p2, ProcessField key) {
   int index = key-LAST_STATIC_PROCESSFIELD-1;
   int metricOffset = Platform_getColumnOffset();
   int type = Metric_type(metricOffset+index);
   switch (type) {
      case PM_TYPE_STRING:
         return SPACESHIP_NULLSTR(p2->dc[index].cp, p1->dc[index].cp);
      case PM_TYPE_32:
         return SPACESHIP_NUMBER(p2->dc[index].l, p1->dc[index].l);
      case PM_TYPE_U32:
         return SPACESHIP_NUMBER(p2->dc[index].ul, p1->dc[index].ul);
      case PM_TYPE_64:
         return SPACESHIP_NUMBER(p2->dc[index].ll, p1->dc[index].ll);
      case PM_TYPE_U64:
         return SPACESHIP_NUMBER(p2->dc[index].ull, p1->dc[index].ull);
      case PM_TYPE_FLOAT:
         return SPACESHIP_NUMBER(p2->dc[index].f, p1->dc[index].f);
      case PM_TYPE_DOUBLE:
         return SPACESHIP_NUMBER(p2->dc[index].d, p1->dc[index].d);
      default:
         break;
   }
   return -1;
}
