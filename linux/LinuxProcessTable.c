/*
htop - LinuxProcessTable.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/LinuxProcessTable.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <linux/capability.h> // raw syscall, no libcap  // IWYU pragma: keep // IWYU pragma: no_include <sys/capability.h>
#include <sys/stat.h>

#include "GPUMeter.h"
#include "Hashtable.h"
#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "Row.h"
#include "RowField.h"
#include "Scheduling.h"
#include "Settings.h"
#include "Table.h"
#include "UsersTable.h"
#include "linux/CGroupUtils.h"
#include "linux/Compat.h"
#include "linux/GPU.h"
#include "linux/LinuxMachine.h"
#include "linux/LinuxProcess.h"
#include "linux/Platform.h" // needed for GNU/hurd to get PATH_MAX  // IWYU pragma: keep

#ifdef HAVE_DELAYACCT
#include "linux/LibNl.h"
#endif

#if defined(MAJOR_IN_MKDEV)
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS)
#include <sys/sysmacros.h>
#endif

/* Not exposed yet. Defined at include/linux/sched.h */
#ifndef PF_KTHREAD
#define PF_KTHREAD 0x00200000
#endif

/* Inode number of the PID namespace of htop */
static ino_t rootPidNs = (ino_t)-1;


static FILE* fopenat(openat_arg_t openatArg, const char* pathname, const char* mode) {
   assert(String_eq(mode, "r")); /* only currently supported mode */

   int fd = Compat_openat(openatArg, pathname, O_RDONLY);
   if (fd < 0)
      return NULL;

   FILE* fp = fdopen(fd, mode);
   if (!fp)
      close(fd);

   return fp;
}

static inline uint64_t fast_strtoull_dec(char** str, int maxlen) {
   uint64_t result = 0;

   if (!maxlen)
      maxlen = 20; // length of maximum value of 18446744073709551615

   while (maxlen-- && **str >= '0' && **str <= '9') {
      result *= 10;
      result += **str - '0';
      (*str)++;
   }

   return result;
}

static long long fast_strtoll_dec(char** str, int maxlen) {
   bool neg = false;

   if (**str == '-') {
      neg = true;
      (*str)++;
   }

   unsigned long long res = fast_strtoull_dec(str, maxlen);
   assert(res <= LLONG_MAX);
   long long result = (long long)res;

   return neg ? -result : result;
}

static long fast_strtol_dec(char** str, int maxlen) {
   long long result = fast_strtoll_dec(str, maxlen);
   assert(result <= LONG_MAX);
   assert(result >= LONG_MIN);
   return (long)result;
}

static unsigned long fast_strtoul_dec(char** str, int maxlen) {
   unsigned long long result = fast_strtoull_dec(str, maxlen);
   assert(result <= ULONG_MAX);
   return (unsigned long)result;
}

static inline uint64_t fast_strtoull_hex(char** str, int maxlen) {
   register uint64_t result = 0;
   register int nibble, letter;
   const long valid_mask = 0x03FF007E;

   if (!maxlen)
      --maxlen;

   while (maxlen--) {
      nibble = (unsigned char)**str;
      if (!(valid_mask & (1 << (nibble & 0x1F))))
         break;
      if ((nibble < '0') || (nibble & ~0x20) > 'F')
         break;
      letter = (nibble & 0x40) ? 'A' - '9' - 1 : 0;
      nibble &=~0x20; // to upper
      nibble ^= 0x10; // switch letters and digits
      nibble -= letter;
      nibble &= 0x0f;
      result <<= 4;
      result += (uint64_t)nibble;
      (*str)++;
   }

   return result;
}

static int sortTtyDrivers(const void* va, const void* vb) {
   const TtyDriver* a = (const TtyDriver*) va;
   const TtyDriver* b = (const TtyDriver*) vb;

   int r = SPACESHIP_NUMBER(a->major, b->major);
   if (r)
      return r;

   return SPACESHIP_NUMBER(a->minorFrom, b->minorFrom);
}

static void LinuxProcessTable_initTtyDrivers(LinuxProcessTable* this) {
   TtyDriver* ttyDrivers;

   char buf[16384];
   ssize_t r = Compat_readfile(PROCTTYDRIVERSFILE, buf, sizeof(buf));
   if (r < 0)
      return;

   size_t numDrivers = 0;
   size_t allocd = 10;
   ttyDrivers = xMallocArray(allocd, sizeof(TtyDriver));
   char* at = buf;
   char* path = NULL;
   while (at && *at != '\0') {
      /*
       * Format:
       * [name]  [node path]  [major]  [minor range]  [type]
       * serial  /dev/ttyS    4        64-95          serial
       */

      at = strchr(at, ' ');    // skip first token
      if (!at)
         goto finish;          // bail out on truncation
      while (*at == ' ') at++; // skip spaces

      const char* token = at;  // mark beginning of path
      at = strchr(at, ' ');    // find end of path
      if (!at)
         goto finish;          // bail out on truncation
      *at = '\0'; at++;        // clear and skip
      path = xStrdup(token);   // save
      while (*at == ' ') at++; // skip spaces

      token = at;              // mark beginning of major
      at = strchr(at, ' ');    // find end of major
      if (!at)
         goto finish;          // bail out on truncation
      *at = '\0'; at++;        // clear and skip
      ttyDrivers[numDrivers].major = atoi(token); // save
      while (*at == ' ') at++; // skip spaces

      token = at;              // mark beginning of minorFrom
      while (*at >= '0' && *at <= '9') at++; //find end of minorFrom
      if (*at == '-') {        // if has range
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorFrom = atoi(token); // save
         token = at;              // mark beginning of minorTo
         at = strchr(at, ' ');    // find end of minorTo
         if (!at)
            goto finish;          // bail out on truncation
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      } else {                 // no range
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorFrom = atoi(token); // save
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      }

      at = strchr(at, '\n');   // go to end of line
      if (at)
         at++;                 // skip
      ttyDrivers[numDrivers].path = path;
      path = NULL;
      numDrivers++;
      if (numDrivers == allocd) {
         allocd += 10;
         ttyDrivers = xReallocArray(ttyDrivers, allocd, sizeof(TtyDriver));
      }
   }
finish:
   free(path);

   ttyDrivers = xRealloc(ttyDrivers, sizeof(TtyDriver) * (numDrivers + 1));
   ttyDrivers[numDrivers].path = NULL;
   qsort(ttyDrivers, numDrivers, sizeof(TtyDriver), sortTtyDrivers);
   this->ttyDrivers = ttyDrivers;
}

ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   LinuxProcessTable* this = xCalloc(1, sizeof(LinuxProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(LinuxProcess), host, pidMatchList);

   LinuxProcessTable_initTtyDrivers(this);

   // Test /proc/PID/smaps_rollup availability (faster to parse, Linux 4.14+)
   this->haveSmapsRollup = (access(PROCDIR "/self/smaps_rollup", R_OK) == 0);

   // Read PID namespace inode number
   {
      struct stat sb;
      int r = stat(PROCDIR "/self/ns/pid", &sb);
      if (r == 0) {
         rootPidNs = sb.st_ino;
      } else {
         rootPidNs = (ino_t)-1;
      }
   }

   return super;
}

void ProcessTable_delete(Object* cast) {
   LinuxProcessTable* this = (LinuxProcessTable*) cast;
   ProcessTable_done(&this->super);
   if (this->ttyDrivers) {
      for (int i = 0; this->ttyDrivers[i].path; i++) {
         free(this->ttyDrivers[i].path);
      }
      free(this->ttyDrivers);
   }
   #ifdef HAVE_DELAYACCT
   LibNl_destroyNetlinkSocket(this);
   #endif
   free(this);
}

static inline unsigned long long LinuxProcessTable_adjustTime(const LinuxMachine* lhost, unsigned long long t) {
   return t * 100 / lhost->jiffies;
}

/* Taken from: https://github.com/torvalds/linux/blob/64570fbc14f8d7cb3fe3995f20e26bc25ce4b2cc/fs/proc/array.c#L120 */
static inline ProcessState LinuxProcessTable_getProcessState(char state) {
   switch (state) {
      case 'S': return SLEEPING;
      case 'X': return DEFUNCT;
      case 'Z': return ZOMBIE;
      case 't': return TRACED;
      case 'T': return STOPPED;
      case 'D': return UNINTERRUPTIBLE_WAIT;
      case 'R': return RUNNING;
      case 'P': return BLOCKED;
      case 'I': return IDLE;
      default: return UNKNOWN;
   }
}

/*
 * Read /proc/<pid>/stat (thread-specific data)
 */
static bool LinuxProcessTable_readStatFile(LinuxProcess* lp, openat_arg_t procFd, const LinuxMachine* lhost, bool scanMainThread, char* command, size_t commLen) {
   Process* process = &lp->super;

   char buf[MAX_READ + 1];
   char path[22] = "stat";
   if (scanMainThread) {
      xSnprintf(path, sizeof(path), "task/%"PRIi32"/stat", (int32_t)Process_getPid(process));
   }
   ssize_t r = Compat_readfileat(procFd, path, buf, sizeof(buf));
   if (r < 0)
      return false;

   /* (1) pid   -  %d */
   assert(Process_getPid(process) == atoi(buf));
   char* location = strchr(buf, ' ');
   if (!location)
      return false;

   /* (2) comm  -  (%s) */
   if (!location[0] || !location[1])
      return false;

   location += 2;
   char* end = strrchr(location, ')');
   if (!end)
      return false;

   if (end < location)
      return false;

   String_safeStrncpy(command, location, MINIMUM((size_t)(end - location + 1), commLen));

   if (!end[0] || !end[1])
      return false;

   location = end + 2;

   /* (3) state  -  %c */
   process->state = LinuxProcessTable_getProcessState(location[0]);

   if (!location[0] || !location[1])
      return false;

   location += 2;

   /* (4) ppid  -  %d */
   Process_setParent(process, fast_strtol_dec(&location, 0));

   if (!location[0])
      return false;

   location += 1;

   /* (5) pgrp  -  %d */
   process->pgrp = fast_strtol_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (6) session  -  %d */
   process->session = fast_strtol_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (7) tty_nr  -  %d */
   process->tty_nr = fast_strtoul_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (8) tpgid  -  %d */
   process->tpgid = fast_strtol_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (9) flags  -  %u */
   lp->flags = fast_strtoul_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (10) minflt  -  %lu */
   process->minflt = fast_strtoull_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (11) cminflt  -  %lu */
   lp->cminflt = fast_strtoull_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (12) majflt  -  %lu */
   process->majflt = fast_strtoull_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (13) cmajflt  -  %lu */
   lp->cmajflt = fast_strtoull_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (14) utime  -  %lu */
   lp->utime = LinuxProcessTable_adjustTime(lhost, fast_strtoull_dec(&location, 0));

   if (!location[0])
      return false;

   location += 1;

   /* (15) stime  -  %lu */
   lp->stime = LinuxProcessTable_adjustTime(lhost, fast_strtoull_dec(&location, 0));

   if (!location[0])
      return false;

   location += 1;

   /* (16) cutime  -  %ld */
   lp->cutime = LinuxProcessTable_adjustTime(lhost, fast_strtoull_dec(&location, 0));

   if (!location[0])
      return false;

   location += 1;

   /* (17) cstime  -  %ld */
   lp->cstime = LinuxProcessTable_adjustTime(lhost, fast_strtoull_dec(&location, 0));

   if (!location[0])
      return false;

   location += 1;

   /* (18) priority  -  %ld */
   process->priority = fast_strtol_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (19) nice  -  %ld */
   process->nice = fast_strtol_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* (20) num_threads  -  %ld */
   process->nlwp = fast_strtol_dec(&location, 0);

   if (!location[0])
      return false;

   location += 1;

   /* Skip (21) itrealvalue  -  %ld */
   location = strchr(location, ' ');

   if (!location)
      return false;

   location += 1;

   /* (22) starttime  -  %llu */
   if (process->starttime_ctime == 0) {
      process->starttime_ctime = lhost->boottime + LinuxProcessTable_adjustTime(lhost, fast_strtoll_dec(&location, 0)) / 100;
   } else {
      location = strchr(location, ' ');
      if (!location)
         return false;
   }
   location += 1;

   /* Skip (23) - (38) */
   for (int i = 0; i < 16; i++) {
      location = strchr(location, ' ');

      if (!location)
         return false;

      location += 1;
   }

   assert(location != NULL);

   /* (39) processor  -  %d */
   process->processor = fast_strtol_dec(&location, 0);

   /* Ignore further fields */

   process->time = lp->utime + lp->stime;

   return true;
}

/*
 * Read /proc/<pid>/status (thread-specific data)
 */
static bool LinuxProcessTable_readStatusFile(Process* process, openat_arg_t procFd) {
   LinuxProcess* lp = (LinuxProcess*) process;

   unsigned long ctxt = 0;
   process->isRunningInContainer = TRI_OFF;
#ifdef HAVE_VSERVER
   lp->vxid = 0;
#endif

   FILE* statusfile = fopenat(procFd, "status", "r");
   if (!statusfile)
      return false;

   char buffer[PROC_LINE_LENGTH + 1] = {0};

   while (fgets(buffer, sizeof(buffer), statusfile)) {

      if (String_startsWith(buffer, "NSpid:")) {
         const char* ptr = buffer;
         int pid_ns_count = 0;
         while (*ptr && *ptr != '\n' && !isdigit((unsigned char)*ptr))
            ++ptr;

         while (*ptr && *ptr != '\n') {
            if (isdigit(*ptr))
               pid_ns_count++;
            while (isdigit((unsigned char)*ptr))
               ++ptr;
            while (*ptr && *ptr != '\n' && !isdigit((unsigned char)*ptr))
               ++ptr;
         }

         if (pid_ns_count > 1)
            process->isRunningInContainer = TRI_ON;

      } else if (String_startsWith(buffer, "voluntary_ctxt_switches:")) {
         unsigned long vctxt;
         int ok = sscanf(buffer, "voluntary_ctxt_switches:\t%lu", &vctxt);
         if (ok == 1) {
            ctxt += vctxt;
         }

      } else if (String_startsWith(buffer, "nonvoluntary_ctxt_switches:")) {
         unsigned long nvctxt;
         int ok = sscanf(buffer, "nonvoluntary_ctxt_switches:\t%lu", &nvctxt);
         if (ok == 1) {
            ctxt += nvctxt;
         }

#ifdef HAVE_VSERVER
      } else if (String_startsWith(buffer, "VxID:")) {
         int vxid;
         int ok = sscanf(buffer, "VxID:\t%32d", &vxid);
         if (ok == 1) {
            lp->vxid = vxid;
         }
#ifdef HAVE_ANCIENT_VSERVER
      } else if (String_startsWith(buffer, "s_context:")) {
         int vxid;
         int ok = sscanf(buffer, "s_context:\t%32d", &vxid);
         if (ok == 1) {
            lp->vxid = vxid;
         }
#endif /* HAVE_ANCIENT_VSERVER */
#endif /* HAVE_VSERVER */
      }
   }

   fclose(statusfile);

   lp->ctxt_diff = (ctxt > lp->ctxt_total) ? (ctxt - lp->ctxt_total) : 0;
   lp->ctxt_total = ctxt;

   return true;
}

/*
 * Gather user of task (process-shared data)
 */
static bool LinuxProcessTable_updateUser(const Machine* host, Process* process, openat_arg_t procFd, const LinuxProcess* mainTask) {
   if (mainTask) {
      process->st_uid = mainTask->super.st_uid;
      process->user = mainTask->super.user;
      return true;
   }

   struct stat sb;
#ifdef HAVE_OPENAT
   int statok = fstat(procFd, &sb);
#else
   int statok = stat(procFd, &sb);
#endif
   if (statok == -1)
      return false;

   if (process->st_uid != sb.st_uid) {
      process->st_uid = sb.st_uid;
      process->user = UsersTable_getRef(host->usersTable, sb.st_uid);
   }

   return true;
}

/*
 * Read /proc/<pid>/io (thread-specific data)
 */
static void LinuxProcessTable_readIoFile(LinuxProcess* lp, openat_arg_t procFd, bool scanMainThread) {
   Process* process = &lp->super;
   const Machine* host = process->super.host;
   char path[20] = "io";
   char buffer[1024];
   if (scanMainThread) {
      xSnprintf(path, sizeof(path), "task/%"PRIi32"/io", (int32_t)Process_getPid(process));
   }
   ssize_t r = Compat_readfileat(procFd, path, buffer, sizeof(buffer));
   if (r < 0) {
      lp->io_rate_read_bps = NAN;
      lp->io_rate_write_bps = NAN;
      lp->io_rchar = ULLONG_MAX;
      lp->io_wchar = ULLONG_MAX;
      lp->io_syscr = ULLONG_MAX;
      lp->io_syscw = ULLONG_MAX;
      lp->io_read_bytes = ULLONG_MAX;
      lp->io_write_bytes = ULLONG_MAX;
      lp->io_cancelled_write_bytes = ULLONG_MAX;
      lp->io_last_scan_time_ms = host->realtimeMs;
      return;
   }

   unsigned long long last_read = lp->io_read_bytes;
   unsigned long long last_write = lp->io_write_bytes;
   unsigned long long time_delta = saturatingSub(host->realtimeMs, lp->io_last_scan_time_ms);

   // Note: Linux Kernel documentation states that /proc/<pid>/io may be racy
   // on 32-bit machines. (Documentation/filesystems/proc.rst)

   char* buf = buffer;
   const char* line;
   while ((line = strsep(&buf, "\n")) != NULL) {
      switch (line[0]) {
         case 'r':
            if (line[1] == 'c' && String_startsWith(line + 2, "har: ")) {
               lp->io_rchar = strtoull(line + 7, NULL, 10);
            } else if (String_startsWith(line + 1, "ead_bytes: ")) {
               lp->io_read_bytes = strtoull(line + 12, NULL, 10);
               lp->io_rate_read_bps = time_delta ? saturatingSub(lp->io_read_bytes, last_read) * /*ms to s*/1000. / time_delta : NAN;
            }
            break;
         case 'w':
            if (line[1] == 'c' && String_startsWith(line + 2, "har: ")) {
               lp->io_wchar = strtoull(line + 7, NULL, 10);
            } else if (String_startsWith(line + 1, "rite_bytes: ")) {
               lp->io_write_bytes = strtoull(line + 13, NULL, 10);
               lp->io_rate_write_bps = time_delta ? saturatingSub(lp->io_write_bytes, last_write) * /*ms to s*/1000. / time_delta : NAN;
            }
            break;
         case 's':
            if (line[4] == 'r' && String_startsWith(line + 1, "yscr: ")) {
               lp->io_syscr = strtoull(line + 7, NULL, 10);
            } else if (String_startsWith(line + 1, "yscw: ")) {
               lp->io_syscw = strtoull(line + 7, NULL, 10);
            }
            break;
         case 'c':
            if (String_startsWith(line + 1, "ancelled_write_bytes: ")) {
               lp->io_cancelled_write_bytes = strtoull(line + 23, NULL, 10);
            }
      }
   }

   lp->io_last_scan_time_ms = host->realtimeMs;
}

typedef struct LibraryData_ {
   uint64_t size;
   bool exec;
} LibraryData;

static void LinuxProcessTable_calcLibSize_helper(ATTR_UNUSED ht_key_t key, void* value, void* data) {
   if (!data)
      return;

   if (!value)
      return;

   const LibraryData* v = (const LibraryData*)value;
   uint64_t* d = (uint64_t*)data;
   if (!v->exec)
      return;

   *d += v->size;
}

/*
 * Read /proc/<pid>/maps (process-shared data)
 */
static void LinuxProcessTable_readMaps(LinuxProcess* process, openat_arg_t procFd, const LinuxMachine* host, bool calcSize, bool checkDeletedLib) {
   Process* proc = (Process*)process;

   proc->usesDeletedLib = false;

   FILE* mapsfile = fopenat(procFd, "maps", "r");
   if (!mapsfile)
      return;

   Hashtable* ht = NULL;
   if (calcSize)
      ht = Hashtable_new(64, true);

   char buffer[1024];
   while (fgets(buffer, sizeof(buffer), mapsfile)) {
      uint64_t map_start;
      uint64_t map_end;
      bool map_execute;
      unsigned int map_devmaj;
      unsigned int map_devmin;
      uint64_t map_inode;

      // Short circuit test: Look for a slash
      if (!strchr(buffer, '/'))
         continue;

      // Parse format: "%Lx-%Lx %4s %x %2x:%2x %Ld"
      char* readptr = buffer;

      map_start = fast_strtoull_hex(&readptr, 16);
      if ('-' != *readptr++)
         continue;

      map_end = fast_strtoull_hex(&readptr, 16);
      if (' ' != *readptr++)
         continue;

      if (!readptr[0] || !readptr[1] || !readptr[2] || !readptr[3])
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

      map_inode = fast_strtoull_dec(&readptr, 0);
      if (!map_inode)
         continue;

      if (calcSize) {
         LibraryData* libdata = Hashtable_get(ht, map_inode);
         if (!libdata) {
            libdata = xCalloc(1, sizeof(LibraryData));
            Hashtable_put(ht, map_inode, libdata);
         }

         libdata->size += map_end - map_start;
         libdata->exec |= map_execute;
      }

      if (checkDeletedLib && map_execute && !proc->usesDeletedLib) {
         while (*readptr == ' ')
            readptr++;

         if (*readptr != '/')
            continue;

         if (String_startsWith(readptr, "/memfd:"))
            continue;

         /* Virtualbox maps /dev/zero for memory allocation. That results in
          * false positive, so ignore. */
         if (String_eq(readptr, "/dev/zero (deleted)\n"))
            continue;

         if (strstr(readptr, " (deleted)\n")) {
            proc->usesDeletedLib = true;
            if (!calcSize)
               break;
         }
      }
   }

   fclose(mapsfile);

   if (calcSize) {
      uint64_t total_size = 0;
      Hashtable_foreach(ht, LinuxProcessTable_calcLibSize_helper, &total_size);

      Hashtable_delete(ht);

      process->m_lrs = total_size / host->pageSize;
   }
}

/*
 * Read /proc/<pid>/statm (process-shared data)
 */
static bool LinuxProcessTable_readStatmFile(LinuxProcess* process, openat_arg_t procFd, const LinuxMachine* host, const LinuxProcess* mainTask) {
   if (mainTask) {
      process->super.m_virt     = mainTask->super.m_virt;
      process->super.m_resident = mainTask->super.m_resident;
      return true;
   }

   char statmdata[128] = {0};

   if (Compat_readfileat(procFd, "statm", statmdata, sizeof(statmdata)) < 1) {
      return false;
   }

   long int dummy, dummy2;

   int r = sscanf(statmdata, "%ld %ld %ld %ld %ld %ld %ld",
                  &process->super.m_virt,
                  &process->super.m_resident,
                  &process->m_share,
                  &process->m_trs,
                  &dummy, /* unused since Linux 2.6; always 0 */
                  &process->m_drs,
                  &dummy2); /* unused since Linux 2.6; always 0 */

   if (r == 7) {
      process->super.m_virt *= host->pageSizeKB;
      process->super.m_resident *= host->pageSizeKB;

      process->m_priv = process->super.m_resident - (process->m_share * host->pageSizeKB);
   }

   return r == 7;
}

/*
 * Read /proc/<pid>/smaps (process-shared data)
 */
static bool LinuxProcessTable_readSmapsFile(LinuxProcess* process, openat_arg_t procFd, bool haveSmapsRollup) {
   //http://elixir.free-electrons.com/linux/v4.10/source/fs/proc/task_mmu.c#L719
   //kernel will return data in chunks of size PAGE_SIZE or less.
   FILE* fp = fopenat(procFd, haveSmapsRollup ? "smaps_rollup" : "smaps", "r");
   if (!fp)
      return false;

   process->m_pss   = 0;
   process->m_swap  = 0;
   process->m_psswp = 0;

   char buffer[256];
   while (fgets(buffer, sizeof(buffer), fp)) {
      if (!strchr(buffer, '\n')) {
         // Partial line, skip to end of this line
         if (!skipEndOfLine(fp)) {
            fclose(fp);
            return false;
         }

      }

      if (String_startsWith(buffer, "Pss:")) {
         process->m_pss += strtol(buffer + 4, NULL, 10);
      } else if (String_startsWith(buffer, "Swap:")) {
         process->m_swap += strtol(buffer + 5, NULL, 10);
      } else if (String_startsWith(buffer, "SwapPss:")) {
         process->m_psswp += strtol(buffer + 8, NULL, 10);
      }
   }

   fclose(fp);
   return true;
}

#ifdef HAVE_OPENVZ

static void LinuxProcessTable_readOpenVZData(LinuxProcess* process, openat_arg_t procFd) {
   if (access(PROCDIR "/vz", R_OK) != 0) {
      free(process->ctid);
      process->ctid = NULL;
      process->vpid = Process_getPid(&process->super);
      return;
   }

   FILE* file = fopenat(procFd, "status", "r");
   if (!file) {
      free(process->ctid);
      process->ctid = NULL;
      process->vpid = Process_getPid(&process->super);
      return;
   }

   bool foundEnvID = false;
   bool foundVPid = false;
   char linebuf[256];
   while (fgets(linebuf, sizeof(linebuf), file) != NULL) {
      if (strchr(linebuf, '\n') == NULL) {
         // Partial line, skip to end of this line
         if (!skipEndOfLine(file)) {
            break;
         }
      }

      char* name_value_sep = strchr(linebuf, ':');
      if (name_value_sep == NULL) {
         continue;
      }

      int field;
      if (0 == strncasecmp(linebuf, "envID", name_value_sep - linebuf)) {
         field = 1;
      } else if (0 == strncasecmp(linebuf, "VPid", name_value_sep - linebuf)) {
         field = 2;
      } else {
         continue;
      }

      do {
         name_value_sep++;
      } while (*name_value_sep != '\0' && *name_value_sep <= 32);

      char* value_end = name_value_sep;

      while (*value_end > 32) {
         value_end++;
      }

      if (name_value_sep == value_end) {
         continue;
      }

      *value_end = '\0';

      switch (field) {
         case 1:
            foundEnvID = true;
            if (!String_eq(name_value_sep, process->ctid ? process->ctid : ""))
               free_and_xStrdup(&process->ctid, name_value_sep);
            break;
         case 2:
            foundVPid = true;
            process->vpid = strtoul(name_value_sep, NULL, 0);
            break;
         default:
            //Sanity Check: Should never reach here, or the implementation is missing something!
            assert(false && "OpenVZ handling: Unimplemented case for field handling reached.");
      }
   }

   fclose(file);

   if (!foundEnvID) {
      free(process->ctid);
      process->ctid = NULL;
   }

   if (!foundVPid) {
      process->vpid = Process_getPid(&process->super);
   }
}

#endif /* HAVE_OPENVZ */

/*
 * Read /proc/<pid>/cgroup (thread-specific data)
 */
static void LinuxProcessTable_readCGroupFile(LinuxProcess* process, openat_arg_t procFd) {
   FILE* file = fopenat(procFd, "cgroup", "r");
   if (!file) {
      if (process->cgroup) {
         free(process->cgroup);
         process->cgroup = NULL;
      }
      if (process->cgroup_short) {
         free(process->cgroup_short);
         process->cgroup_short = NULL;
      }
      if (process->container_short) {
         free(process->container_short);
         process->container_short = NULL;
      }
      return;
   }
   char output[PROC_LINE_LENGTH + 1];
   output[0] = '\0';
   char* at = output;
   int left = PROC_LINE_LENGTH;
   while (!feof(file) && left > 0) {
      char buffer[PROC_LINE_LENGTH + 1];
      const char* ok = fgets(buffer, PROC_LINE_LENGTH, file);
      if (!ok)
         break;

      char* group = buffer;
      for (size_t i = 0; i < 2; i++) {
         group = String_strchrnul(group, ':');
         if (!*group)
            break;
         group++;
      }

      char* eol = String_strchrnul(group, '\n');
      *eol = '\0';

      if (at != output) {
         *at = ';';
         at++;
         left--;
      }
      int wrote = snprintf(at, left, "%s", group);
      left -= wrote;
   }
   fclose(file);

   bool changed = !process->cgroup || !String_eq(process->cgroup, output);

   Row_updateFieldWidth(CGROUP, strlen(output));
   free_and_xStrdup(&process->cgroup, output);

   if (!changed) {
      if (process->cgroup_short) {
         Row_updateFieldWidth(CCGROUP, strlen(process->cgroup_short));
      } else {
         //CCGROUP is alias to normal CGROUP if shortening fails
         Row_updateFieldWidth(CCGROUP, strlen(process->cgroup));
      }
      if (process->container_short) {
         Row_updateFieldWidth(CONTAINER, strlen(process->container_short));
      } else {
         Row_updateFieldWidth(CONTAINER, strlen("N/A"));
      }
      return;
   }

   char* cgroup_short = CGroup_filterName(process->cgroup);
   if (cgroup_short) {
      Row_updateFieldWidth(CCGROUP, strlen(cgroup_short));
      free_and_xStrdup(&process->cgroup_short, cgroup_short);
      free(cgroup_short);
   } else {
      //CCGROUP is alias to normal CGROUP if shortening fails
      Row_updateFieldWidth(CCGROUP, strlen(process->cgroup));
      free(process->cgroup_short);
      process->cgroup_short = NULL;
   }

   char* container_short = CGroup_filterContainer(process->cgroup);
   if (container_short) {
      Row_updateFieldWidth(CONTAINER, strlen(container_short));
      free_and_xStrdup(&process->container_short, container_short);
      free(container_short);
   } else {
      //CONTAINER is just "N/A" if shortening fails
      Row_updateFieldWidth(CONTAINER, strlen("N/A"));
      free(process->container_short);
      process->container_short = NULL;
   }
}

/*
 * Read /proc/<pid>/oom_score (process-shared data)
 */
static void LinuxProcessTable_readOomData(LinuxProcess* process, openat_arg_t procFd, const LinuxProcess* mainTask) {
   if (mainTask) {
      process->oom = mainTask->oom;
      return;
   }

   char buffer[PROC_LINE_LENGTH + 1] = {0};

   ssize_t oomRead = Compat_readfileat(procFd, "oom_score", buffer, sizeof(buffer));
   if (oomRead < 1) {
      return;
   }

   char* oomPtr = buffer;
   uint64_t oom = fast_strtoull_dec(&oomPtr, oomRead);
   if (*oomPtr && *oomPtr != '\n' && *oomPtr != ' ') {
      return;
   }

   if (oom > UINT_MAX) {
      return;
   }

   process->oom = oom;
}

/*
 * Read /proc/<pid>/autogroup (process-shared data)
 */
static void LinuxProcessTable_readAutogroup(LinuxProcess* process, openat_arg_t procFd, const LinuxProcess* mainTask) {
   if (mainTask) {
      process->autogroup_id = mainTask->autogroup_id;
      return;
   }

   process->autogroup_id = -1;

   char autogroup[64]; // space for two numeric values and fixed length strings
   ssize_t amtRead = Compat_readfileat(procFd, "autogroup", autogroup, sizeof(autogroup));
   if (amtRead < 0)
      return;

   long int identity;
   int nice;
   int ok = sscanf(autogroup, "/autogroup-%ld nice %d", &identity, &nice);
   if (ok == 2) {
      process->autogroup_id = identity;
      process->autogroup_nice = nice;
   }
}

/*
 * Read /proc/<pid>/attr/current (process-shared data)
 */
static void LinuxProcessTable_readSecattrData(LinuxProcess* process, openat_arg_t procFd, const LinuxProcess* mainTask) {
   if (mainTask) {
      const char* mainSecAttr = mainTask->secattr;
      if (mainSecAttr) {
         free_and_xStrdup(&process->secattr, mainSecAttr);
      } else {
         free(process->secattr);
         process->secattr = NULL;
      }
      return;
   }

   char buffer[PROC_LINE_LENGTH + 1] = {0};

   ssize_t attrdata = Compat_readfileat(procFd, "attr/current", buffer, sizeof(buffer));
   if (attrdata < 1) {
      free(process->secattr);
      process->secattr = NULL;
      return;
   }

   char* newline = strchr(buffer, '\n');
   if (newline) {
      *newline = '\0';
   }

   Row_updateFieldWidth(SECATTR, strlen(buffer));

   free_and_xStrdup(&process->secattr, buffer);
}

/*
 * Read /proc/<pid>/cwd (process-shared data)
 */
static void LinuxProcessTable_readCwd(LinuxProcess* process, openat_arg_t procFd, const LinuxProcess* mainTask) {
   if (mainTask) {
      const char* mainCwd = mainTask->super.procCwd;
      if (mainCwd) {
         free_and_xStrdup(&process->super.procCwd, mainCwd);
      } else {
         free(process->super.procCwd);
         process->super.procCwd = NULL;
      }
      return;
   }

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

   free_and_xStrdup(&process->super.procCwd, pathBuffer);
}

/*
 * Read /proc/<pid>/exe (process-shared data)
 */
static void LinuxProcessList_readExe(Process* process, openat_arg_t procFd, const LinuxProcess* mainTask) {
   if (mainTask) {
      Process_updateExe(process, mainTask->super.procExe);
      process->procExeDeleted = mainTask->super.procExeDeleted;
      return;
   }

   char filename[PATH_MAX + 1];

#if defined(HAVE_READLINKAT) && defined(HAVE_OPENAT)
   ssize_t amtRead = readlinkat(procFd, "exe", filename, sizeof(filename) - 1);
#else
   ssize_t amtRead = Compat_readlink(procFd, "exe", filename, sizeof(filename) - 1);
#endif
   if (amtRead > 0) {
      filename[amtRead] = 0;
      if (!process->procExe ||
          (!process->procExeDeleted && !String_eq(filename, process->procExe)) ||
          process->procExeDeleted) {

         const char* deletedMarker = " (deleted)";
         const size_t markerLen = strlen(deletedMarker);
         const size_t filenameLen = strlen(filename);

         if (filenameLen > markerLen) {
            bool oldExeDeleted = process->procExeDeleted;

            process->procExeDeleted = String_eq(filename + filenameLen - markerLen, deletedMarker);

            if (process->procExeDeleted)
               filename[filenameLen - markerLen] = '\0';

            if (oldExeDeleted != process->procExeDeleted)
               process->mergedCommand.lastUpdate = 0;
         }

         Process_updateExe(process, filename);
      }
   } else if (process->procExe) {
      Process_updateExe(process, NULL);
      process->procExeDeleted = false;
   }
}

/*
 * Read /proc/<pid>/cmdline (process-shared data)
 */
static bool LinuxProcessTable_readCmdlineFile(Process* process, openat_arg_t procFd, const LinuxProcess* mainTask) {
   LinuxProcessList_readExe(process, procFd, mainTask);

   char command[4096 + 1]; // max cmdline length on Linux
   ssize_t amtRead = Compat_readfileat(procFd, "cmdline", command, sizeof(command));
   if (amtRead <= 0)
      return false;

   size_t tokenEnd = (size_t)-1;
   size_t tokenStart = (size_t)-1;
   size_t lastChar = 0;
   bool argSepNUL = false;
   bool argSepSpace = false;

   for (size_t i = 0; i < (size_t)amtRead; i++) {
      // If this is true, there's a NUL byte in the middle of command
      if (tokenEnd != (size_t)-1) {
         argSepNUL = true;
      }

      const char argChar = command[i];

      /* newline used as delimiter - when forming the mergedCommand, newline is
       * converted to space by Process_makeCommandStr */
      if (argChar == '\n') {
         /* Set to some other non-printable character */
         command[i] = '\r';
         continue;
      }

      if (argChar == '\0') {
         command[i] = '\n';

         // Set tokenEnd to the NUL byte
         if (tokenEnd == (size_t)-1) {
            tokenEnd = i;
         }

         continue;
      }

      /* Record some information for the argument parsing heuristic below. */
      if (argChar <= ' ') {
         argSepSpace = true;
      }

      /* Detect the last / before the end of the token as
       * the start of the basename in cmdline, see Process_writeCommand */
      if (argChar == '/' && tokenEnd == (size_t)-1) {
         tokenStart = i + 1;
      }

      lastChar = i;
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

      tokenStart = (size_t)-1;
      tokenEnd = (size_t)-1;

      size_t exeLen = process->procExe ? strlen(process->procExe) : 0;

      if (process->procExe && String_startsWith(command, process->procExe) &&
         exeLen < lastChar && command[exeLen] <= ' ') {
         tokenStart = process->procExeBasenameOffset;
         tokenEnd = exeLen;
      }

      // From initial scan we know there's at least one space.
      // Check if that's part of a filename for an existing file.
      else if (Compat_faccessat(AT_FDCWD, command, F_OK, AT_SYMLINK_NOFOLLOW) != 0) {
         // If we reach here the path does not exist.
         // Thus begin searching for the part of it that actually does.
         size_t tokenArg0Start = (size_t)-1;

         for (size_t i = 0; i <= lastChar; i++) {
            const char cmdChar = command[i];

            /* Any ASCII control or space used as delimiter */
            if (cmdChar <= ' ') {
               if (tokenEnd != (size_t)-1) {
                  // Split on every further separator, regardless of path correctness
                  command[i] = '\n';
                  continue;
               }

               // Found our first argument
               command[i] = '\0';

               bool found = Compat_faccessat(AT_FDCWD, command, F_OK, AT_SYMLINK_NOFOLLOW) == 0;

               // Restore if this wasn't it
               command[i] = found ? '\n' : cmdChar;

               if (found)
                  tokenEnd = i;
               if (tokenArg0Start == (size_t)-1)
                  tokenArg0Start = tokenStart == (size_t)-1 ? 0 : tokenStart;

               continue;
            }

            if (tokenEnd != (size_t)-1) {
               continue;
            }

            if (cmdChar == '/') {
               // Normal path separator
               tokenStart = i + 1;
            } else if (cmdChar == '\\' && (tokenStart == (size_t)-1 || tokenStart == 0 || command[tokenStart - 1] == '\\')) {
               // Windows Path separator (WINE)
               tokenStart = i + 1;
            } else if (cmdChar == ':' && (command[i + 1] != '/' && command[i + 1] != '\\')) {
               // Colon not part of a Windows Path
               tokenEnd = i;
            } else if (tokenStart == (size_t)-1) {
               // Relative path
               tokenStart = i;
            }
         }

         if (tokenEnd == (size_t)-1) {
            tokenStart = tokenArg0Start;

            // No token delimiter found, forcibly split
            for (size_t i = 0; i <= lastChar; i++) {
               if (command[i] <= ' ') {
                  command[i] = '\n';
                  if (tokenEnd == (size_t)-1) {
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
      if (tokenStart >= tokenEnd) {
         tokenStart = (size_t)-1;
         tokenEnd = (size_t)-1;
      }
   }

   if (tokenStart == (size_t)-1) {
      tokenStart = 0;
   }

   if (tokenEnd == (size_t)-1) {
      tokenEnd = lastChar + 1;
   }

   Process_updateCmdline(process, command, tokenStart, tokenEnd);

   return true;
}

/*
 * Read /proc/<pid>/comm (thread-specific data)
 */
static void LinuxProcessList_readComm(Process* process, openat_arg_t procFd) {
   char command[4096 + 1]; // max cmdline length on Linux
   ssize_t amtRead = Compat_readfileat(procFd, "comm", command, sizeof(command));
   if (amtRead > 0) {
      command[amtRead - 1] = '\0';
      Process_updateComm(process, command);
   } else {
      Process_updateComm(process, NULL);
   }
}

static char* LinuxProcessTable_updateTtyDevice(TtyDriver* ttyDrivers, unsigned long int tty_nr) {
   unsigned int maj = major(tty_nr);
   unsigned int min = minor(tty_nr);

   int i = -1;
   for (;;) {
      i++;
      if ((!ttyDrivers[i].path) || maj < ttyDrivers[i].major) {
         break;
      }
      if (maj > ttyDrivers[i].major) {
         continue;
      }
      if (min < ttyDrivers[i].minorFrom) {
         break;
      }
      if (min > ttyDrivers[i].minorTo) {
         continue;
      }

      unsigned int idx = min - ttyDrivers[i].minorFrom;

      struct stat sb;

      for (;;) {
         char* fullPath = NULL;
         size_t fullPathLen = xAsprintf(&fullPath, "%s/%d", ttyDrivers[i].path, idx);
         int err = stat(fullPath, &sb);
         if (err == 0 && major(sb.st_rdev) == maj && minor(sb.st_rdev) == min) {
            return fullPath;
         }

         xSnprintf(fullPath, fullPathLen + 1, "%s%d", ttyDrivers[i].path, idx);
         err = stat(fullPath, &sb);
         if (err == 0 && major(sb.st_rdev) == maj && minor(sb.st_rdev) == min) {
            return fullPath;
         }
         free(fullPath);

         if (idx == min) {
            break;
         }

         idx = min;
      }

      int err = stat(ttyDrivers[i].path, &sb);
      if (err == 0 && tty_nr == sb.st_rdev) {
         return xStrdup(ttyDrivers[i].path);
      }
   }

   char* out = NULL;
   xAsprintf(&out, "/dev/%u:%u", maj, min);
   return out;
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

static bool LinuxProcessTable_recurseProcTree(LinuxProcessTable* this, openat_arg_t parentFd, const LinuxMachine* lhost, const char* dirname, const LinuxProcess* mainTask) {
   ProcessTable* pt = (ProcessTable*) this;
   const Machine* host = &lhost->super;
   const Settings* settings = host->settings;
   const ScreenSettings* ss = settings->ss;
   const struct dirent* entry;

   /* set runningTasks from /proc/stat (from Machine_scanCPUTime) */
   pt->runningTasks = lhost->runningTasks;

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
   const bool hideRunningInContainer = settings->hideRunningInContainer;
   while ((entry = readdir(dir)) != NULL) {
      const char* name = entry->d_name;

      // Ignore all non-directories
      if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
         continue;
      }

      // The RedHat kernel hides threads with a dot.
      // I believe this is non-standard.
      if (name[0] == '.') {
         name++;
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
      if (mainTask && pid == Process_getPid(&mainTask->super))
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
      Process* proc = ProcessTable_getProcess(pt, pid, &preExisting, LinuxProcess_new);
      LinuxProcess* lp = (LinuxProcess*) proc;

      Process_setThreadGroup(proc, mainTask ? Process_getPid(&mainTask->super) : pid);
      proc->isUserlandThread = Process_getPid(proc) != Process_getThreadGroup(proc);
      assert(proc->isUserlandThread == (mainTask != NULL));

      if (!mainTask) {
         // As the list of tasks/threads is presented as a flat view in procfs
         // below each directories main entry, it makes no sense to
         // look for further directories that will not be there.
         LinuxProcessTable_recurseProcTree(this, procFd, lhost, "task", lp);
      }

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
      if (preExisting && hideRunningInContainer && proc->isRunningInContainer == TRI_ON) {
         proc->super.updated = true;
         proc->super.show = false;
         Compat_openatArgClose(procFd);
         continue;
      }

      const bool scanMainThread = !hideUserlandThreads && !Process_isKernelThread(proc) && !mainTask;

      if (!LinuxProcessTable_readStatmFile(lp, procFd, lhost, mainTask))
         goto errorReadingProcess;

      {
         bool prev = proc->usesDeletedLib;

         if (!proc->isKernelThread && !proc->isUserlandThread &&
             ((ss->flags & PROCESS_FLAG_LINUX_LRS_FIX) || (settings->highlightDeletedExe && !proc->procExeDeleted && isOlderThan(proc, 10)))) {

            // Check if we really should recalculate the M_LRS value for this process
            uint64_t passedTimeInMs = host->realtimeMs - lp->last_mlrs_calctime;

            uint64_t recheck = ((uint64_t)rand()) % 2048;

            if (passedTimeInMs > recheck) {
               lp->last_mlrs_calctime = host->realtimeMs;
               LinuxProcessTable_readMaps(lp, procFd, lhost, ss->flags & PROCESS_FLAG_LINUX_LRS_FIX, settings->highlightDeletedExe);
            }
         } else {
            /* Copy from process structure in threads and reset if setting got disabled */
            proc->usesDeletedLib = (proc->isUserlandThread && mainTask) ? mainTask->super.usesDeletedLib : false;
            lp->m_lrs = (proc->isUserlandThread && mainTask) ? mainTask->m_lrs : 0;
         }

         if (prev != proc->usesDeletedLib)
            proc->mergedCommand.lastUpdate = 0;
      }

      char statCommand[MAX_NAME + 1];
      unsigned long long int lasttimes = (lp->utime + lp->stime);
      unsigned long int last_tty_nr = proc->tty_nr;
      if (!LinuxProcessTable_readStatFile(lp, procFd, lhost, scanMainThread, statCommand, sizeof(statCommand)))
         goto errorReadingProcess;

      if (lp->flags & PF_KTHREAD) {
         proc->isKernelThread = true;
      }

      if (last_tty_nr != proc->tty_nr && this->ttyDrivers) {
         free(proc->tty_name);
         proc->tty_name = LinuxProcessTable_updateTtyDevice(this->ttyDrivers, proc->tty_nr);
      }

      proc->percent_cpu = NAN;
      /* lhost->period might be 0 after system sleep */
      if (lhost->period > 0.0) {
         float percent_cpu = saturatingSub(lp->utime + lp->stime, lasttimes) / lhost->period * 100.0;
         proc->percent_cpu = MINIMUM(percent_cpu, host->activeCPUs * 100.0F);
      }
      proc->percent_mem = proc->m_resident / (double)(lhost->totalMem) * 100.0;
      Process_updateCPUFieldWidths(proc->percent_cpu);

      if (!LinuxProcessTable_updateUser(host, proc, procFd, mainTask))
         goto errorReadingProcess;

      /* Check if the process is inside a different PID namespace. */
      if (proc->isRunningInContainer == TRI_INITIAL && rootPidNs != (ino_t)-1) {
         struct stat sb;
#if defined(HAVE_OPENAT) && defined(HAVE_FSTATAT)
         int res = fstatat(procFd, "ns/pid", &sb, 0);
#else
         char path[PATH_MAX];
         xSnprintf(path, sizeof(path), "%s/ns/pid", procFd);
         int res = stat(path, &sb);
#endif
         if (res == 0) {
            proc->isRunningInContainer = (sb.st_ino != rootPidNs) ? TRI_ON : TRI_OFF;
         }
      }

      if (ss->flags & PROCESS_FLAG_LINUX_CTXT
         || ((hideRunningInContainer || ss->flags & PROCESS_FLAG_LINUX_CONTAINER) && proc->isRunningInContainer == TRI_INITIAL)
#ifdef HAVE_VSERVER
         || ss->flags & PROCESS_FLAG_LINUX_VSERVER
#endif
      ) {
         proc->isRunningInContainer = TRI_OFF;
         if (!LinuxProcessTable_readStatusFile(proc, procFd))
            goto errorReadingProcess;
      }

      if (!preExisting) {

         #ifdef HAVE_OPENVZ
         if (ss->flags & PROCESS_FLAG_LINUX_OPENVZ) {
            LinuxProcessTable_readOpenVZData(lp, procFd);
         }
         #endif

         if (proc->isKernelThread) {
            Process_updateCmdline(proc, NULL, 0, 0);
         } else {
            if (!LinuxProcessTable_readCmdlineFile(proc, procFd, mainTask)) {
               Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
            }
            LinuxProcessList_readComm(proc, procFd);
         }

         Process_fillStarttimeBuffer(proc);

         ProcessTable_add(pt, proc);
      } else {
         if (settings->updateProcessNames && proc->state != ZOMBIE) {
            if (proc->isKernelThread) {
               Process_updateCmdline(proc, NULL, 0, 0);
            } else {
               if (!LinuxProcessTable_readCmdlineFile(proc, procFd, mainTask)) {
                  Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
               }
               LinuxProcessList_readComm(proc, procFd);
            }
         }
      }

      /*
       * Section gathering non-critical information that is independent from
       * each other.
       */

      /* Gather permitted capabilities (thread-specific data) for non-root process. */
      if (proc->st_uid != 0 && proc->elevated_priv != TRI_OFF) {
         struct __user_cap_header_struct header = { .version = _LINUX_CAPABILITY_VERSION_3, .pid = Process_getPid(proc) };
         struct __user_cap_data_struct data;

         long res = syscall(SYS_capget, &header, &data);
         if (res == 0) {
            proc->elevated_priv = (data.permitted != 0) ? TRI_ON : TRI_OFF;
         } else {
            proc->elevated_priv = TRI_OFF;
         }
      }

      if (ss->flags & PROCESS_FLAG_LINUX_CGROUP)
         LinuxProcessTable_readCGroupFile(lp, procFd);

      if ((ss->flags & PROCESS_FLAG_LINUX_SMAPS) && !Process_isKernelThread(proc)) {
         if (!mainTask) {
            // Read smaps file of each process only every second pass to improve performance
            static int smaps_flag = 0;
            if ((pid & 1) == smaps_flag) {
               LinuxProcessTable_readSmapsFile(lp, procFd, this->haveSmapsRollup);
            }
            if (pid == 1) {
               smaps_flag = !smaps_flag;
            }
         } else {
            lp->m_pss   = mainTask->m_pss;
            lp->m_swap  = mainTask->m_swap;
            lp->m_psswp = mainTask->m_psswp;
         }
      }

      if (ss->flags & PROCESS_FLAG_IO) {
         LinuxProcessTable_readIoFile(lp, procFd, scanMainThread);
      }

      #ifdef HAVE_DELAYACCT
      if (ss->flags & PROCESS_FLAG_LINUX_DELAYACCT) {
         LibNl_readDelayAcctData(this, lp);
      }
      #endif

      if (ss->flags & PROCESS_FLAG_LINUX_OOM) {
         LinuxProcessTable_readOomData(lp, procFd, mainTask);
      }

      if (ss->flags & PROCESS_FLAG_LINUX_IOPRIO) {
         LinuxProcess_updateIOPriority(proc);
      }

      if (ss->flags & PROCESS_FLAG_LINUX_SECATTR) {
         LinuxProcessTable_readSecattrData(lp, procFd, mainTask);
      }

      if (ss->flags & PROCESS_FLAG_CWD) {
         LinuxProcessTable_readCwd(lp, procFd, mainTask);
      }

      if ((ss->flags & PROCESS_FLAG_LINUX_AUTOGROUP) && this->haveAutogroup) {
         LinuxProcessTable_readAutogroup(lp, procFd, mainTask);
      }

      #ifdef SCHEDULER_SUPPORT
      if (ss->flags & PROCESS_FLAG_SCHEDPOL) {
         Scheduling_readProcessPolicy(proc);
      }
      #endif

      if (ss->flags & PROCESS_FLAG_LINUX_GPU || GPUMeter_active()) {
         if (mainTask) {
            lp->gpu_time = mainTask->gpu_time;
         } else {
            GPU_readProcessData(this, lp, procFd);
         }
      }

      /*
       * Final section after all data has been gathered
       */

      if (!proc->cmdline && statCommand[0] &&
          (proc->state == ZOMBIE || Process_isKernelThread(proc) || settings->showThreadNames)) {
         Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
      }

      proc->super.updated = true;
      Compat_openatArgClose(procFd);

      if (hideRunningInContainer && proc->isRunningInContainer == TRI_ON) {
         proc->super.show = false;
         continue;
      }

      if (Process_isKernelThread(proc)) {
         pt->kernelThreads++;
      } else if (Process_isUserlandThread(proc)) {
         pt->userlandThreads++;
      }

      /* Set at the end when we know if a new entry is a thread */
      proc->super.show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      pt->totalTasks++;
      /* runningTasks is set in Machine_scanCPUTime() from /proc/stat */
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
             * The only real reason for coming here (apart from Linux violating the /proc API)
             * would be the process going away with its /proc files disappearing (!HAVE_OPENAT).
             * However, we want to keep in the process list for now for the "highlight dying" mode.
             */
         } else {
            /* A really short-lived process that we don't have full info about */
            assert(ProcessTable_findProcess(pt, Process_getPid(proc)) == NULL);
            Process_delete((Object*)proc);
         }
      }
   }
   closedir(dir);
   return true;
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   LinuxProcessTable* this = (LinuxProcessTable*) super;
   Machine* host = super->super.host;
   const Settings* settings = host->settings;
   LinuxMachine* lhost = (LinuxMachine*) host;

   if (settings->ss->flags & PROCESS_FLAG_LINUX_AUTOGROUP) {
      // Refer to sched(7) 'autogroup feature' section
      // The kernel feature can be enabled/disabled through procfs at
      // any time, so check for it at the start of each sample - only
      // read from per-process procfs files if it's globally enabled.
      this->haveAutogroup = LinuxProcess_isAutogroupEnabled();
   } else {
      this->haveAutogroup = false;
   }

   /* Shift GPU values */
   {
      lhost->prevGpuTime = lhost->curGpuTime;
      lhost->curGpuTime = 0;

      for (GPUEngineData* engine = lhost->gpuEngineData; engine; engine = engine->next) {
         engine->prevTime = engine->curTime;
         engine->curTime = 0;
      }
   }

   /* PROCDIR is an absolute path */
   assert(PROCDIR[0] == '/');
#ifdef HAVE_OPENAT
   openat_arg_t rootFd = AT_FDCWD;
#else
   openat_arg_t rootFd = "";
#endif

   LinuxProcessTable_recurseProcTree(this, rootFd, lhost, PROCDIR, NULL);
}
