/*
htop - PCPDynamicColumn.c
(C) 2021 Sohaib Mohammed
(C) 2021 htop dev team
(C) 2021 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/PCPDynamicColumn.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Macros.h"
#include "Platform.h"
#include "Process.h"
#include "ProcessList.h"
#include "RichString.h"
#include "XUtils.h"

#include "pcp/PCPProcess.h"
#include "pcp/PCPMetric.h"


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

static void PCPDynamicColumns_free(ATTR_UNUSED ht_key_t key, void* value, ATTR_UNUSED void* data) {
   PCPDynamicColumn* column = (PCPDynamicColumn*) value;
   free(column->metricName);
   free(column->super.heading);
   free(column->super.caption);
   free(column->super.description);
}

void PCPDynamicColumns_done(Hashtable* table) {
   Hashtable_foreach(table, PCPDynamicColumns_free, NULL);
}

void PCPDynamicColumn_writeField(PCPDynamicColumn* this, const Process* proc, RichString* str) {
   const PCPProcess* pp = (const PCPProcess*) proc;
   unsigned int type = PCPMetric_type(this->id);

   pmAtomValue atom;
   if (!PCPMetric_instance(this->id, proc->pid, pp->offset, &atom, type)) {
      RichString_appendAscii(str, CRT_colors[METER_VALUE_ERROR], "no data");
      return;
   }

   int width = this->super.width;
   if (!width || abs(width) > DYNAMIC_MAX_COLUMN_WIDTH)
      width = DYNAMIC_DEFAULT_COLUMN_WIDTH;
   int abswidth = abs(width);
   if (abswidth > DYNAMIC_MAX_COLUMN_WIDTH) {
      abswidth = DYNAMIC_MAX_COLUMN_WIDTH;
      width = -abswidth;
   }

   char buffer[DYNAMIC_MAX_COLUMN_WIDTH + /* space */ 1 + /* null terminator */ + 1];
   int attr = CRT_colors[DEFAULT_COLOR];
   switch (type) {
      case PM_TYPE_STRING:
         attr = CRT_colors[PROCESS_SHADOW];
         Process_printLeftAlignedField(str, attr, atom.cp, abswidth);
         free(atom.cp);
         break;
      case PM_TYPE_32:
         xSnprintf(buffer, sizeof(buffer), "%*d ", width, atom.l);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_U32:
         xSnprintf(buffer, sizeof(buffer), "%*u ", width, atom.ul);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_64:
         xSnprintf(buffer, sizeof(buffer), "%*lld ", width, (long long) atom.ll);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_U64:
         xSnprintf(buffer, sizeof(buffer), "%*llu ", width, (unsigned long long) atom.ull);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_FLOAT:
         xSnprintf(buffer, sizeof(buffer), "%*.2f ", width, (double) atom.f);
         RichString_appendAscii(str, attr, buffer);
         break;
      case PM_TYPE_DOUBLE:
         xSnprintf(buffer, sizeof(buffer), "%*.2f ", width, atom.d);
         RichString_appendAscii(str, attr, buffer);
         break;
      default:
         attr = CRT_colors[METER_VALUE_ERROR];
         RichString_appendAscii(str, attr, "no type");
         break;
   }
}

int PCPDynamicColumn_compareByKey(const PCPProcess* p1, const PCPProcess* p2, ProcessField key) {
   const PCPDynamicColumn* column = Hashtable_get(p1->super.processList->dynamicColumns, key);

   if (!column)
      return -1;

   size_t metric = column->id;
   unsigned int type = PCPMetric_type(metric);

   pmAtomValue atom1 = {0}, atom2 = {0};
   if (!PCPMetric_instance(metric, p1->super.pid, p1->offset, &atom1, type) ||
       !PCPMetric_instance(metric, p2->super.pid, p2->offset, &atom2, type)) {
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
         return SPACESHIP_NUMBER(atom2.f, atom1.f);
      case PM_TYPE_DOUBLE:
         return SPACESHIP_NUMBER(atom2.d, atom1.d);
      default:
         break;
   }
   return -1;
}
