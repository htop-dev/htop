/*
htop - Row.c
(C) 2004-2015 Hisham H. Muhammad
(C) 2020-2023 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Row.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Machine.h"
#include "Macros.h"
#include "Process.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"


int Row_pidDigits = ROW_MIN_PID_DIGITS;
int Row_uidDigits = ROW_MIN_UID_DIGITS;

void Row_init(Row* this, const Machine* host) {
   this->host = host;
   this->tag = false;
   this->showChildren = true;
   this->show = true;
   this->wasShown = false;
   this->updated = false;
}

void Row_done(Row* this) {
   assert(this != NULL);
   (void) this;
}

static inline bool Row_isNew(const Row* this) {
   const Machine* host = this->host;
   if (host->monotonicMs < this->seenStampMs)
      return false;

   const Settings* settings = host->settings;
   return host->monotonicMs - this->seenStampMs <= 1000 * (uint64_t)settings->highlightDelaySecs;
}

static inline bool Row_isTomb(const Row* this) {
   return this->tombStampMs > 0;
}

void Row_display(const Object* cast, RichString* out) {
   const Row* this = (const Row*) cast;
   const Settings* settings = this->host->settings;
   const RowField* fields = settings->ss->fields;

   for (int i = 0; fields[i]; i++)
      As_Row(this)->writeField(this, out, fields[i]);

   if (Row_isHighlighted(this))
      RichString_setAttr(out, CRT_colors[PROCESS_SHADOW]);

   if (this->tag == true)
      RichString_setAttr(out, CRT_colors[PROCESS_TAG]);

   if (settings->highlightChanges) {
      if (Row_isTomb(this))
         out->highlightAttr = CRT_colors[PROCESS_TOMB];
      else if (Row_isNew(this))
         out->highlightAttr = CRT_colors[PROCESS_NEW];
   }

   assert(RichString_size(out) > 0);
}

void Row_setPidColumnWidth(pid_t maxPid) {
   if (maxPid < (int)pow(10, ROW_MIN_PID_DIGITS)) {
      Row_pidDigits = ROW_MIN_PID_DIGITS;
      return;
   }

   Row_pidDigits = (int)log10(maxPid) + 1;
   assert(Row_pidDigits <= ROW_MAX_PID_DIGITS);
}

void Row_setUidColumnWidth(uid_t maxUid) {
   if (maxUid < (uid_t)pow(10, ROW_MIN_UID_DIGITS)) {
      Row_uidDigits = ROW_MIN_UID_DIGITS;
      return;
   }

   Row_uidDigits = (int)log10(maxUid) + 1;
   assert(Row_uidDigits <= ROW_MAX_UID_DIGITS);
}

uint8_t Row_fieldWidths[LAST_PROCESSFIELD] = { 0 };

void Row_resetFieldWidths(void) {
   for (size_t i = 0; i < LAST_PROCESSFIELD; i++) {
      if (!Process_fields[i].autoWidth)
         continue;

      size_t len = strlen(Process_fields[i].title);
      assert(len <= UINT8_MAX);
      Row_fieldWidths[i] = (uint8_t)len;
   }
}

void Row_updateFieldWidth(RowField key, size_t width) {
   if (width > UINT8_MAX)
      Row_fieldWidths[key] = UINT8_MAX;
   else if (width > Row_fieldWidths[key])
      Row_fieldWidths[key] = (uint8_t)width;
}

// helper function to fill an aligned title string for a dynamic column
static const char* alignedTitleDynamicColumn(const Settings* settings, int key, char* titleBuffer, size_t titleBufferSize) {
   const DynamicColumn* column = Hashtable_get(settings->dynamicColumns, key);
   if (column == NULL)
      return "- ";

   int width = column->width;
   if (!width || abs(width) > DYNAMIC_MAX_COLUMN_WIDTH)
      width = DYNAMIC_DEFAULT_COLUMN_WIDTH;

   xSnprintf(titleBuffer, titleBufferSize, "%*s ", width, column->heading);
   return titleBuffer;
}

// helper function to fill an aligned title string for a process field
static const char* alignedTitleProcessField(ProcessField field, char* titleBuffer, size_t titleBufferSize) {
   const char* title = Process_fields[field].title;
   if (!title)
      return "- ";

   if (Process_fields[field].pidColumn) {
      xSnprintf(titleBuffer, titleBufferSize, "%*s ", Row_pidDigits, title);
      return titleBuffer;
   }

   if (field == ST_UID) {
      xSnprintf(titleBuffer, titleBufferSize, "%*s ", Row_uidDigits, title);
      return titleBuffer;
   }

   if (Process_fields[field].autoWidth) {
      if (field == PERCENT_CPU)
         xSnprintf(titleBuffer, titleBufferSize, "%*s ", Row_fieldWidths[field], title);
      else
         xSnprintf(titleBuffer, titleBufferSize, "%-*.*s ", Row_fieldWidths[field], Row_fieldWidths[field], title);
      return titleBuffer;
   }

   return title;
}

// helper function to create an aligned title string for a given field
const char* RowField_alignedTitle(const Settings* settings, RowField field) {
   static char titleBuffer[UINT8_MAX + sizeof(" ")];
   assert(sizeof(titleBuffer) >= DYNAMIC_MAX_COLUMN_WIDTH + sizeof(" "));
   assert(sizeof(titleBuffer) >= ROW_MAX_PID_DIGITS + sizeof(" "));
   assert(sizeof(titleBuffer) >= ROW_MAX_UID_DIGITS + sizeof(" "));

   if (field < LAST_PROCESSFIELD)
      return alignedTitleProcessField((ProcessField)field, titleBuffer, sizeof(titleBuffer));
   return alignedTitleDynamicColumn(settings, field, titleBuffer, sizeof(titleBuffer));
}

RowField RowField_keyAt(const Settings* settings, int at) {
   const RowField* fields = (const RowField*) settings->ss->fields;
   RowField field;
   int x = 0;
   for (int i = 0; (field = fields[i]); i++) {
      int len = strlen(RowField_alignedTitle(settings, field));
      if (at >= x && at <= x + len) {
         return field;
      }
      x += len;
   }
   return COMM;
}

void Row_printKBytes(RichString* str, unsigned long long number, bool coloring) {
   char buffer[16];
   int len;

   int color = CRT_colors[PROCESS];
   int nextUnitColor = CRT_colors[PROCESS];

   const int colors[4] = {
      [0] = CRT_colors[PROCESS],
      [1] = CRT_colors[PROCESS_MEGABYTES],
      [2] = CRT_colors[PROCESS_GIGABYTES],
      [3] = CRT_colors[LARGE_NUMBER]
   };

   if (number == ULLONG_MAX)
      goto invalidNumber;

   if (coloring) {
      color = colors[0];
      nextUnitColor = colors[1];
   }

   if (number < 1000) {
      // Plain number, no markings
      len = xSnprintf(buffer, sizeof(buffer), "%5u ", (unsigned int)number);
      RichString_appendnAscii(str, color, buffer, len);
      return;
   }

   if (number < 100000) {
      // 2 digits for M, 3 digits for K
      len = xSnprintf(buffer, sizeof(buffer), "%2u", (unsigned int)(number / 1000));
      RichString_appendnAscii(str, nextUnitColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%03u ", (unsigned int)(number % 1000));
      RichString_appendnAscii(str, color, buffer, len);
      return;
   }

   // 100000 KiB (97.6 MiB) or greater. A unit prefix would be added.
   const size_t maxUnitIndex = (sizeof(number) * CHAR_BIT - 1) / 10 + 1;
   const bool canOverflow = maxUnitIndex >= ARRAYSIZE(unitPrefixes);

   size_t i = 1;
   int prevUnitColor;
   // Convert KiB to (1/100) of MiB
   unsigned long long hundredths = (number / 256) * 25 + (number % 256) * 25 / 256;
   while (true) {
      if (canOverflow && i >= ARRAYSIZE(unitPrefixes))
         goto invalidNumber;

      prevUnitColor = color;
      color = nextUnitColor;

      if (coloring && i + 1 < ARRAYSIZE(colors))
         nextUnitColor = colors[i + 1];

      if (hundredths < 1000000)
         break;

      hundredths /= ONE_K;
      i++;
   }

   number = hundredths / 100;
   hundredths %= 100;
   if (number < 100) {
      if (number < 10) {
         // 1 digit + decimal point + 2 digits
         // "9.76G", "9.99G", "9.76T", "9.99T", etc.
         len = xSnprintf(buffer, sizeof(buffer), "%1u", (unsigned int)number);
         RichString_appendnAscii(str, color, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), ".%02u", (unsigned int)hundredths);
      } else {
         // 2 digits + decimal point + 1 digit
         // "97.6M", "99.9M", "10.0G", "99.9G", etc.
         len = xSnprintf(buffer, sizeof(buffer), "%2u", (unsigned int)number);
         RichString_appendnAscii(str, color, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), ".%1u", (unsigned int)hundredths / 10);
      }
      RichString_appendnAscii(str, prevUnitColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%c ", unitPrefixes[i]);
   } else if (number < 1000) {
      // 3 digits
      // "100M", "999M", "100G", "999G", etc.
      len = xSnprintf(buffer, sizeof(buffer), "%4u%c ", (unsigned int)number, unitPrefixes[i]);
   } else {
      // 1 digit + 3 digits
      // "1000M", "9999M", "1000G", "9999G", etc.
      assert(number < 10000);

      len = xSnprintf(buffer, sizeof(buffer), "%1u", (unsigned int)number / 1000);
      RichString_appendnAscii(str, nextUnitColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%03u%c ", (unsigned int)number % 1000, unitPrefixes[i]);
   }
   RichString_appendnAscii(str, color, buffer, len);
   return;

invalidNumber:
   if (coloring)
      color = CRT_colors[PROCESS_SHADOW];

   RichString_appendAscii(str, color, "  N/A ");
   return;
}

void Row_printBytes(RichString* str, unsigned long long number, bool coloring) {
   if (number == ULLONG_MAX)
      Row_printKBytes(str, ULLONG_MAX, coloring);
   else
      Row_printKBytes(str, number / ONE_K, coloring);
}

void Row_printCount(RichString* str, unsigned long long number, bool coloring) {
   char buffer[13];

   int largeNumberColor = coloring ? CRT_colors[LARGE_NUMBER] : CRT_colors[PROCESS];
   int megabytesColor = coloring ? CRT_colors[PROCESS_MEGABYTES] : CRT_colors[PROCESS];
   int shadowColor = coloring ? CRT_colors[PROCESS_SHADOW] : CRT_colors[PROCESS];
   int baseColor = CRT_colors[PROCESS];

   if (number == ULLONG_MAX) {
      RichString_appendAscii(str, CRT_colors[PROCESS_SHADOW], "        N/A ");
   } else if (number >= 100000LL * ONE_DECIMAL_T) {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number / ONE_DECIMAL_G);
      RichString_appendnAscii(str, largeNumberColor, buffer, 12);
   } else if (number >= 100LL * ONE_DECIMAL_T) {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number / ONE_DECIMAL_M);
      RichString_appendnAscii(str, largeNumberColor, buffer, 8);
      RichString_appendnAscii(str, megabytesColor, buffer + 8, 4);
   } else if (number >= 10LL * ONE_DECIMAL_G) {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number / ONE_DECIMAL_K);
      RichString_appendnAscii(str, largeNumberColor, buffer, 5);
      RichString_appendnAscii(str, megabytesColor, buffer + 5, 3);
      RichString_appendnAscii(str, baseColor, buffer + 8, 4);
   } else {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number);
      RichString_appendnAscii(str, largeNumberColor, buffer, 2);
      RichString_appendnAscii(str, megabytesColor, buffer + 2, 3);
      RichString_appendnAscii(str, baseColor, buffer + 5, 3);
      RichString_appendnAscii(str, shadowColor, buffer + 8, 4);
   }
}

void Row_printTime(RichString* str, unsigned long long totalHundredths, bool coloring) {
   char buffer[10];
   int len;

   int yearColor = coloring ? CRT_colors[LARGE_NUMBER]      : CRT_colors[PROCESS];
   int dayColor  = coloring ? CRT_colors[PROCESS_GIGABYTES] : CRT_colors[PROCESS];
   int hourColor = coloring ? CRT_colors[PROCESS_MEGABYTES] : CRT_colors[PROCESS];
   int baseColor = CRT_colors[PROCESS];

   unsigned long long totalSeconds = totalHundredths / 100;
   unsigned long long totalMinutes = totalSeconds / 60;
   unsigned long long totalHours = totalMinutes / 60;
   unsigned int seconds = totalSeconds % 60;
   unsigned int minutes = totalMinutes % 60;

   if (totalMinutes < 60) {
      unsigned int hundredths = totalHundredths % 100;
      len = xSnprintf(buffer, sizeof(buffer), "%2u:%02u.%02u ", (unsigned int)totalMinutes, seconds, hundredths);
      RichString_appendnAscii(str, baseColor, buffer, len);
      return;
   }
   if (totalHours < 24) {
      len = xSnprintf(buffer, sizeof(buffer), "%2uh", (unsigned int)totalHours);
      RichString_appendnAscii(str, hourColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%02u:%02u ", minutes, seconds);
      RichString_appendnAscii(str, baseColor, buffer, len);
      return;
   }

   unsigned long long totalDays = totalHours / 24;
   unsigned int hours = totalHours % 24;
   if (totalDays < 10) {
      len = xSnprintf(buffer, sizeof(buffer), "%1ud", (unsigned int)totalDays);
      RichString_appendnAscii(str, dayColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%02uh", hours);
      RichString_appendnAscii(str, hourColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%02um ", minutes);
      RichString_appendnAscii(str, baseColor, buffer, len);
      return;
   }
   if (totalDays < /* Ignore leap years */365) {
      len = xSnprintf(buffer, sizeof(buffer), "%4ud", (unsigned int)totalDays);
      RichString_appendnAscii(str, dayColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%02uh ", hours);
      RichString_appendnAscii(str, hourColor, buffer, len);
      return;
   }

   unsigned long long years = totalDays / 365;
   unsigned int days = totalDays % 365;
   if (years < 1000) {
      len = xSnprintf(buffer, sizeof(buffer), "%3uy", (unsigned int)years);
      RichString_appendnAscii(str, yearColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%03ud ", days);
      RichString_appendnAscii(str, dayColor, buffer, len);
   } else if (years < 10000000) {
      len = xSnprintf(buffer, sizeof(buffer), "%7luy ", (unsigned long)years);
      RichString_appendnAscii(str, yearColor, buffer, len);
   } else {
      RichString_appendAscii(str, yearColor, "eternity ");
   }
}

void Row_printRate(RichString* str, double rate, bool coloring) {
   char buffer[16];

   int largeNumberColor = CRT_colors[LARGE_NUMBER];
   int megabytesColor = CRT_colors[PROCESS_MEGABYTES];
   int shadowColor = CRT_colors[PROCESS_SHADOW];
   int baseColor = CRT_colors[PROCESS];

   if (!coloring) {
      largeNumberColor = CRT_colors[PROCESS];
      megabytesColor = CRT_colors[PROCESS];
   }

   if (!isNonnegative(rate)) {
      RichString_appendAscii(str, shadowColor, "        N/A ");
   } else if (rate < 0.005) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f B/s ", rate);
      RichString_appendnAscii(str, shadowColor, buffer, len);
   } else if (rate < ONE_K) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f B/s ", rate);
      RichString_appendnAscii(str, baseColor, buffer, len);
   } else if (rate < ONE_M) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f K/s ", rate / ONE_K);
      RichString_appendnAscii(str, baseColor, buffer, len);
   } else if (rate < ONE_G) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f M/s ", rate / ONE_M);
      RichString_appendnAscii(str, megabytesColor, buffer, len);
   } else if (rate < ONE_T) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f G/s ", rate / ONE_G);
      RichString_appendnAscii(str, largeNumberColor, buffer, len);
   } else if (rate < ONE_P) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f T/s ", rate / ONE_T);
      RichString_appendnAscii(str, largeNumberColor, buffer, len);
   } else {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f P/s ", rate / ONE_P);
      RichString_appendnAscii(str, largeNumberColor, buffer, len);
   }
}

void Row_printLeftAlignedField(RichString* str, int attr, const char* content, unsigned int width) {
   int columns = width;
   RichString_appendnWideColumns(str, attr, content, strlen(content), &columns);
   RichString_appendChr(str, attr, ' ', width + 1 - columns);
}

int Row_printPercentage(float val, char* buffer, size_t n, uint8_t width, int* attr) {
   if (isNonnegative(val)) {
      if (val < 0.05F)
         *attr = CRT_colors[PROCESS_SHADOW];
      else if (val >= 99.9F)
         *attr = CRT_colors[PROCESS_MEGABYTES];

      int precision = 1;

      // Display "val" as "100" for columns like "MEM%".
      if (width == 4 && val > 99.9F) {
         precision = 0;
         val = 100.0F;
      }

      return xSnprintf(buffer, n, "%*.*f ", width, precision, val);
   }

   *attr = CRT_colors[PROCESS_SHADOW];
   return xSnprintf(buffer, n, "%*.*s ", width, width, "N/A");
}

void Row_toggleTag(Row* this) {
   this->tag = !this->tag;
}

int Row_compare(const void* v1, const void* v2) {
   const Row* r1 = (const Row*)v1;
   const Row* r2 = (const Row*)v2;

   return SPACESHIP_NUMBER(r1->id, r2->id);
}

int Row_compareByParent_Base(const void* v1, const void* v2) {
   const Row* r1 = (const Row*)v1;
   const Row* r2 = (const Row*)v2;

   int result = SPACESHIP_NUMBER(
      r1->isRoot ? 0 : Row_getGroupOrParent(r1),
      r2->isRoot ? 0 : Row_getGroupOrParent(r2)
   );

   if (result != 0)
      return result;

   return Row_compare(v1, v2);
}

const RowClass Row_class = {
   .super = {
      .extends = Class(Object),
      .compare = Row_compare
   },
};
