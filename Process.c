/*
htop - Process.c
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Process.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>

#include "CRT.h"
#include "Macros.h"
#include "Platform.h"
#include "ProcessList.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"

#if defined(MAJOR_IN_MKDEV)
#include <sys/mkdev.h>
#endif


/* Used to identify kernel threads in Comm and Exe columns */
static const char *const kthreadID = "KTHREAD";

static uid_t Process_getuid = (uid_t)-1;

int Process_pidDigits = 7;

void Process_setupColumnWidths() {
   int maxPid = Platform_getMaxPid();
   if (maxPid == -1)
      return;

   Process_pidDigits = ceil(log10(maxPid));
   assert(Process_pidDigits <= PROCESS_MAX_PID_DIGITS);
}

void Process_printBytes(RichString* str, unsigned long long number, bool coloring) {
   char buffer[16];
   int len;

   int largeNumberColor = coloring ? CRT_colors[LARGE_NUMBER] : CRT_colors[PROCESS];
   int processMegabytesColor = coloring ? CRT_colors[PROCESS_MEGABYTES] : CRT_colors[PROCESS];
   int processGigabytesColor = coloring ? CRT_colors[PROCESS_GIGABYTES] : CRT_colors[PROCESS];
   int shadowColor = coloring ? CRT_colors[PROCESS_SHADOW] : CRT_colors[PROCESS];
   int processColor = CRT_colors[PROCESS];

   if (number == ULLONG_MAX) {
      //Invalid number
      RichString_appendAscii(str, shadowColor, "  N/A ");
      return;
   }

   number /= ONE_K;

   if (number < 1000) {
      //Plain number, no markings
      len = xSnprintf(buffer, sizeof(buffer), "%5llu ", number);
      RichString_appendnAscii(str, processColor, buffer, len);
   } else if (number < 100000) {
      //2 digit MB, 3 digit KB
      len = xSnprintf(buffer, sizeof(buffer), "%2llu", number/1000);
      RichString_appendnAscii(str, processMegabytesColor, buffer, len);
      number %= 1000;
      len = xSnprintf(buffer, sizeof(buffer), "%03llu ", number);
      RichString_appendnAscii(str, processColor, buffer, len);
   } else if (number < 1000 * ONE_K) {
      //3 digit MB
      number /= ONE_K;
      len = xSnprintf(buffer, sizeof(buffer), "%4lluM ", number);
      RichString_appendnAscii(str, processMegabytesColor, buffer, len);
   } else if (number < 10000 * ONE_K) {
      //1 digit GB, 3 digit MB
      number /= ONE_K;
      len = xSnprintf(buffer, sizeof(buffer), "%1llu", number/1000);
      RichString_appendnAscii(str, processGigabytesColor, buffer, len);
      number %= 1000;
      len = xSnprintf(buffer, sizeof(buffer), "%03lluM ", number);
      RichString_appendnAscii(str, processMegabytesColor, buffer, len);
   } else if (number < 100000 * ONE_K) {
      //2 digit GB, 1 digit MB
      number /= 100 * ONE_K;
      len = xSnprintf(buffer, sizeof(buffer), "%2llu", number/10);
      RichString_appendnAscii(str, processGigabytesColor, buffer, len);
      number %= 10;
      len = xSnprintf(buffer, sizeof(buffer), ".%1llu", number);
      RichString_appendnAscii(str, processMegabytesColor, buffer, len);
      RichString_appendAscii(str, processGigabytesColor, "G ");
   } else if (number < 1000 * ONE_M) {
      //3 digit GB
      number /= ONE_M;
      len = xSnprintf(buffer, sizeof(buffer), "%4lluG ", number);
      RichString_appendnAscii(str, processGigabytesColor, buffer, len);
   } else if (number < 10000ULL * ONE_M) {
      //1 digit TB, 3 digit GB
      number /= ONE_M;
      len = xSnprintf(buffer, sizeof(buffer), "%1llu", number/1000);
      RichString_appendnAscii(str, largeNumberColor, buffer, len);
      number %= 1000;
      len = xSnprintf(buffer, sizeof(buffer), "%03lluG ", number);
      RichString_appendnAscii(str, processGigabytesColor, buffer, len);
   } else if (number < 100000 * ONE_M) {
      //2 digit TB, 1 digit GB
      number /= 100 * ONE_M;
      len = xSnprintf(buffer, sizeof(buffer), "%2llu", number/10);
      RichString_appendnAscii(str, largeNumberColor, buffer, len);
      number %= 10;
      len = xSnprintf(buffer, sizeof(buffer), ".%1llu", number);
      RichString_appendnAscii(str, processGigabytesColor, buffer, len);
      RichString_appendAscii(str, largeNumberColor, "T ");
   } else if (number < 10000ULL * ONE_G) {
      //3 digit TB or 1 digit PB, 3 digit TB
      number /= ONE_G;
      len = xSnprintf(buffer, sizeof(buffer), "%4lluT ", number);
      RichString_appendnAscii(str, largeNumberColor, buffer, len);
   } else {
      //2 digit PB and above
      len = xSnprintf(buffer, sizeof(buffer), "%4.1lfP ", (double)number/ONE_T);
      RichString_appendnAscii(str, largeNumberColor, buffer, len);
   }
}

void Process_printKBytes(RichString* str, unsigned long long number, bool coloring) {
   if (number == ULLONG_MAX)
      Process_printBytes(str, ULLONG_MAX, coloring);
   else
      Process_printBytes(str, number * ONE_K, coloring);
}

void Process_printCount(RichString* str, unsigned long long number, bool coloring) {
   char buffer[13];

   int largeNumberColor = coloring ? CRT_colors[LARGE_NUMBER] : CRT_colors[PROCESS];
   int processMegabytesColor = coloring ? CRT_colors[PROCESS_MEGABYTES] : CRT_colors[PROCESS];
   int processColor = CRT_colors[PROCESS];
   int processShadowColor = coloring ? CRT_colors[PROCESS_SHADOW] : CRT_colors[PROCESS];

   if (number == ULLONG_MAX) {
      RichString_appendAscii(str, CRT_colors[PROCESS_SHADOW], "        N/A ");
   } else if (number >= 100000LL * ONE_DECIMAL_T) {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number / ONE_DECIMAL_G);
      RichString_appendnAscii(str, largeNumberColor, buffer, 12);
   } else if (number >= 100LL * ONE_DECIMAL_T) {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number / ONE_DECIMAL_M);
      RichString_appendnAscii(str, largeNumberColor, buffer, 8);
      RichString_appendnAscii(str, processMegabytesColor, buffer+8, 4);
   } else if (number >= 10LL * ONE_DECIMAL_G) {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number / ONE_DECIMAL_K);
      RichString_appendnAscii(str, largeNumberColor, buffer, 5);
      RichString_appendnAscii(str, processMegabytesColor, buffer+5, 3);
      RichString_appendnAscii(str, processColor, buffer+8, 4);
   } else {
      xSnprintf(buffer, sizeof(buffer), "%11llu ", number);
      RichString_appendnAscii(str, largeNumberColor, buffer, 2);
      RichString_appendnAscii(str, processMegabytesColor, buffer+2, 3);
      RichString_appendnAscii(str, processColor, buffer+5, 3);
      RichString_appendnAscii(str, processShadowColor, buffer+8, 4);
   }
}

void Process_printTime(RichString* str, unsigned long long totalHundredths, bool coloring) {
   char buffer[10];
   int len;

   unsigned long long totalSeconds = totalHundredths / 100;
   unsigned long long hours = totalSeconds / 3600;
   unsigned long long days = totalSeconds / 86400;
   int minutes = (totalSeconds / 60) % 60;
   int seconds = totalSeconds % 60;
   int hundredths = totalHundredths - (totalSeconds * 100);

   int yearColor = coloring ? CRT_colors[LARGE_NUMBER]      : CRT_colors[PROCESS];
   int dayColor  = coloring ? CRT_colors[PROCESS_GIGABYTES] : CRT_colors[PROCESS];
   int hourColor = coloring ? CRT_colors[PROCESS_MEGABYTES] : CRT_colors[PROCESS];
   int defColor  = CRT_colors[PROCESS];

   if (days >= /* Ignore leapyears */365) {
      int years = days / 365;
      int daysLeft = days - 365 * years;

      if (daysLeft >= 100) {
         len = xSnprintf(buffer, sizeof(buffer), "%3dy", years);
         RichString_appendnAscii(str, yearColor, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), "%3dd ", daysLeft);
         RichString_appendnAscii(str, dayColor, buffer, len);
      } else if (daysLeft >= 10) {
         len = xSnprintf(buffer, sizeof(buffer), "%4dy", years);
         RichString_appendnAscii(str, yearColor, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), "%2dd ", daysLeft);
         RichString_appendnAscii(str, dayColor, buffer, len);
      } else {
         len = xSnprintf(buffer, sizeof(buffer), "%5dy", years);
         RichString_appendnAscii(str, yearColor, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), "%1dd ", daysLeft);
         RichString_appendnAscii(str, dayColor, buffer, len);
      }
   } else if (days >= 100) {
      int hoursLeft = hours - days * 24;

      if (hoursLeft >= 10) {
         len = xSnprintf(buffer, sizeof(buffer), "%4llud", days);
         RichString_appendnAscii(str, dayColor, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), "%2dh ", hoursLeft);
         RichString_appendnAscii(str, hourColor, buffer, len);
      } else {
         len = xSnprintf(buffer, sizeof(buffer), "%5llud", days);
         RichString_appendnAscii(str, dayColor, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), "%1dh ", hoursLeft);
         RichString_appendnAscii(str, hourColor, buffer, len);
      }
   } else if (hours >= 100) {
      int minutesLeft = totalSeconds / 60 - hours * 60;

      if (minutesLeft >= 10) {
         len = xSnprintf(buffer, sizeof(buffer), "%4lluh", hours);
         RichString_appendnAscii(str, hourColor, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), "%2dm ", minutesLeft);
         RichString_appendnAscii(str, defColor, buffer, len);
      } else {
         len = xSnprintf(buffer, sizeof(buffer), "%5lluh", hours);
         RichString_appendnAscii(str, hourColor, buffer, len);
         len = xSnprintf(buffer, sizeof(buffer), "%1dm ", minutesLeft);
         RichString_appendnAscii(str, defColor, buffer, len);
      }
   } else if (hours > 0) {
      len = xSnprintf(buffer, sizeof(buffer), "%2lluh", hours);
      RichString_appendnAscii(str, hourColor, buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%02d:%02d ", minutes, seconds);
      RichString_appendnAscii(str, defColor, buffer, len);
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%2d:%02d.%02d ", minutes, seconds, hundredths);
      RichString_appendnAscii(str, defColor, buffer, len);
   }
}

void Process_fillStarttimeBuffer(Process* this) {
   struct tm date;
   (void) localtime_r(&this->starttime_ctime, &date);
   strftime(this->starttime_show, sizeof(this->starttime_show) - 1, (this->starttime_ctime > (time(NULL) - 86400)) ? "%R " : "%b%d ", &date);
}

/*
 * TASK_COMM_LEN is defined to be 16 for /proc/[pid]/comm in man proc(5), but it is
 * not available in an userspace header - so define it.
 *
 * Note: This is taken from LINUX headers, but implicitly taken for other platforms
 * for sake of brevity.
 *
 * Note: when colorizing a basename with the comm prefix, the entire basename
 * (not just the comm prefix) is colorized for better readability, and it is
 * implicit that only upto (TASK_COMM_LEN - 1) could be comm.
 */
#define TASK_COMM_LEN 16

static bool findCommInCmdline(const char *comm, const char *cmdline, int cmdlineBasenameStart, int *pCommStart, int *pCommEnd) {
   /* Try to find procComm in tokenized cmdline - this might in rare cases
    * mis-identify a string or fail, if comm or cmdline had been unsuitably
    * modified by the process */
   const char *tokenBase;
   size_t tokenLen;
   const size_t commLen = strlen(comm);

   if (cmdlineBasenameStart < 0)
      return false;

   for (const char *token = cmdline + cmdlineBasenameStart; *token;) {
      for (tokenBase = token; *token && *token != '\n'; ++token) {
         if (*token == '/') {
            tokenBase = token + 1;
         }
      }
      tokenLen = token - tokenBase;

      if ((tokenLen == commLen || (tokenLen > commLen && commLen == (TASK_COMM_LEN - 1))) &&
          strncmp(tokenBase, comm, commLen) == 0) {
         *pCommStart = tokenBase - cmdline;
         *pCommEnd = token - cmdline;
         return true;
      }

      if (*token) {
         do {
            ++token;
         } while (*token && '\n' == *token);
      }
   }
   return false;
}

static int matchCmdlinePrefixWithExeSuffix(const char *cmdline, int cmdlineBaseOffset, const char *exe, int exeBaseOffset, int exeBaseLen) {
   int matchLen; /* matching length to be returned */
   char delim;   /* delimiter following basename */

   /* cmdline prefix is an absolute path: it must match whole exe. */
   if (cmdline[0] == '/') {
      matchLen = exeBaseLen + exeBaseOffset;
      if (strncmp(cmdline, exe, matchLen) == 0) {
         delim = cmdline[matchLen];
         if (delim == 0 || delim == '\n' || delim == ' ') {
            return matchLen;
         }
      }
      return 0;
   }

   /* cmdline prefix is a relative path: We need to first match the basename at
    * cmdlineBaseOffset and then reverse match the cmdline prefix with the exe
    * suffix. But there is a catch: Some processes modify their cmdline in ways
    * that make htop's identification of the basename in cmdline unreliable.
    * For e.g. /usr/libexec/gdm-session-worker modifies its cmdline to
    * "gdm-session-worker [pam/gdm-autologin]" and htop ends up with
    * proccmdlineBasenameEnd at "gdm-autologin]". This issue could arise with
    * chrome as well as it stores in cmdline its concatenated argument vector,
    * without NUL delimiter between the arguments (which may contain a '/')
    *
    * So if needed, we adjust cmdlineBaseOffset to the previous (if any)
    * component of the cmdline relative path, and retry the procedure. */
   bool delimFound; /* if valid basename delimiter found */
   do {
      /* match basename */
      matchLen = exeBaseLen + cmdlineBaseOffset;
      if (cmdlineBaseOffset < exeBaseOffset &&
          strncmp(cmdline + cmdlineBaseOffset, exe + exeBaseOffset, exeBaseLen) == 0) {
         delim = cmdline[matchLen];
         if (delim == 0 || delim == '\n' || delim == ' ') {
            int i, j;
            /* reverse match the cmdline prefix and exe suffix */
            for (i = cmdlineBaseOffset - 1, j = exeBaseOffset - 1;
                 i >= 0 && j >= 0 && cmdline[i] == exe[j]; --i, --j)
               ;

            /* full match, with exe suffix being a valid relative path */
            if (i < 0 && j >= 0 && exe[j] == '/')
               return matchLen;
         }
      }

      /* Try to find the previous potential cmdlineBaseOffset - it would be
       * preceded by '/' or nothing, and delimited by ' ' or '\n' */
      for (delimFound = false, cmdlineBaseOffset -= 2; cmdlineBaseOffset > 0; --cmdlineBaseOffset) {
         if (delimFound) {
            if (cmdline[cmdlineBaseOffset - 1] == '/') {
               break;
            }
         } else if (cmdline[cmdlineBaseOffset] == ' ' || cmdline[cmdlineBaseOffset] == '\n') {
            delimFound = true;
         }
      }
   } while (delimFound);

   return 0;
}

/* stpcpy, but also converts newlines to spaces */
static inline char *stpcpyWithNewlineConversion(char *dstStr, const char *srcStr) {
   for (; *srcStr; ++srcStr) {
      *dstStr++ = (*srcStr == '\n') ? ' ' : *srcStr;
   }
   *dstStr = 0;
   return dstStr;
}

/*
 * This function makes the merged Command string. It also stores the offsets of the
 * basename, comm w.r.t the merged Command string - these offsets will be used by
 * Process_writeCommand() for coloring. The merged Command string is also
 * returned by Process_getCommandStr() for searching, sorting and filtering.
 */
void Process_makeCommandStr(Process *this) {
   ProcessMergedCommand *mc = &this->mergedCommand;
   const Settings *settings = this->settings;

   bool showMergedCommand = settings->showMergedCommand;
   bool showProgramPath = settings->showProgramPath;
   bool searchCommInCmdline = settings->findCommInCmdline;
   bool stripExeFromCmdline = settings->stripExeFromCmdline;

   /* Nothing to do to (Re)Generate the Command string, if the process is:
    * - a kernel thread, or
    * - a zombie from before being under htop's watch, or
    * - a user thread and showThreadNames is not set */
   if (Process_isKernelThread(this))
      return;
   if (this->state == 'Z' && !this->mergedCommand.str)
      return;
   if (Process_isUserlandThread(this) && settings->showThreadNames)
      return;

   /* this->mergedCommand.str needs updating only if its state or contents changed.
    * Its content is based on the fields cmdline, comm, and exe. */
   if (
       mc->prevMergeSet == showMergedCommand &&
       mc->prevPathSet == showProgramPath &&
       mc->prevCommSet == searchCommInCmdline &&
       mc->prevCmdlineSet == stripExeFromCmdline &&
       !mc->cmdlineChanged &&
       !mc->commChanged &&
       !mc->exeChanged
   ) {
      return;
   }

   /* The field separtor "│" has been chosen such that it will not match any
    * valid string used for searching or filtering */
   const char *SEPARATOR = CRT_treeStr[TREE_STR_VERT];
   const int SEPARATOR_LEN = strlen(SEPARATOR);

   /* Check for any changed fields since we last built this string */
   if (mc->cmdlineChanged || mc->commChanged || mc->exeChanged) {
      free(mc->str);
      /* Accommodate the column text, two field separators and terminating NUL */
      size_t maxLen = 2 * SEPARATOR_LEN + 1;
      maxLen += this->cmdline ? strlen(this->cmdline) : strlen("(zombie)");
      maxLen += this->procComm ? strlen(this->procComm) : 0;
      maxLen += this->procExe ? strlen(this->procExe) : 0;

      mc->str = xCalloc(1, maxLen);
   }

   /* Preserve the settings used in this run */
   mc->prevMergeSet = showMergedCommand;
   mc->prevPathSet = showProgramPath;
   mc->prevCommSet = searchCommInCmdline;
   mc->prevCmdlineSet = stripExeFromCmdline;

   /* Mark everything as unchanged */
   mc->cmdlineChanged = false;
   mc->commChanged = false;
   mc->exeChanged = false;

   /* Reset all locations that need extra handling when actually displaying */
   mc->highlightCount = 0;
   memset(mc->highlights, 0, sizeof(mc->highlights));

   size_t mbMismatch = 0;
   #define WRITE_HIGHLIGHT(_offset, _length, _attr, _flags)                                   \
      do {                                                                                    \
         /* Check if we still have capacity */                                                \
         assert(mc->highlightCount < ARRAYSIZE(mc->highlights));                              \
         if (mc->highlightCount >= ARRAYSIZE(mc->highlights))                                 \
            continue;                                                                         \
                                                                                              \
         mc->highlights[mc->highlightCount].offset = str - strStart + (_offset) - mbMismatch; \
         mc->highlights[mc->highlightCount].length = _length;                                 \
         mc->highlights[mc->highlightCount].attr = _attr;                                     \
         mc->highlights[mc->highlightCount].flags = _flags;                                   \
         mc->highlightCount++;                                                                \
      } while (0)

   #define WRITE_SEPARATOR                                                                    \
      do {                                                                                    \
         WRITE_HIGHLIGHT(0, 1, CRT_colors[FAILED_READ], CMDLINE_HIGHLIGHT_FLAG_SEPARATOR);    \
         mbMismatch += SEPARATOR_LEN - 1;                                                     \
         str = stpcpy(str, SEPARATOR);                                                        \
      } while (0)

   const int baseAttr = Process_isThread(this) ? CRT_colors[PROCESS_THREAD_BASENAME] : CRT_colors[PROCESS_BASENAME];
   const int commAttr = Process_isThread(this) ? CRT_colors[PROCESS_THREAD_COMM] : CRT_colors[PROCESS_COMM];
   const int delExeAttr = CRT_colors[FAILED_READ];
   const int delLibAttr = CRT_colors[PROCESS_TAG];

   /* Establish some shortcuts to data we need */
   const char *cmdline = this->cmdline;
   const char *procComm = this->procComm;
   const char *procExe = this->procExe;

   char *strStart = mc->str;
   char *str = strStart;

   int cmdlineBasenameStart = this->cmdlineBasenameStart;
   int cmdlineBasenameEnd = this->cmdlineBasenameEnd;

   if (!cmdline) {
      cmdlineBasenameStart = 0;
      cmdlineBasenameEnd = 0;
      cmdline = "(zombie)";
   }

   assert(cmdlineBasenameStart >= 0);
   assert(cmdlineBasenameStart <= (int)strlen(cmdline));

   if (!showMergedCommand || !procExe || !procComm) { /* fall back to cmdline */
      if (showMergedCommand && !procExe && procComm && strlen(procComm)) { /* Prefix column with comm */
         if (strncmp(cmdline + cmdlineBasenameStart, procComm, MINIMUM(TASK_COMM_LEN - 1, strlen(procComm))) != 0) {
            WRITE_HIGHLIGHT(0, strlen(procComm), commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);
            str = stpcpy(str, procComm);

            WRITE_SEPARATOR;
         }
      }

      if (cmdlineBasenameEnd > cmdlineBasenameStart)
         WRITE_HIGHLIGHT(showProgramPath ? cmdlineBasenameStart : 0, cmdlineBasenameEnd - cmdlineBasenameStart, baseAttr, CMDLINE_HIGHLIGHT_FLAG_BASENAME);
      (void)stpcpyWithNewlineConversion(str, cmdline + (showProgramPath ? 0 : cmdlineBasenameStart));

      return;
   }

   int exeLen = strlen(this->procExe);
   int exeBasenameOffset = this->procExeBasenameOffset;
   int exeBasenameLen = exeLen - exeBasenameOffset;

   assert(exeBasenameOffset >= 0);
   assert(exeBasenameOffset <= (int)strlen(procExe));

   bool haveCommInExe = false;
   if (procExe && procComm) {
      haveCommInExe = strncmp(procExe + exeBasenameOffset, procComm, TASK_COMM_LEN - 1) == 0;
   }

   /* Start with copying exe */
   if (showProgramPath) {
      if (haveCommInExe)
         WRITE_HIGHLIGHT(exeBasenameOffset, exeBasenameLen, commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);
      WRITE_HIGHLIGHT(exeBasenameOffset, exeBasenameLen, baseAttr, CMDLINE_HIGHLIGHT_FLAG_BASENAME);
      if (this->procExeDeleted)
         WRITE_HIGHLIGHT(exeBasenameOffset, exeBasenameLen, delExeAttr, CMDLINE_HIGHLIGHT_FLAG_DELETED);
      else if (this->usesDeletedLib)
         WRITE_HIGHLIGHT(exeBasenameOffset, exeBasenameLen, delLibAttr, CMDLINE_HIGHLIGHT_FLAG_DELETED);
      str = stpcpy(str, procExe);
   } else {
      if (haveCommInExe)
         WRITE_HIGHLIGHT(0, exeBasenameLen, commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);
      WRITE_HIGHLIGHT(0, exeBasenameLen, baseAttr, CMDLINE_HIGHLIGHT_FLAG_BASENAME);
      if (this->procExeDeleted)
         WRITE_HIGHLIGHT(0, exeBasenameLen, delExeAttr, CMDLINE_HIGHLIGHT_FLAG_DELETED);
      else if (this->usesDeletedLib)
         WRITE_HIGHLIGHT(0, exeBasenameLen, delLibAttr, CMDLINE_HIGHLIGHT_FLAG_DELETED);
      str = stpcpy(str, procExe + exeBasenameOffset);
   }

   bool haveCommInCmdline = false;
   int commStart = 0;
   int commEnd = 0;

   /* Try to match procComm with procExe's basename: This is reliable (predictable) */
   if (searchCommInCmdline) {
      /* commStart/commEnd will be adjusted later along with cmdline */
      haveCommInCmdline = findCommInCmdline(procComm, cmdline, cmdlineBasenameStart, &commStart, &commEnd);
   }

   int matchLen = matchCmdlinePrefixWithExeSuffix(cmdline, cmdlineBasenameStart, procExe, exeBasenameOffset, exeBasenameLen);

   bool haveCommField = false;

   if (!haveCommInExe && !haveCommInCmdline && procComm) {
      WRITE_SEPARATOR;
      WRITE_HIGHLIGHT(0, strlen(procComm), commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);
      str = stpcpy(str, procComm);
      haveCommField = true;
   }

   if (matchLen) {
      /* strip the matched exe prefix */
      cmdline += matchLen;

      commStart -= matchLen;
      commEnd -= matchLen;
   }

   if (!matchLen || (haveCommField && *cmdline)) {
      /* cmdline will be a separate field */
      WRITE_SEPARATOR;
   }

   if (!haveCommInExe && haveCommInCmdline && !haveCommField)
      WRITE_HIGHLIGHT(commStart, commEnd - commStart, commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);

   /* Display cmdline if it hasn't been consumed by procExe */
   if (*cmdline)
      (void)stpcpyWithNewlineConversion(str, cmdline);

   #undef WRITE_SEPARATOR
   #undef WRITE_HIGHLIGHT
}

void Process_writeCommand(const Process* this, int attr, int baseAttr, RichString* str) {
   (void)baseAttr;

   const ProcessMergedCommand *mc = &this->mergedCommand;

   int strStart = RichString_size(str);

   const bool highlightBaseName = this->settings->highlightBaseName;
   const bool highlightSeparator = true;
   const bool highlightDeleted = this->settings->highlightDeletedExe;

   if (!this->mergedCommand.str) {
      int len = 0;
      const char* cmdline = this->cmdline;

      if (highlightBaseName || !this->settings->showProgramPath) {
         int basename = 0;
         for (int i = 0; i < this->cmdlineBasenameEnd; i++) {
            if (cmdline[i] == '/') {
               basename = i + 1;
            } else if (cmdline[i] == ':') {
               len = i + 1;
               break;
            }
         }
         if (len == 0) {
            if (this->settings->showProgramPath) {
               strStart += basename;
            } else {
               cmdline += basename;
            }
            len = this->cmdlineBasenameEnd - basename;
         }
      }

      RichString_appendWide(str, attr, cmdline);

      if (this->settings->highlightBaseName) {
         RichString_setAttrn(str, baseAttr, strStart, len);
      }

      return;
   }

   RichString_appendWide(str, attr, this->mergedCommand.str);

   for (size_t i = 0, hlCount = CLAMP(mc->highlightCount, 0, ARRAYSIZE(mc->highlights)); i < hlCount; i++) {
      const ProcessCmdlineHighlight *hl = &mc->highlights[i];

      if (!hl->length)
         continue;

      if (hl->flags & CMDLINE_HIGHLIGHT_FLAG_SEPARATOR)
         if (!highlightSeparator)
            continue;

      if (hl->flags & CMDLINE_HIGHLIGHT_FLAG_BASENAME)
         if (!highlightBaseName)
            continue;

      if (hl->flags & CMDLINE_HIGHLIGHT_FLAG_DELETED)
         if (!highlightDeleted)
            continue;

      RichString_setAttrn(str, hl->attr, strStart + hl->offset, hl->length);
   }
}

void Process_printRate(RichString* str, double rate, bool coloring) {
   char buffer[16];

   int largeNumberColor = CRT_colors[LARGE_NUMBER];
   int processMegabytesColor = CRT_colors[PROCESS_MEGABYTES];
   int processColor = CRT_colors[PROCESS];
   int shadowColor = CRT_colors[PROCESS_SHADOW];

   if (!coloring) {
      largeNumberColor = CRT_colors[PROCESS];
      processMegabytesColor = CRT_colors[PROCESS];
   }

   if (isnan(rate)) {
      RichString_appendAscii(str, shadowColor, "        N/A ");
   } else if (rate < 0.005) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f B/s ", rate);
      RichString_appendnAscii(str, shadowColor, buffer, len);
   } else if (rate < ONE_K) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f B/s ", rate);
      RichString_appendnAscii(str, processColor, buffer, len);
   } else if (rate < ONE_M) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f K/s ", rate / ONE_K);
      RichString_appendnAscii(str, processColor, buffer, len);
   } else if (rate < ONE_G) {
      int len = snprintf(buffer, sizeof(buffer), "%7.2f M/s ", rate / ONE_M);
      RichString_appendnAscii(str, processMegabytesColor, buffer, len);
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

void Process_printLeftAlignedField(RichString* str, int attr, const char* content, unsigned int width) {
   int columns = width;
   RichString_appendnWideColumns(str, attr, content, strlen(content), &columns);
   RichString_appendChr(str, attr, ' ', width + 1 - columns);
}

void Process_writeField(const Process* this, RichString* str, ProcessField field) {
   char buffer[256];
   size_t n = sizeof(buffer);
   int attr = CRT_colors[DEFAULT_COLOR];
   bool coloring = this->settings->highlightMegabytes;

   switch (field) {
   case COMM: {
      int baseattr = CRT_colors[PROCESS_BASENAME];
      if (this->settings->highlightThreads && Process_isThread(this)) {
         attr = CRT_colors[PROCESS_THREAD];
         baseattr = CRT_colors[PROCESS_THREAD_BASENAME];
      }
      if (!this->settings->treeView || this->indent == 0) {
         Process_writeCommand(this, attr, baseattr, str);
         return;
      }

      char* buf = buffer;
      int maxIndent = 0;
      bool lastItem = (this->indent < 0);
      int indent = (this->indent < 0 ? -this->indent : this->indent);

      for (int i = 0; i < 32; i++) {
         if (indent & (1U << i)) {
            maxIndent = i+1;
         }
      }

      for (int i = 0; i < maxIndent - 1; i++) {
         int written, ret;
         if (indent & (1 << i)) {
            ret = xSnprintf(buf, n, "%s  ", CRT_treeStr[TREE_STR_VERT]);
         } else {
            ret = xSnprintf(buf, n, "   ");
         }
         if (ret < 0 || (size_t)ret >= n) {
            written = n;
         } else {
            written = ret;
         }
         buf += written;
         n -= written;
      }

      const char* draw = CRT_treeStr[lastItem ? TREE_STR_BEND : TREE_STR_RTEE];
      xSnprintf(buf, n, "%s%s ", draw, this->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
      RichString_appendWide(str, CRT_colors[PROCESS_TREE], buffer);
      Process_writeCommand(this, attr, baseattr, str);
      return;
   }
   case PROC_COMM: {
      const char* procComm;
      if (this->procComm) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_COMM : PROCESS_COMM];
         procComm = this->procComm;
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         procComm = Process_isKernelThread(this) ? kthreadID : "N/A";
      }

      Process_printLeftAlignedField(str, attr, procComm, TASK_COMM_LEN - 1);
      return;
   }
   case PROC_EXE: {
      const char* procExe;
      if (this->procExe) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_BASENAME : PROCESS_BASENAME];
         if (this->settings->highlightDeletedExe) {
            if (this->procExeDeleted)
               attr = CRT_colors[FAILED_READ];
            else if (this->usesDeletedLib)
               attr = CRT_colors[PROCESS_TAG];
         }
         procExe = this->procExe + this->procExeBasenameOffset;
      } else {
         attr = CRT_colors[PROCESS_SHADOW];
         procExe = Process_isKernelThread(this) ? kthreadID : "N/A";
      }

      Process_printLeftAlignedField(str, attr, procExe, TASK_COMM_LEN - 1);
      return;
   }
   case CWD: {
      const char* cwd;
      if (!this->procCwd) {
         attr = CRT_colors[PROCESS_SHADOW];
         cwd = "N/A";
      } else if (String_startsWith(this->procCwd, "/proc/") && strstr(this->procCwd, " (deleted)") != NULL) {
         attr = CRT_colors[PROCESS_SHADOW];
         cwd = "main thread terminated";
      } else {
         cwd = this->procCwd;
      }
      Process_printLeftAlignedField(str, attr, cwd, 25);
      return;
   }
   case ELAPSED: Process_printTime(str, /* convert to hundreds of a second */ this->processList->realtimeMs / 10 - 100 * this->starttime_ctime, coloring); return;
   case MAJFLT: Process_printCount(str, this->majflt, coloring); return;
   case MINFLT: Process_printCount(str, this->minflt, coloring); return;
   case M_RESIDENT: Process_printKBytes(str, this->m_resident, coloring); return;
   case M_VIRT: Process_printKBytes(str, this->m_virt, coloring); return;
   case NICE:
      xSnprintf(buffer, n, "%3ld ", this->nice);
      attr = this->nice < 0 ? CRT_colors[PROCESS_HIGH_PRIORITY]
           : this->nice > 0 ? CRT_colors[PROCESS_LOW_PRIORITY]
           : CRT_colors[PROCESS_SHADOW];
      break;
   case NLWP:
      if (this->nlwp == 1)
         attr = CRT_colors[PROCESS_SHADOW];

      xSnprintf(buffer, n, "%4ld ", this->nlwp);
      break;
   case PERCENT_CPU:
   case PERCENT_NORM_CPU: {
      float cpuPercentage = this->percent_cpu;
      if (field == PERCENT_NORM_CPU) {
         cpuPercentage /= this->processList->cpuCount;
      }
      if (cpuPercentage > 999.9F) {
         xSnprintf(buffer, n, "%4u ", (unsigned int)cpuPercentage);
      } else if (cpuPercentage > 99.9F) {
         xSnprintf(buffer, n, "%3u. ", (unsigned int)cpuPercentage);
      } else {
         if (cpuPercentage < 0.05F)
            attr = CRT_colors[PROCESS_SHADOW];

         xSnprintf(buffer, n, "%4.1f ", cpuPercentage);
      }
      break;
   }
   case PERCENT_MEM:
      if (this->percent_mem > 99.9F) {
         xSnprintf(buffer, n, "100. ");
      } else {
         if (this->percent_mem < 0.05F)
            attr = CRT_colors[PROCESS_SHADOW];

         xSnprintf(buffer, n, "%4.1f ", this->percent_mem);
      }
      break;
   case PGRP: xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->pgrp); break;
   case PID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->pid); break;
   case PPID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->ppid); break;
   case PRIORITY:
      if (this->priority <= -100)
         xSnprintf(buffer, n, " RT ");
      else
         xSnprintf(buffer, n, "%3ld ", this->priority);
      break;
   case PROCESSOR: xSnprintf(buffer, n, "%3d ", Settings_cpuId(this->settings, this->processor)); break;
   case SESSION: xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->session); break;
   case STARTTIME: xSnprintf(buffer, n, "%s", this->starttime_show); break;
   case STATE:
      xSnprintf(buffer, n, "%c ", this->state);
      switch (this->state) {
         case 'R':
            attr = CRT_colors[PROCESS_R_STATE];
            break;
         case 'D':
            attr = CRT_colors[PROCESS_D_STATE];
            break;
         case 'I':
         case 'S':
            attr = CRT_colors[PROCESS_SHADOW];
            break;
      }
      break;
   case ST_UID: xSnprintf(buffer, n, "%5d ", this->st_uid); break;
   case TIME: Process_printTime(str, this->time, coloring); return;
   case TGID:
      if (this->tgid == this->pid)
         attr = CRT_colors[PROCESS_SHADOW];

      xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->tgid);
      break;
   case TPGID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->tpgid); break;
   case TTY:
      if (!this->tty_name) {
         attr = CRT_colors[PROCESS_SHADOW];
         xSnprintf(buffer, n, "(no tty) ");
      } else {
         const char* name = String_startsWith(this->tty_name, "/dev/") ? (this->tty_name + strlen("/dev/")) : this->tty_name;
         xSnprintf(buffer, n, "%-8s ", name);
      }
      break;
   case USER:
      if (Process_getuid != this->st_uid)
         attr = CRT_colors[PROCESS_SHADOW];

      if (this->user) {
         Process_printLeftAlignedField(str, attr, this->user, 9);
         return;
      }

      xSnprintf(buffer, n, "%-9d ", this->st_uid);
      break;
   default:
      assert(0 && "Process_writeField: default key reached"); /* should never be reached */
      xSnprintf(buffer, n, "- ");
   }
   RichString_appendAscii(str, attr, buffer);
}

void Process_display(const Object* cast, RichString* out) {
   const Process* this = (const Process*) cast;
   const ProcessField* fields = this->settings->fields;
   for (int i = 0; fields[i]; i++)
      As_Process(this)->writeField(this, out, fields[i]);

   if (this->settings->shadowOtherUsers && this->st_uid != Process_getuid) {
      RichString_setAttr(out, CRT_colors[PROCESS_SHADOW]);
   }

   if (this->tag == true) {
      RichString_setAttr(out, CRT_colors[PROCESS_TAG]);
   }

   if (this->settings->highlightChanges) {
      if (Process_isTomb(this)) {
         out->highlightAttr = CRT_colors[PROCESS_TOMB];
      } else if (Process_isNew(this)) {
         out->highlightAttr = CRT_colors[PROCESS_NEW];
      }
   }

   assert(RichString_size(out) > 0);
}

void Process_done(Process* this) {
   assert (this != NULL);
   free(this->cmdline);
   free(this->procComm);
   free(this->procExe);
   free(this->procCwd);
   free(this->mergedCommand.str);
   free(this->tty_name);
}

/* This function returns the string displayed in Command column, so that sorting
 * happens on what is displayed - whether comm, full path, basename, etc.. So
 * this follows Process_writeField(COMM) and Process_writeCommand */
const char *Process_getCommandStr(const Process *this) {
   if ((Process_isUserlandThread(this) && this->settings->showThreadNames) || !this->mergedCommand.str) {
      return this->cmdline;
   }

   return this->mergedCommand.str;
}

const ProcessClass Process_class = {
   .super = {
      .extends = Class(Object),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = Process_writeField,
   .getCommandStr = Process_getCommandStr,
};

void Process_init(Process* this, const Settings* settings) {
   this->settings = settings;
   this->tag = false;
   this->showChildren = true;
   this->show = true;
   this->updated = false;
   this->cmdlineBasenameEnd = -1;
   this->st_uid = (uid_t)-1;

   if (Process_getuid == (uid_t)-1) {
      Process_getuid = getuid();
   }
}

void Process_toggleTag(Process* this) {
   this->tag = !this->tag;
}

bool Process_isNew(const Process* this) {
   assert(this->processList);
   if (this->processList->monotonicMs >= this->seenStampMs) {
      return this->processList->monotonicMs - this->seenStampMs <= 1000 * (uint64_t)this->processList->settings->highlightDelaySecs;
   }
   return false;
}

bool Process_isTomb(const Process* this) {
    return this->tombStampMs > 0;
}

bool Process_setPriority(Process* this, int priority) {
   if (Settings_isReadonly())
      return false;

   int old_prio = getpriority(PRIO_PROCESS, this->pid);
   int err = setpriority(PRIO_PROCESS, this->pid, priority);

   if (err == 0 && old_prio != getpriority(PRIO_PROCESS, this->pid)) {
      this->nice = priority;
   }
   return (err == 0);
}

bool Process_changePriorityBy(Process* this, Arg delta) {
   return Process_setPriority(this, this->nice + delta.i);
}

bool Process_sendSignal(Process* this, Arg sgn) {
   return kill(this->pid, sgn.i) == 0;
}

int Process_pidCompare(const void* v1, const void* v2) {
   const Process* p1 = (const Process*)v1;
   const Process* p2 = (const Process*)v2;

   return SPACESHIP_NUMBER(p1->pid, p2->pid);
}

int Process_compare(const void* v1, const void* v2) {
   const Process *p1 = (const Process*)v1;
   const Process *p2 = (const Process*)v2;

   const Settings *settings = p1->settings;

   ProcessField key = Settings_getActiveSortKey(settings);

   int result = Process_compareByKey(p1, p2, key);

   // Implement tie-breaker (needed to make tree mode more stable)
   if (!result)
      return SPACESHIP_NUMBER(p1->pid, p2->pid);

   return (Settings_getActiveDirection(settings) == 1) ? result : -result;
}

static uint8_t stateCompareValue(char state) {
   switch (state) {

   case 'S':
      return 10;

   case 'I':
      return 9;

   case 'X':
      return 8;

   case 'Z':
      return 7;

   case 't':
      return 6;

   case 'T':
      return 5;

   case 'L':
      return 4;

   case 'D':
      return 3;

   case 'R':
      return 2;

   case '?':
      return 1;

   default:
      return 0;
   }
}

int Process_compareByKey_Base(const Process* p1, const Process* p2, ProcessField key) {
   int r;

   switch (key) {
   case PERCENT_CPU:
   case PERCENT_NORM_CPU:
      return SPACESHIP_NUMBER(p1->percent_cpu, p2->percent_cpu);
   case PERCENT_MEM:
      return SPACESHIP_NUMBER(p1->m_resident, p2->m_resident);
   case COMM:
      return SPACESHIP_NULLSTR(Process_getCommand(p1), Process_getCommand(p2));
   case PROC_COMM: {
      const char *comm1 = p1->procComm ? p1->procComm : (Process_isKernelThread(p1) ? kthreadID : "");
      const char *comm2 = p2->procComm ? p2->procComm : (Process_isKernelThread(p2) ? kthreadID : "");
      return SPACESHIP_NULLSTR(comm1, comm2);
   }
   case PROC_EXE: {
      const char *exe1 = p1->procExe ? (p1->procExe + p1->procExeBasenameOffset) : (Process_isKernelThread(p1) ? kthreadID : "");
      const char *exe2 = p2->procExe ? (p2->procExe + p2->procExeBasenameOffset) : (Process_isKernelThread(p2) ? kthreadID : "");
      return SPACESHIP_NULLSTR(exe1, exe2);
   }
   case CWD:
      return SPACESHIP_NULLSTR(p1->procCwd, p2->procCwd);
   case ELAPSED:
      r = -SPACESHIP_NUMBER(p1->starttime_ctime, p2->starttime_ctime);
      return r != 0 ? r : SPACESHIP_NUMBER(p1->pid, p2->pid);
   case MAJFLT:
      return SPACESHIP_NUMBER(p1->majflt, p2->majflt);
   case MINFLT:
      return SPACESHIP_NUMBER(p1->minflt, p2->minflt);
   case M_RESIDENT:
      return SPACESHIP_NUMBER(p1->m_resident, p2->m_resident);
   case M_VIRT:
      return SPACESHIP_NUMBER(p1->m_virt, p2->m_virt);
   case NICE:
      return SPACESHIP_NUMBER(p1->nice, p2->nice);
   case NLWP:
      return SPACESHIP_NUMBER(p1->nlwp, p2->nlwp);
   case PGRP:
      return SPACESHIP_NUMBER(p1->pgrp, p2->pgrp);
   case PID:
      return SPACESHIP_NUMBER(p1->pid, p2->pid);
   case PPID:
      return SPACESHIP_NUMBER(p1->ppid, p2->ppid);
   case PRIORITY:
      return SPACESHIP_NUMBER(p1->priority, p2->priority);
   case PROCESSOR:
      return SPACESHIP_NUMBER(p1->processor, p2->processor);
   case SESSION:
      return SPACESHIP_NUMBER(p1->session, p2->session);
   case STARTTIME:
      r = SPACESHIP_NUMBER(p1->starttime_ctime, p2->starttime_ctime);
      return r != 0 ? r : SPACESHIP_NUMBER(p1->pid, p2->pid);
   case STATE:
      return SPACESHIP_NUMBER(stateCompareValue(p1->state), stateCompareValue(p2->state));
   case ST_UID:
      return SPACESHIP_NUMBER(p1->st_uid, p2->st_uid);
   case TIME:
      return SPACESHIP_NUMBER(p1->time, p2->time);
   case TGID:
      return SPACESHIP_NUMBER(p1->tgid, p2->tgid);
   case TPGID:
      return SPACESHIP_NUMBER(p1->tpgid, p2->tpgid);
   case TTY:
      /* Order no tty last */
      return SPACESHIP_DEFAULTSTR(p1->tty_name, p2->tty_name, "\x7F");
   case USER:
      return SPACESHIP_NULLSTR(p1->user, p2->user);
   default:
      assert(0 && "Process_compareByKey_Base: default key reached"); /* should never be reached */
      return SPACESHIP_NUMBER(p1->pid, p2->pid);
   }
}

void Process_updateComm(Process* this, const char* comm) {
   if (!this->procComm && !comm)
      return;

   if (this->procComm && comm && String_eq(this->procComm, comm))
      return;

   free(this->procComm);
   this->procComm = comm ? xStrdup(comm) : NULL;
   this->mergedCommand.commChanged = true;
}

static int skipPotentialPath(const char* cmdline, int end) {
   if (cmdline[0] != '/')
      return 0;

   int slash = 0;
   for (int i = 1; i < end; i++) {
      if (cmdline[i] == '/' && cmdline[i+1] != '\0') {
         slash = i + 1;
         continue;
      }

      if (cmdline[i] == ' ' && cmdline[i-1] != '\\')
         return slash;

      if (cmdline[i] == ':' && cmdline[i+1] == ' ')
         return slash;
   }

   return slash;
}

void Process_updateCmdline(Process* this, const char* cmdline, int basenameStart, int basenameEnd) {
   assert(basenameStart >= 0);
   assert((cmdline && basenameStart < (int)strlen(cmdline)) || (!cmdline && basenameStart == 0));
   assert((basenameEnd > basenameStart) || (basenameEnd == 0 && basenameStart == 0));
   assert((cmdline && basenameEnd <= (int)strlen(cmdline)) || (!cmdline && basenameEnd == 0));

   if (!this->cmdline && !cmdline)
      return;

   if (this->cmdline && cmdline && String_eq(this->cmdline, cmdline))
      return;

   free(this->cmdline);
   this->cmdline = cmdline ? xStrdup(cmdline) : NULL;
   this->cmdlineBasenameStart = (basenameStart || !cmdline) ? basenameStart : skipPotentialPath(cmdline, basenameEnd);
   this->cmdlineBasenameEnd = basenameEnd;
   this->mergedCommand.cmdlineChanged = true;
}

void Process_updateExe(Process* this, const char* exe) {
   if (!this->procExe && !exe)
      return;

   if (this->procExe && exe && String_eq(this->procExe, exe))
      return;

   free(this->procExe);
   if (exe) {
      this->procExe = xStrdup(exe);
      const char* lastSlash = strrchr(exe, '/');
      this->procExeBasenameOffset = (lastSlash && *(lastSlash + 1) != '\0' && lastSlash != exe) ? (lastSlash - exe + 1) : 0;
   } else {
      this->procExe = NULL;
      this->procExeBasenameOffset = 0;
   }
   this->mergedCommand.exeChanged = true;
}
