/*
htop - Process.c
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "Process.h"

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

#include "CRT.h"
#include "Hashtable.h"
#include "Machine.h"
#include "Macros.h"
#include "ProcessTable.h"
#include "DynamicColumn.h"
#include "RichString.h"
#include "Scheduling.h"
#include "Settings.h"
#include "Table.h"
#include "XUtils.h"

#if defined(MAJOR_IN_MKDEV)
#include <sys/mkdev.h>
#endif


/* Used to identify kernel threads in Comm and Exe columns */
static const char* const kthreadID = "KTHREAD";

void Process_fillStarttimeBuffer(Process* this) {
   struct tm date;
   time_t now = this->super.host->realtime.tv_sec;
   (void) localtime_r(&this->starttime_ctime, &date);

   strftime(this->starttime_show,
            sizeof(this->starttime_show) - 1,
            (this->starttime_ctime > now - 86400) ? "%R " : (this->starttime_ctime > now - 364 * 86400) ? "%b%d " : " %Y ",
            &date);
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
 * implicit that only up to (TASK_COMM_LEN - 1) could be comm.
 */
#define TASK_COMM_LEN 16

static bool findCommInCmdline(const char* comm, const char* cmdline, int cmdlineBasenameStart, int* pCommStart, int* pCommEnd) {
   /* Try to find procComm in tokenized cmdline - this might in rare cases
    * mis-identify a string or fail, if comm or cmdline had been unsuitably
    * modified by the process */
   const char* tokenBase;
   size_t tokenLen;
   const size_t commLen = strlen(comm);

   if (cmdlineBasenameStart < 0)
      return false;

   for (const char* token = cmdline + cmdlineBasenameStart; *token;) {
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

static int matchCmdlinePrefixWithExeSuffix(const char* cmdline, int cmdlineBaseOffset, const char* exe, int exeBaseOffset, int exeBaseLen) {
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
static inline char* stpcpyWithNewlineConversion(char* dstStr, const char* srcStr) {
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
 * returned by Process_getCommand() for searching, sorting and filtering.
 */
void Process_makeCommandStr(Process* this, const Settings* settings) {
   ProcessMergedCommand* mc = &this->mergedCommand;

   bool showMergedCommand = settings->showMergedCommand;
   bool showProgramPath = settings->showProgramPath;
   bool searchCommInCmdline = settings->findCommInCmdline;
   bool stripExeFromCmdline = settings->stripExeFromCmdline;
   bool showThreadNames = settings->showThreadNames;
   bool shadowDistPathPrefix = settings->shadowDistPathPrefix;

   uint64_t settingsStamp = settings->lastUpdate;

   /* Nothing to do to (Re)Generate the Command string, if the process is:
    * - a kernel thread, or
    * - a zombie from before being under htop's watch, or
    * - a user thread and showThreadNames is not set */
   if (Process_isKernelThread(this))
      return;
   if (this->state == ZOMBIE && !this->mergedCommand.str)
      return;

   /* this->mergedCommand.str needs updating only if its state or contents changed.
    * Its content is based on the fields cmdline, comm, and exe. */
   if (mc->lastUpdate >= settingsStamp)
      return;

   mc->lastUpdate = settingsStamp;

   /* The field separator "│" has been chosen such that it will not match any
    * valid string used for searching or filtering */
   const char* SEPARATOR = CRT_treeStr[TREE_STR_VERT];
   const int SEPARATOR_LEN = strlen(SEPARATOR);

   /* Accommodate the column text, two field separators and terminating NUL */
   size_t maxLen = 2 * SEPARATOR_LEN + 1;
   maxLen += this->cmdline ? strlen(this->cmdline) : strlen("(zombie)");
   maxLen += this->procComm ? strlen(this->procComm) : 0;
   maxLen += this->procExe ? strlen(this->procExe) : 0;

   free(mc->str);
   mc->str = xCalloc(1, maxLen);

   /* Reset all locations that need extra handling when actually displaying */
   mc->highlightCount = 0;
   memset(mc->highlights, 0, sizeof(mc->highlights));

   size_t mbMismatch = 0;
   #define WRITE_HIGHLIGHT(_offset, _length, _attr, _flags)                                   \
      do {                                                                                    \
         /* Check if we still have capacity */                                                \
         assert(mc->highlightCount < ARRAYSIZE(mc->highlights));                              \
         if (mc->highlightCount >= ARRAYSIZE(mc->highlights))                                 \
            break;                                                                            \
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

   #define CHECK_AND_MARK(str_, prefix_)                                                      \
      if (String_startsWith(str_, prefix_)) {                                                 \
         WRITE_HIGHLIGHT(0, strlen(prefix_), CRT_colors[PROCESS_SHADOW], CMDLINE_HIGHLIGHT_FLAG_PREFIXDIR); \
         break;                                                                               \
      } else (void)0

   #define CHECK_AND_MARK_DIST_PATH_PREFIXES(str_)                                            \
      do {                                                                                    \
         if ((str_)[0] != '/') {                                                              \
            break;                                                                            \
         }                                                                                    \
         switch ((str_)[1]) {                                                                 \
            case 'b':                                                                         \
               CHECK_AND_MARK(str_, "/bin/");                                                 \
               break;                                                                         \
            case 'l':                                                                         \
               CHECK_AND_MARK(str_, "/lib/");                                                 \
               CHECK_AND_MARK(str_, "/lib32/");                                               \
               CHECK_AND_MARK(str_, "/lib64/");                                               \
               CHECK_AND_MARK(str_, "/libx32/");                                              \
               break;                                                                         \
            case 's':                                                                         \
               CHECK_AND_MARK(str_, "/sbin/");                                                \
               break;                                                                         \
            case 'u':                                                                         \
               if (String_startsWith(str_, "/usr/")) {                                        \
                  switch ((str_)[5]) {                                                        \
                     case 'b':                                                                \
                        CHECK_AND_MARK(str_, "/usr/bin/");                                    \
                        break;                                                                \
                     case 'l':                                                                \
                        CHECK_AND_MARK(str_, "/usr/libexec/");                                \
                        CHECK_AND_MARK(str_, "/usr/lib/");                                    \
                        CHECK_AND_MARK(str_, "/usr/lib32/");                                  \
                        CHECK_AND_MARK(str_, "/usr/lib64/");                                  \
                        CHECK_AND_MARK(str_, "/usr/libx32/");                                 \
                                                                                              \
                        CHECK_AND_MARK(str_, "/usr/local/bin/");                              \
                        CHECK_AND_MARK(str_, "/usr/local/lib/");                              \
                        CHECK_AND_MARK(str_, "/usr/local/sbin/");                             \
                        break;                                                                \
                     case 's':                                                                \
                        CHECK_AND_MARK(str_, "/usr/sbin/");                                   \
                        break;                                                                \
                  }                                                                           \
               }                                                                              \
               break;                                                                         \
         }                                                                                    \
      } while (0)

   const int baseAttr = Process_isThread(this) ? CRT_colors[PROCESS_THREAD_BASENAME] : CRT_colors[PROCESS_BASENAME];
   const int commAttr = Process_isThread(this) ? CRT_colors[PROCESS_THREAD_COMM] : CRT_colors[PROCESS_COMM];
   const int delExeAttr = CRT_colors[FAILED_READ];
   const int delLibAttr = CRT_colors[PROCESS_TAG];

   /* Establish some shortcuts to data we need */
   const char* cmdline = this->cmdline;
   const char* procComm = this->procComm;
   const char* procExe = this->procExe;

   char* strStart = mc->str;
   char* str = strStart;

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
      if ((showMergedCommand || (Process_isUserlandThread(this) && showThreadNames)) && procComm && strlen(procComm)) { /* set column to or prefix it with comm */
         if (strncmp(cmdline + cmdlineBasenameStart, procComm, MINIMUM(TASK_COMM_LEN - 1, strlen(procComm))) != 0) {
            WRITE_HIGHLIGHT(0, strlen(procComm), commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);
            str = stpcpy(str, procComm);

            if (!showMergedCommand)
               return;

            WRITE_SEPARATOR;
         }
      }

      if (shadowDistPathPrefix && showProgramPath)
         CHECK_AND_MARK_DIST_PATH_PREFIXES(cmdline);

      if (cmdlineBasenameEnd > cmdlineBasenameStart)
         WRITE_HIGHLIGHT(showProgramPath ? cmdlineBasenameStart : 0, cmdlineBasenameEnd - cmdlineBasenameStart, baseAttr, CMDLINE_HIGHLIGHT_FLAG_BASENAME);

      if (this->procExeDeleted)
         WRITE_HIGHLIGHT(showProgramPath ? cmdlineBasenameStart : 0, cmdlineBasenameEnd - cmdlineBasenameStart, delExeAttr, CMDLINE_HIGHLIGHT_FLAG_DELETED);
      else if (this->usesDeletedLib)
         WRITE_HIGHLIGHT(showProgramPath ? cmdlineBasenameStart : 0, cmdlineBasenameEnd - cmdlineBasenameStart, delLibAttr, CMDLINE_HIGHLIGHT_FLAG_DELETED);

      (void)stpcpyWithNewlineConversion(str, cmdline + (showProgramPath ? 0 : cmdlineBasenameStart));

      return;
   }

   int exeLen = strlen(this->procExe);
   int exeBasenameOffset = this->procExeBasenameOffset;
   int exeBasenameLen = exeLen - exeBasenameOffset;

   assert(exeBasenameOffset >= 0);
   assert(exeBasenameOffset <= (int)strlen(procExe));

   bool haveCommInExe = false;
   if (procExe && procComm && (!Process_isUserlandThread(this) || showThreadNames)) {
      haveCommInExe = strncmp(procExe + exeBasenameOffset, procComm, TASK_COMM_LEN - 1) == 0;
   }

   /* Start with copying exe */
   if (showProgramPath) {
      if (shadowDistPathPrefix)
         CHECK_AND_MARK_DIST_PATH_PREFIXES(procExe);
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
      haveCommInCmdline = (!Process_isUserlandThread(this) || showThreadNames) && findCommInCmdline(procComm, cmdline, cmdlineBasenameStart, &commStart, &commEnd);
   }

   int matchLen = matchCmdlinePrefixWithExeSuffix(cmdline, cmdlineBasenameStart, procExe, exeBasenameOffset, exeBasenameLen);

   bool haveCommField = false;

   if (!haveCommInExe && !haveCommInCmdline && procComm && (!Process_isUserlandThread(this) || showThreadNames)) {
      WRITE_SEPARATOR;
      WRITE_HIGHLIGHT(0, strlen(procComm), commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);
      str = stpcpy(str, procComm);
      haveCommField = true;
   }

   if (matchLen) {
      if (stripExeFromCmdline) {
         /* strip the matched exe prefix */
         cmdline += matchLen;

         commStart -= matchLen;
         commEnd -= matchLen;
      } else {
         matchLen = 0;
      }
   }

   if (!matchLen || (haveCommField && *cmdline)) {
      /* cmdline will be a separate field */
      WRITE_SEPARATOR;
   }

   if (shadowDistPathPrefix)
      CHECK_AND_MARK_DIST_PATH_PREFIXES(cmdline);

   if (!haveCommInExe && haveCommInCmdline && !haveCommField && (!Process_isUserlandThread(this) || showThreadNames))
      WRITE_HIGHLIGHT(commStart, commEnd - commStart, commAttr, CMDLINE_HIGHLIGHT_FLAG_COMM);

   /* Display cmdline if it hasn't been consumed by procExe */
   if (*cmdline)
      (void)stpcpyWithNewlineConversion(str, cmdline);

   #undef CHECK_AND_MARK_DIST_PATH_PREFIXES
   #undef CHECK_AND_MARK
   #undef WRITE_SEPARATOR
   #undef WRITE_HIGHLIGHT
}

void Process_writeCommand(const Process* this, int attr, int baseAttr, RichString* str) {
   (void)baseAttr;

   const ProcessMergedCommand* mc = &this->mergedCommand;
   const char* mergedCommand = mc->str;

   int strStart = RichString_size(str);

   const Settings* settings = this->super.host->settings;
   const bool highlightBaseName = settings->highlightBaseName;
   const bool highlightSeparator = true;
   const bool highlightDeleted = settings->highlightDeletedExe;

   if (!mergedCommand) {
      int len = 0;
      const char* cmdline = this->cmdline;

      if (highlightBaseName || !settings->showProgramPath) {
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
            if (settings->showProgramPath) {
               strStart += basename;
            } else {
               cmdline += basename;
            }
            len = this->cmdlineBasenameEnd - basename;
         }
      }

      RichString_appendWide(str, attr, cmdline);

      if (settings->highlightBaseName) {
         RichString_setAttrn(str, baseAttr, strStart, len);
      }

      return;
   }

   RichString_appendWide(str, attr, mergedCommand);

   for (size_t i = 0, hlCount = CLAMP(mc->highlightCount, 0, ARRAYSIZE(mc->highlights)); i < hlCount; i++) {
      const ProcessCmdlineHighlight* hl = &mc->highlights[i];

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

      if (hl->flags & CMDLINE_HIGHLIGHT_FLAG_PREFIXDIR)
         if (!highlightDeleted)
            continue;

      RichString_setAttrn(str, hl->attr, strStart + hl->offset, hl->length);
   }
}

static inline char processStateChar(ProcessState state) {
   switch (state) {
      case UNKNOWN: return '?';
      case RUNNABLE: return 'U';
      case RUNNING: return 'R';
      case QUEUED: return 'Q';
      case WAITING: return 'W';
      case UNINTERRUPTIBLE_WAIT: return 'D';
      case BLOCKED: return 'B';
      case PAGING: return 'P';
      case STOPPED: return 'T';
      case TRACED: return 't';
      case ZOMBIE: return 'Z';
      case DEFUNCT: return 'X';
      case IDLE: return 'I';
      case SLEEPING: return 'S';
      default:
         assert(0);
         return '!';
   }
}

static void Process_rowWriteField(const Row* super, RichString* str, RowField field) {
   const Process* this = (const Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   Process_writeField(this, str, field);
}

void Process_writeField(const Process* this, RichString* str, RowField field) {
   const Row* super = (const Row*) &this->super;
   const Machine* host = super->host;
   const Settings* settings = host->settings;

   bool coloring = settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   size_t n = sizeof(buffer) - 1;

   switch (field) {
   case COMM: {
      int baseattr = CRT_colors[PROCESS_BASENAME];
      if (settings->highlightThreads && Process_isThread(this)) {
         attr = CRT_colors[PROCESS_THREAD];
         baseattr = CRT_colors[PROCESS_THREAD_BASENAME];
      }
      const ScreenSettings* ss = settings->ss;
      if (!ss->treeView || super->indent == 0) {
         Process_writeCommand(this, attr, baseattr, str);
         return;
      }

      char* buf = buffer;
      const bool lastItem = (super->indent < 0);

      for (uint32_t indent = (super->indent < 0 ? -super->indent : super->indent); indent > 1; indent >>= 1) {
         int written, ret;
         if (indent & 1U) {
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
      xSnprintf(buf, n, "%s%s ", draw, super->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
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

      Row_printLeftAlignedField(str, attr, procComm, TASK_COMM_LEN - 1);
      return;
   }
   case PROC_EXE: {
      const char* procExe;
      if (this->procExe) {
         attr = CRT_colors[Process_isUserlandThread(this) ? PROCESS_THREAD_BASENAME : PROCESS_BASENAME];
         if (settings->highlightDeletedExe) {
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

      Row_printLeftAlignedField(str, attr, procExe, TASK_COMM_LEN - 1);
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
      Row_printLeftAlignedField(str, attr, cwd, 25);
      return;
   }
   case ELAPSED: {
      const uint64_t rt = host->realtimeMs;
      const uint64_t st = this->starttime_ctime * 1000;
      const uint64_t dt =
         rt < st ? 0 :
         rt - st;
      Row_printTime(str, /* convert to hundreds of a second */ dt / 10, coloring);
      return;
   }
   case MAJFLT: Row_printCount(str, this->majflt, coloring); return;
   case MINFLT: Row_printCount(str, this->minflt, coloring); return;
   case M_RESIDENT: Row_printKBytes(str, this->m_resident, coloring); return;
   case M_VIRT: Row_printKBytes(str, this->m_virt, coloring); return;
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
   case PERCENT_CPU: Row_printPercentage(this->percent_cpu, buffer, n, Row_fieldWidths[PERCENT_CPU], &attr); break;
   case PERCENT_NORM_CPU: {
      float cpuPercentage = this->percent_cpu / host->activeCPUs;
      Row_printPercentage(cpuPercentage, buffer, n, Row_fieldWidths[PERCENT_CPU], &attr);
      break;
   }
   case PERCENT_MEM: Row_printPercentage(this->percent_mem, buffer, n, 4, &attr); break;
   case PGRP: xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->pgrp); break;
   case PID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, Process_getPid(this)); break;
   case PPID: xSnprintf(buffer, n, "%*d ", Process_pidDigits, Process_getParent(this)); break;
   case PRIORITY:
      if (this->priority <= -100)
         xSnprintf(buffer, n, " RT ");
      else
         xSnprintf(buffer, n, "%3ld ", this->priority);
      break;
   case PROCESSOR: xSnprintf(buffer, n, "%3d ", Settings_cpuId(settings, this->processor)); break;
   case SCHEDULERPOLICY: {
      const char* schedPolStr = "N/A";
#ifdef SCHEDULER_SUPPORT
      if (this->scheduling_policy >= 0)
         schedPolStr = Scheduling_formatPolicy(this->scheduling_policy);
#endif
      xSnprintf(buffer, n, "%-5s ", schedPolStr);
      break;
   }
   case SESSION: xSnprintf(buffer, n, "%*d ", Process_pidDigits, this->session); break;
   case STARTTIME: xSnprintf(buffer, n, "%s", this->starttime_show); break;
   case STATE:
      xSnprintf(buffer, n, "%c ", processStateChar(this->state));
      switch (this->state) {
      case RUNNABLE:
      case RUNNING:
      case TRACED:
         attr = CRT_colors[PROCESS_RUN_STATE];
         break;

      case BLOCKED:
      case DEFUNCT:
      case STOPPED:
      case UNINTERRUPTIBLE_WAIT:
      case ZOMBIE:
         attr = CRT_colors[PROCESS_D_STATE];
         break;

      case QUEUED:
      case WAITING:
      case IDLE:
      case SLEEPING:
         attr = CRT_colors[PROCESS_SHADOW];
         break;

      case UNKNOWN:
      case PAGING:
         break;
      }
      break;
   case ST_UID: xSnprintf(buffer, n, "%*d ", Process_uidDigits, this->st_uid); break;
   case TIME: Row_printTime(str, this->time, coloring); return;
   case TGID:
      if (Process_getThreadGroup(this) == Process_getPid(this))
         attr = CRT_colors[PROCESS_SHADOW];

      xSnprintf(buffer, n, "%*d ", Process_pidDigits, Process_getThreadGroup(this));
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
      if (this->elevated_priv)
         attr = CRT_colors[PROCESS_PRIV];
      else if (host->htopUserId != this->st_uid)
         attr = CRT_colors[PROCESS_SHADOW];

      if (this->user) {
         Row_printLeftAlignedField(str, attr, this->user, 10);
         return;
      }

      xSnprintf(buffer, n, "%-10d ", this->st_uid);
      break;
   default:
      if (DynamicColumn_writeField(this, str, field))
         return;
      assert(0 && "Process_writeField: default key reached"); /* should never be reached */
      xSnprintf(buffer, n, "- ");
      break;
   }

   RichString_appendAscii(str, attr, buffer);
}

void Process_done(Process* this) {
   assert(this != NULL);
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
const char* Process_getCommand(const Process* this) {
   const Settings* settings = this->super.host->settings;

   if ((Process_isUserlandThread(this) && settings->showThreadNames) || !this->mergedCommand.str) {
      return this->cmdline;
   }

   return this->mergedCommand.str;
}

static const char* Process_getSortKey(const Process* this) {
   return Process_getCommand(this);
}

const char* Process_rowGetSortKey(Row* super) {
   const Process* this = (const Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   return Process_getSortKey(this);
}

/* Test whether display must highlight this row (if the htop UID matches) */
static bool Process_isHighlighted(const Process* this) {
   const Machine* host = this->super.host;
   const Settings* settings = host->settings;
   return settings->shadowOtherUsers && this->st_uid != host->htopUserId;
}

bool Process_rowIsHighlighted(const Row* super) {
   const Process* this = (const Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   return Process_isHighlighted(this);
}

/* Test whether display must follow parent process (if this thread is hidden) */
static bool Process_isVisible(const Process* p, const Settings* settings) {
   if (settings->hideUserlandThreads)
      return !Process_isThread(p);
   return true;
}

bool Process_rowIsVisible(const Row* super, const Table* table) {
   const Process* this = (const Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   return Process_isVisible(this, table->host->settings);
}

/* Test whether display must filter out this process (various mechanisms) */
static bool Process_matchesFilter(const Process* this, const Table* table) {
   const Machine* host = table->host;
   if (host->userId != (uid_t) -1 && this->st_uid != host->userId)
      return true;

   const char* incFilter = table->incFilter;
   if (incFilter && !String_contains_i(Process_getCommand(this), incFilter, true))
      return true;

   const ProcessTable* pt = (const ProcessTable*) host->activeTable;
   assert(Object_isA((const Object*) pt, (const ObjectClass*) &ProcessTable_class));
   if (pt->pidMatchList && !Hashtable_get(pt->pidMatchList, Process_getThreadGroup(this)))
      return true;

   return false;
}

bool Process_rowMatchesFilter(const Row* super, const Table* table) {
   const Process* this = (const Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   return Process_matchesFilter(this, table);
}

void Process_init(Process* this, const Machine* host) {
   Row_init(&this->super, host);

   this->cmdlineBasenameEnd = -1;
   this->st_uid = (uid_t)-1;
}

static bool Process_setPriority(Process* this, int priority) {
   if (Settings_isReadonly())
      return false;

   int old_prio = getpriority(PRIO_PROCESS, Process_getPid(this));
   int err = setpriority(PRIO_PROCESS, Process_getPid(this), priority);

   if (err == 0 && old_prio != getpriority(PRIO_PROCESS, Process_getPid(this))) {
      this->nice = priority;
   }
   return (err == 0);
}

bool Process_rowSetPriority(Row* super, int priority) {
   Process* this = (Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   return Process_setPriority(this, priority);
}

bool Process_rowChangePriorityBy(Row* super, Arg delta) {
   Process* this = (Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   return Process_setPriority(this, this->nice + delta.i);
}

static bool Process_sendSignal(Process* this, Arg sgn) {
   return kill(Process_getPid(this), sgn.i) == 0;
}

bool Process_rowSendSignal(Row* super, Arg sgn) {
   Process* this = (Process*) super;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));
   return Process_sendSignal(this, sgn);
}

int Process_compare(const void* v1, const void* v2) {
   const Process* p1 = (const Process*)v1;
   const Process* p2 = (const Process*)v2;

   const ScreenSettings* ss = p1->super.host->settings->ss;

   ProcessField key = ScreenSettings_getActiveSortKey(ss);

   int result = Process_compareByKey(p1, p2, key);

   // Implement tie-breaker (needed to make tree mode more stable)
   if (!result)
      return SPACESHIP_NUMBER(Process_getPid(p1), Process_getPid(p2));

   return (ScreenSettings_getActiveDirection(ss) == 1) ? result : -result;
}

int Process_compareByParent(const Row* r1, const Row* r2) {
   int result = Row_compareByParent_Base(r1, r2);

   if (result != 0)
      return result;

   return Process_compare(r1, r2);
}

int Process_compareByKey_Base(const Process* p1, const Process* p2, ProcessField key) {
   int r;

   switch (key) {
   case PERCENT_CPU:
   case PERCENT_NORM_CPU:
      return compareRealNumbers(p1->percent_cpu, p2->percent_cpu);
   case PERCENT_MEM:
      return SPACESHIP_NUMBER(p1->m_resident, p2->m_resident);
   case COMM:
      return SPACESHIP_NULLSTR(Process_getCommand(p1), Process_getCommand(p2));
   case PROC_COMM: {
      const char* comm1 = p1->procComm ? p1->procComm : (Process_isKernelThread(p1) ? kthreadID : "");
      const char* comm2 = p2->procComm ? p2->procComm : (Process_isKernelThread(p2) ? kthreadID : "");
      return SPACESHIP_NULLSTR(comm1, comm2);
   }
   case PROC_EXE: {
      const char* exe1 = p1->procExe ? (p1->procExe + p1->procExeBasenameOffset) : (Process_isKernelThread(p1) ? kthreadID : "");
      const char* exe2 = p2->procExe ? (p2->procExe + p2->procExeBasenameOffset) : (Process_isKernelThread(p2) ? kthreadID : "");
      return SPACESHIP_NULLSTR(exe1, exe2);
   }
   case CWD:
      return SPACESHIP_NULLSTR(p1->procCwd, p2->procCwd);
   case ELAPSED:
      r = -SPACESHIP_NUMBER(p1->starttime_ctime, p2->starttime_ctime);
      return r != 0 ? r : SPACESHIP_NUMBER(Process_getPid(p1), Process_getPid(p2));
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
      return SPACESHIP_NUMBER(Process_getPid(p1), Process_getPid(p2));
   case PPID:
      return SPACESHIP_NUMBER(Process_getParent(p1), Process_getParent(p2));
   case PRIORITY:
      return SPACESHIP_NUMBER(p1->priority, p2->priority);
   case PROCESSOR:
      return SPACESHIP_NUMBER(p1->processor, p2->processor);
   case SCHEDULERPOLICY:
      return SPACESHIP_NUMBER(p1->scheduling_policy, p2->scheduling_policy);
   case SESSION:
      return SPACESHIP_NUMBER(p1->session, p2->session);
   case STARTTIME:
      r = SPACESHIP_NUMBER(p1->starttime_ctime, p2->starttime_ctime);
      return r != 0 ? r : SPACESHIP_NUMBER(Process_getPid(p1), Process_getPid(p2));
   case STATE:
      return SPACESHIP_NUMBER(p1->state, p2->state);
   case ST_UID:
      return SPACESHIP_NUMBER(p1->st_uid, p2->st_uid);
   case TIME:
      return SPACESHIP_NUMBER(p1->time, p2->time);
   case TGID:
      return SPACESHIP_NUMBER(Process_getThreadGroup(p1), Process_getThreadGroup(p2));
   case TPGID:
      return SPACESHIP_NUMBER(p1->tpgid, p2->tpgid);
   case TTY:
      /* Order no tty last */
      return SPACESHIP_DEFAULTSTR(p1->tty_name, p2->tty_name, "\x7F");
   case USER:
      return SPACESHIP_NULLSTR(p1->user, p2->user);
   default:
      CRT_debug("Process_compareByKey_Base() called with key %d", key);
      assert(0 && "Process_compareByKey_Base: default key reached"); /* should never be reached */
      return SPACESHIP_NUMBER(Process_getPid(p1), Process_getPid(p2));
   }
}

void Process_updateComm(Process* this, const char* comm) {
   if (!this->procComm && !comm)
      return;

   if (this->procComm && comm && String_eq(this->procComm, comm))
      return;

   free(this->procComm);
   this->procComm = comm ? xStrdup(comm) : NULL;

   this->mergedCommand.lastUpdate = 0;
}

static int skipPotentialPath(const char* cmdline, int end) {
   if (cmdline[0] != '/')
      return 0;

   int slash = 0;
   for (int i = 1; i < end; i++) {
      if (cmdline[i] == '/' && cmdline[i + 1] != '\0') {
         slash = i + 1;
         continue;
      }

      if (cmdline[i] == ' ' && cmdline[i - 1] != '\\')
         return slash;

      if (cmdline[i] == ':' && cmdline[i + 1] == ' ')
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

   this->mergedCommand.lastUpdate = 0;
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

   this->mergedCommand.lastUpdate = 0;
}

void Process_updateCPUFieldWidths(float percentage) {
   if (percentage < 99.9F) {
      Row_updateFieldWidth(PERCENT_CPU, 4);
      Row_updateFieldWidth(PERCENT_NORM_CPU, 4);
      return;
   }

   // Add additional two characters, one for "." and another for precision.
   uint8_t width = ceil(log10(percentage + 0.1)) + 2;

   Row_updateFieldWidth(PERCENT_CPU, width);
   Row_updateFieldWidth(PERCENT_NORM_CPU, width);
}

const ProcessClass Process_class = {
   .super = {
      .super = {
         .extends = Class(Row),
         .display = Row_display,
         .delete = Process_delete,
         .compare = Process_compare
      },
      .isHighlighted = Process_rowIsHighlighted,
      .isVisible = Process_rowIsVisible,
      .matchesFilter = Process_rowMatchesFilter,
      .sortKeyString = Process_rowGetSortKey,
      .compareByParent = Process_compareByParent,
      .writeField = Process_rowWriteField
   },
};
