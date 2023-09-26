/*
htop - CygwinProcessTable.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "cygwin/CygwinProcessTable.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "Compat.h"
#include "Object.h"
#include "ProcessTable.h"
#include "XUtils.h"
#include "generic/fast_strtoull.h"
#include "generic/fdopenat.h"
#include "cygwin/CygwinMachine.h"
#include "cygwin/CygwinProcess.h"


ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   CygwinProcessTable* this = xCalloc(1, sizeof(CygwinProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(CygwinProcess), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* super) {
   CygwinProcessTable* this = (CygwinProcessTable*) super;
   ProcessTable_done(&this->super);
   free(this);
}

static inline unsigned long long CygwinProcessTable_adjustTime(const CygwinMachine* chost, unsigned long long t) {
   return t * 100 / chost->jiffies;
}

/* Reference: https://github.com/cygwin/cygwin/blob/a9e8e3d1cb8235f513f4d8434509acf287494fcf/winsup/cygwin/fhandler/process.cc#L1234 */
static inline ProcessState CygwinProcessTable_getProcessState(char state) {
   switch (state) {
      case 'R':
      case 'O': return RUNNING;
      case 'D':
      case 'S': return SLEEPING;
      case 'Z': return ZOMBIE;
      case 'T': return STOPPED;
      default: return UNKNOWN;
   }
}

static bool CygwinProcessTable_readStatFile(CygwinProcess* cp, openat_arg_t procFd, const CygwinMachine* chost, char* command, size_t commLen) {
   Process* process = &cp->super;

   char buf[MAX_READ + 1];
   ssize_t r = xReadfileat(procFd, "stat", buf, sizeof(buf));
   if (r < 0)
      return false;

   /* (1) pid   -  %d */
   assert(Process_getPid(process) == atoi(buf));
   char* location = strchr(buf, ' ');
   if (!location)
      return false;

   /* (2) comm  -  (%s) */
   location += 2;
   char* end = strrchr(location, ')');
   if (!end)
      return false;

   String_safeStrncpy(command, location, MINIMUM((size_t)(end - location + 1), commLen));

   location = end + 2;

   /* (3) state  -  %c */
   process->state = CygwinProcessTable_getProcessState(location[0]);
   location += 2;

   /* (4) ppid  -  %d */
   Process_setParent(process, strtol(location, &location, 10));
   location += 1;

   /* (5) pgrp  -  %d */
   process->pgrp = strtol(location, &location, 10);
   location += 1;

   /* (6) session  -  %d */
   process->session = strtol(location, &location, 10);
   location += 1;

   /* (7) tty_nr  -  %d */
   process->tty_nr = strtoul(location, &location, 10);
   location += 1;

   /* (8) tpgid  -  %d */
   process->tpgid = strtol(location, &location, 10);
   location += 1;

   /* (9) flags  -  %u */
   cp->flags = strtoul(location, &location, 10);
   location += 1;

   /* (10) minflt  -  %lu */
   process->minflt = strtoull(location, &location, 10);
   location += 1;

   /* (11) cminflt  -  %lu */
   cp->cminflt = strtoull(location, &location, 10);
   location += 1;

   /* (12) majflt  -  %u */
   process->majflt = strtoul(location, &location, 10);
   location += 1;

   /* (13) cmajflt  -  %u */
   cp->cmajflt = strtoul(location, &location, 10);
   location += 1;

   /* (14) utime  -  %lu */
   cp->utime = CygwinProcessTable_adjustTime(chost, strtoull(location, &location, 10));
   location += 1;

   /* (15) stime  -  %lu */
   cp->stime = CygwinProcessTable_adjustTime(chost, strtoull(location, &location, 10));
   location += 1;

   /* (16) cutime  -  %lu */
   cp->cutime = CygwinProcessTable_adjustTime(chost, strtoull(location, &location, 10));
   location += 1;

   /* (17) cstime  -  %lu */
   cp->cstime = CygwinProcessTable_adjustTime(chost, strtoull(location, &location, 10));
   location += 1;

   /* (18) priority  -  %d */
   process->priority = strtol(location, &location, 10);
   location += 1;

   /* (19) nice  -  %d */
   process->nice = strtol(location, &location, 10);
   location += 1;

   /* (20) num_threads  -  %d */
   process->nlwp = strtol(location, &location, 10);
   location += 1;

   /* Skip (21) itrealvalue  -  %d */
   location = strchr(location, ' ') + 1;

   /* (22) starttime  -  %lu */
   if (process->starttime_ctime == 0) {
      process->starttime_ctime = chost->boottime + CygwinProcessTable_adjustTime(chost, strtoull(location, &location, 10)) / 100;
   } else {
      location = strchr(location, ' ');
   }

   /* Ignore further fields */

   process->processor = 0;  // FIXME

   process->time = cp->utime + cp->stime;

   return true;
}

static bool CygwinProcessTable_updateUser(const Machine* host, Process* process, openat_arg_t procFd) {
   struct stat sstat;
#ifdef HAVE_OPENAT
   int statok = fstat(procFd, &sstat);
#else
   int statok = stat(procFd, &sstat);
#endif
   if (statok == -1)
      return false;

   if (process->st_uid != sstat.st_uid) {
      process->st_uid = sstat.st_uid;
      process->user = UsersTable_getRef(host->usersTable, sstat.st_uid);
   }

   return true;
}

static void CygwinProcessTable_readMaps(CygwinProcess* process, openat_arg_t procFd) {
   Process* proc = (Process*)process;

   proc->usesDeletedLib = false;

   FILE* mapsfile = fopenat(procFd, "maps", "r");
   if (!mapsfile)
      return;

   char buffer[1024];
   while (fgets(buffer, sizeof(buffer), mapsfile)) {
      bool map_execute;
      unsigned int map_devmaj;
      unsigned int map_devmin;
      uint64_t map_inode;

      // Short circuit test: Look for a slash
      if (!strchr(buffer, '/'))
         continue;

      // Parse format: "%Lx-%Lx %4s %x %2x:%2x %Ld"
      char* readptr = buffer;

      fast_strtoull_hex(&readptr, 16);
      if ('-' != *readptr++)
         continue;

      fast_strtoull_hex(&readptr, 16);
      if (' ' != *readptr++)
         continue;

      map_execute = (readptr[2] == 'x');
      readptr += 4;
      if (' ' != *readptr++)
         continue;

      while (*readptr > ' ')
         readptr++; // Skip parsing this hex value
      if (' ' != *readptr++)
         continue;

      map_devmaj = fast_strtoull_hex(&readptr, 4);
      if (':' != *readptr++)
         continue;

      map_devmin = fast_strtoull_hex(&readptr, 4);
      if (' ' != *readptr++)
         continue;

      //Minor shortcut: Once we know there's no file for this region, we skip
      if (!map_devmaj && !map_devmin)
         continue;

      map_inode = fast_strtoull_dec(&readptr, 20);
      if (!map_inode)
         continue;

      if (map_execute && !proc->usesDeletedLib) {
         while (*readptr == ' ')
            readptr++;

         if (*readptr != '/')
            continue;

         if (strstr(readptr, "/$Recycle.Bin/")) {
            proc->usesDeletedLib = true;
            break;
         }
      }
   }

   fclose(mapsfile);

   // m_lrs is set in CygwinProcessTable_readStatmFile from /proc/statm
}

static bool CygwinProcessTable_readStatmFile(CygwinProcess* process, openat_arg_t procFd, const CygwinMachine* host) {
   FILE* statmfile = fopenat(procFd, "statm", "r");
   if (!statmfile)
      return false;

   long int dummy;

   int r = fscanf(statmfile, "%ld %ld %ld %ld %ld %ld %ld",
                  &process->super.m_virt,
                  &process->super.m_resident,
                  &process->m_share,
                  &process->m_trs,
                  &process->m_lrs,
                  &process->m_drs,
                  &dummy); /* unused; always 0 */
   fclose(statmfile);

   if (r == 7) {
      process->super.m_virt *= host->pageSizeKB;
      process->super.m_resident *= host->pageSizeKB;
   }

   return r == 7;
}

static void CygwinProcessTable_readCwd(CygwinProcess* process, openat_arg_t procFd) {
   char pathBuffer[PATH_MAX + 1] = {0};

#if defined(HAVE_READLINKAT) && defined(HAVE_OPENAT)
   ssize_t r = readlinkat(procFd, "cwd", pathBuffer, sizeof(pathBuffer) - 1);
#else
   ssize_t r = Compat_readlink(procFd, "cwd", pathBuffer, sizeof(pathBuffer) - 1);
#endif

   if (r < 0) {
      free(process->super.procCwd);
      process->super.procCwd = NULL;
      return;
   }

   pathBuffer[r] = '\0';

   if (process->super.procCwd && String_eq(process->super.procCwd, pathBuffer))
      return;

   free_and_xStrdup(&process->super.procCwd, pathBuffer);
}

static bool CygwinProcessTable_readCmdlineFile(Process* process, openat_arg_t procFd) {
   char command[4096 + 1]; // max cmdline length on Linux
   ssize_t amtRead = xReadfileat(procFd, "cmdline", command, sizeof(command));
   if (amtRead <= 0)
      return false;

   int tokenEnd = 0;
   int tokenStart = 0;
   int lastChar = 0;
   bool argSepNUL = false;
   bool argSepSpace = false;

   for (int i = 0; i < amtRead; i++) {
      /* newline used as delimiter - when forming the mergedCommand, newline is
       * converted to space by Process_makeCommandStr */
      if (command[i] == '\0') {
         command[i] = '\n';
      } else {
         /* Record some information for the argument parsing heuristic below. */
         if (tokenEnd)
            argSepNUL = true;
         if (command[i] <= ' ')
            argSepSpace = true;
      }

      if (command[i] == '\n') {
         if (tokenEnd == 0) {
            tokenEnd = i;
         }
      } else {
         /* htop considers the next character after the last / that is before
          * basenameOffset, as the start of the basename in cmdline - see
          * Process_writeCommand */
         if (!tokenEnd && command[i] == '/') {
            tokenStart = i + 1;
         }
         lastChar = i;
      }
   }

   command[lastChar + 1] = '\0';

   if (!argSepNUL && argSepSpace) {
      /* Argument parsing heuristic.
       *
       * This heuristic is used for processes that rewrite their command line.
       * Normally the command line is split by using NUL bytes between each argument.
       * But some programs like chrome flatten this using spaces.
       *
       * This heuristic tries its best to undo this loss of information.
       * To achieve this, we treat every character <= 32 as argument separators
       * (i.e. all of ASCII control sequences and space).
       * We then search for the basename of the cmdline in the first argument we found that way.
       * As path names may contain we try to cross-validate if the path we got that way exists.
       */

      tokenStart = tokenEnd = 0;

      // From initial scan we know there's at least one space.
      // Check if that's part of a filename for an existing file.
      if (Compat_faccessat(AT_FDCWD, command, F_OK, AT_SYMLINK_NOFOLLOW) != 0) {
         // If we reach here the path does not exist.
         // Thus begin searching for the part of it that actually is.

         int tokenArg0Start = 0;

         for (int i = 0; i <= lastChar; i++) {
            /* Any ASCII control or space used as delimiter */
            char tmpCommandChar = command[i];

            if (command[i] <= ' ') {
               if (!tokenEnd) {
                  command[i] = '\0';

                  bool found = Compat_faccessat(AT_FDCWD, command, F_OK, AT_SYMLINK_NOFOLLOW) == 0;

                  // Restore if this wasn't it
                  command[i] = found ? '\n' : tmpCommandChar;

                  if (found)
                     tokenEnd = i;
                  if (!tokenArg0Start)
                     tokenArg0Start = tokenStart;
               } else {
                  // Split on every further separator, regardless of path correctness
                  command[i] = '\n';
               }
            } else if (!tokenEnd) {
               if (command[i] == '/' || (command[i] == '\\' && (!tokenStart || command[tokenStart - 1] == '\\'))) {
                  tokenStart = i + 1;
               } else if (command[i] == ':' && (command[i + 1] != '/' && command[i + 1] != '\\')) {
                  tokenEnd = i;
               }
            }
         }

         if (!tokenEnd) {
            tokenStart = tokenArg0Start;

            // No token delimiter found, forcibly split
            for (int i = 0; i <= lastChar; i++) {
               if (command[i] <= ' ') {
                  command[i] = '\n';
                  if (!tokenEnd) {
                     tokenEnd = i;
                  }
               }
            }
         }
      }

      /* Some command lines are hard to parse, like
       *   file.so [kdeinit5] file local:/run/user/1000/klauncherdqbouY.1.slave-socket local:/run/user/1000/kded5TwsDAx.1.slave-socket
       * Reset if start is behind end.
       */
      if (tokenStart >= tokenEnd)
         tokenStart = tokenEnd = 0;
   }

   if (tokenEnd == 0) {
      tokenEnd = lastChar + 1;
   }

   Process_updateCmdline(process, command, tokenStart, tokenEnd);

   // comm is set in CygwinProcessTable_recurseProcTree from /proc/[pid]/stat

   char filename[MAX_NAME + 1];

   /* execve could change /proc/[pid]/exe, so procExe should be updated */
#if defined(HAVE_READLINKAT) && defined(HAVE_OPENAT)
   amtRead = readlinkat(procFd, "exe", filename, sizeof(filename) - 1);
#else
   amtRead = Compat_readlink(procFd, "exe", filename, sizeof(filename) - 1);
#endif
   if (amtRead > 0) {
      filename[amtRead] = 0;
      if (!process->procExe ||
          (!process->procExeDeleted && !String_eq(filename, process->procExe)) ||
          process->procExeDeleted) {

         bool oldExeDeleted = process->procExeDeleted;

         process->procExeDeleted = access(filename, F_OK) < 0;

         if (oldExeDeleted != process->procExeDeleted)
            process->mergedCommand.lastUpdate = 0;

         Process_updateExe(process, filename);
      }
   } else if (process->procExe) {
      Process_updateExe(process, NULL);
      process->procExeDeleted = false;
   }

   return true;
}

static bool isOlderThan(const Process* proc, unsigned int seconds) {
   const Machine* host = proc->super.host;

   assert(host->realtimeMs > 0);

   /* Starttime might not yet be parsed */
   if (proc->starttime_ctime <= 0)
      return false;

   uint64_t realtime = host->realtimeMs / 1000;

   if (realtime < (uint64_t)proc->starttime_ctime)
      return false;

   return realtime - proc->starttime_ctime > seconds;
}

static char* CygwinProcessTable_getDevname(dev_t dev) {
   // TODO
   (void) dev;

   return NULL;
}

static bool CygwinProcessTable_recurseProcTree(CygwinProcessTable* this, openat_arg_t parentFd, const CygwinMachine* chost, const char* dirname, const Process* parent) {
   ProcessTable* pt = (ProcessTable*) this;
   const Machine* host = &chost->super;
   const Settings* settings = host->settings;
   const ScreenSettings* ss = settings->ss;
   const struct dirent* entry;

#ifdef HAVE_OPENAT
   int dirFd = openat(parentFd, dirname, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
   if (dirFd < 0)
      return false;
   DIR* dir = fdopendir(dirFd);
#else
   char dirFd[4096];
   xSnprintf(dirFd, sizeof(dirFd), "%s/%s", parentFd, dirname);
   DIR* dir = opendir(dirFd);
#endif
   if (!dir) {
      Compat_openatArgClose(dirFd);
      return false;
   }

   const bool hideKernelThreads = settings->hideKernelThreads;
   const bool hideUserlandThreads = settings->hideUserlandThreads;
   while ((entry = readdir(dir)) != NULL) {
      const char* name = entry->d_name;

      // Ignore all non-directories
      if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
         continue;
      }

      // Just skip all non-number directories.
      if (name[0] < '0' || name[0] > '9') {
         continue;
      }

      // filename is a number: process directory
      int pid;
      {
         char* endptr;
         unsigned long parsedPid = strtoul(name, &endptr, 10);
         if (parsedPid == 0 || parsedPid == ULONG_MAX || *endptr != '\0')
            continue;
         pid = parsedPid;
      }

      // Skip task directory of main thread
      if (parent && pid == Process_getPid(parent))
         continue;

#ifdef HAVE_OPENAT
      int procFd = openat(dirFd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
      if (procFd < 0)
         continue;
#else
      char procFd[4096];
      xSnprintf(procFd, sizeof(procFd), "%s/%s", dirFd, entry->d_name);
#endif

      bool preExisting;
      Process* proc = ProcessTable_getProcess(pt, pid, &preExisting, CygwinProcess_new);
      CygwinProcess* cp = (CygwinProcess*) proc;

      Process_setThreadGroup(proc, parent ? Process_getPid(parent) : pid);
      proc->isUserlandThread = Process_getPid(proc) != Process_getThreadGroup(proc);

      /*
       * These conditions will not trigger on first occurrence, cause we need to
       * add the process to the ProcessTable and do all one time scans
       * (e.g. parsing the cmdline to detect a kernel thread)
       * But it will short-circuit subsequent scans.
       */
      if (preExisting && hideKernelThreads && Process_isKernelThread(proc)) {
         proc->super.updated = true;
         proc->super.show = false;
         pt->kernelThreads++;
         pt->totalTasks++;
         Compat_openatArgClose(procFd);
         continue;
      }
      if (preExisting && hideUserlandThreads && Process_isUserlandThread(proc)) {
         proc->super.updated = true;
         proc->super.show = false;
         pt->userlandThreads++;
         pt->totalTasks++;
         Compat_openatArgClose(procFd);
         continue;
      }

      if (!CygwinProcessTable_readStatmFile(cp, procFd, chost))
         goto errorReadingProcess;

      {
         bool prev = proc->usesDeletedLib;

         if (!proc->isKernelThread && !proc->isUserlandThread &&
            ((settings->highlightDeletedExe && !proc->procExeDeleted && isOlderThan(proc, 10)))) {

            CygwinProcessTable_readMaps(cp, procFd);
         } else {
            /* Copy from process structure in threads and reset if setting got disabled */
            proc->usesDeletedLib = (proc->isUserlandThread && parent) ? parent->usesDeletedLib : false;
         }

         if (prev != proc->usesDeletedLib)
            proc->mergedCommand.lastUpdate = 0;
      }

      char statCommand[MAX_NAME + 1];
      unsigned long long int lasttimes = (cp->utime + cp->stime);
      unsigned long int tty_nr = proc->tty_nr;
      if (!CygwinProcessTable_readStatFile(cp, procFd, chost, statCommand, sizeof(statCommand)))
         goto errorReadingProcess;

      Process_updateComm(proc, statCommand);

      if (tty_nr != proc->tty_nr) {
         free(proc->tty_name);
         proc->tty_name = CygwinProcessTable_getDevname(proc->tty_nr);
      }

      float percent_cpu = saturatingSub(cp->utime + cp->stime, lasttimes) / chost->period * 100.0;
      proc->percent_cpu = MINIMUM(percent_cpu, host->activeCPUs * 100.0F);
      proc->percent_mem = proc->m_resident / (double)(host->totalMem) * 100.0;
      Process_updateCPUFieldWidths(proc->percent_cpu);

      if (! CygwinProcessTable_updateUser(host, proc, procFd))
         goto errorReadingProcess;

      if (!preExisting) {

         if (proc->isKernelThread) {
            Process_updateCmdline(proc, NULL, 0, 0);
         } else if (!CygwinProcessTable_readCmdlineFile(proc, procFd)) {
            Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
         }

         Process_fillStarttimeBuffer(proc);

         ProcessTable_add(pt, proc);
      } else {
         if (settings->updateProcessNames && proc->state != ZOMBIE) {
            if (proc->isKernelThread) {
               Process_updateCmdline(proc, NULL, 0, 0);
            } else if (!CygwinProcessTable_readCmdlineFile(proc, procFd)) {
               Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
            }
         }
      }

      if (ss->flags & PROCESS_FLAG_CWD) {
         CygwinProcessTable_readCwd(cp, procFd);
      }

      #ifdef SCHEDULER_SUPPORT
      if (ss->flags & PROCESS_FLAG_SCHEDPOL) {
         Scheduling_readProcessPolicy(proc);
      }
      #endif

      if (!proc->cmdline && statCommand[0] &&
          (proc->state == ZOMBIE || Process_isKernelThread(proc) || settings->showThreadNames)) {
         Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
      }

      /*
       * Final section after all data has been gathered
       */

      proc->super.updated = true;
      Compat_openatArgClose(procFd);

      if (Process_isKernelThread(proc)) {
         pt->kernelThreads++;
      } else if (Process_isUserlandThread(proc)) {
         pt->userlandThreads++;
      }

      /* Set at the end when we know if a new entry is a thread */
      proc->super.show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      pt->totalTasks++;
      if (proc->state == RUNNING)
         pt->runningTasks++;
      continue;

      // Exception handler.

errorReadingProcess:
      {
#ifdef HAVE_OPENAT
         if (procFd >= 0)
            close(procFd);
#endif

         if (preExisting) {
            /*
             * The only real reason for coming here would be the process going away with
             * its /proc files disappearing (!HAVE_OPENAT).
             * However, we want to keep in the process list for now for the "highlight dying" mode.
             */
         } else {
            /* A really short-lived process that we don't have full info about */
            Process_delete((Object*)proc);
         }
      }
   }
   closedir(dir);
   return true;
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   CygwinProcessTable* this = (CygwinProcessTable*) super;
   const Machine* host = super->super.host;
   const CygwinMachine* chost = (const CygwinMachine*) host;

   /* PROCDIR is an absolute path */
   assert(PROCDIR[0] == '/');
#ifdef HAVE_OPENAT
   openat_arg_t rootFd = AT_FDCWD;
#else
   openat_arg_t rootFd = "";
#endif

   CygwinProcessTable_recurseProcTree(this, rootFd, chost, PROCDIR, NULL);
}
