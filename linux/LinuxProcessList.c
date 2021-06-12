/*
htop - LinuxProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/LinuxProcessList.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef HAVE_DELAYACCT
#include <linux/netlink.h>
#include <linux/taskstats.h>
#include <netlink/attr.h>
#include <netlink/handlers.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#endif

#include "Compat.h"
#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "Settings.h"
#include "XUtils.h"
#include "linux/LinuxProcess.h"
#include "linux/Platform.h" // needed for GNU/hurd to get PATH_MAX

#if defined(MAJOR_IN_MKDEV)
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS)
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_SENSORS_SENSORS_H
#include "LibSensors.h"
#endif


static long long btime = -1;

static long jiffy;

static FILE* fopenat(openat_arg_t openatArg, const char* pathname, const char* mode) {
   assert(String_eq(mode, "r")); /* only currently supported mode */

   int fd = Compat_openat(openatArg, pathname, O_RDONLY);
   if (fd < 0)
      return NULL;

   FILE* stream = fdopen(fd, mode);
   if (!stream)
      close(fd);

   return stream;
}

static int sortTtyDrivers(const void* va, const void* vb) {
   const TtyDriver* a = (const TtyDriver*) va;
   const TtyDriver* b = (const TtyDriver*) vb;

   int r = SPACESHIP_NUMBER(a->major, b->major);
   if (r)
      return r;

   return SPACESHIP_NUMBER(a->minorFrom, b->minorFrom);
}

static void LinuxProcessList_initTtyDrivers(LinuxProcessList* this) {
   TtyDriver* ttyDrivers;

   char buf[16384];
   ssize_t r = xReadfile(PROCTTYDRIVERSFILE, buf, sizeof(buf));
   if (r < 0)
      return;

   int numDrivers = 0;
   int allocd = 10;
   ttyDrivers = xMallocArray(allocd, sizeof(TtyDriver));
   char* at = buf;
   while (*at != '\0') {
      at = strchr(at, ' ');    // skip first token
      while (*at == ' ') at++; // skip spaces
      const char* token = at;  // mark beginning of path
      at = strchr(at, ' ');    // find end of path
      *at = '\0'; at++;        // clear and skip
      ttyDrivers[numDrivers].path = xStrdup(token); // save
      while (*at == ' ') at++; // skip spaces
      token = at;              // mark beginning of major
      at = strchr(at, ' ');    // find end of major
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
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      } else {                 // no range
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorFrom = atoi(token); // save
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      }
      at = strchr(at, '\n');   // go to end of line
      at++;                    // skip
      numDrivers++;
      if (numDrivers == allocd) {
         allocd += 10;
         ttyDrivers = xReallocArray(ttyDrivers, allocd, sizeof(TtyDriver));
      }
   }
   numDrivers++;
   ttyDrivers = xRealloc(ttyDrivers, sizeof(TtyDriver) * numDrivers);
   ttyDrivers[numDrivers - 1].path = NULL;
   qsort(ttyDrivers, numDrivers - 1, sizeof(TtyDriver), sortTtyDrivers);
   this->ttyDrivers = ttyDrivers;
}

#ifdef HAVE_DELAYACCT

static void LinuxProcessList_initNetlinkSocket(LinuxProcessList* this) {
   this->netlink_socket = nl_socket_alloc();
   if (this->netlink_socket == NULL) {
      return;
   }
   if (nl_connect(this->netlink_socket, NETLINK_GENERIC) < 0) {
      return;
   }
   this->netlink_family = genl_ctrl_resolve(this->netlink_socket, TASKSTATS_GENL_NAME);
}

#endif

static void LinuxProcessList_updateCPUcount(ProcessList* super, FILE* stream) {
   LinuxProcessList* this = (LinuxProcessList*) super;

   unsigned int cpus = 0;
   char buffer[PROC_LINE_LENGTH + 1];
   while (fgets(buffer, sizeof(buffer), stream)) {
      if (String_startsWith(buffer, "cpu")) {
         cpus++;
      }
   }

   if (cpus == 0)
      CRT_fatalError("No cpu entry in " PROCSTATFILE);
   if (cpus == 1)
      CRT_fatalError("No cpu aggregate or cpuN entry in " PROCSTATFILE);

   /* Subtract aggregate cpu entry */
   cpus--;

   if (cpus != super->cpuCount || !this->cpus) {
      super->cpuCount = MAXIMUM(cpus, 1);
      free(this->cpus);
      this->cpus = xCalloc(cpus + 1, sizeof(CPUData));
   }
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   LinuxProcessList* this = xCalloc(1, sizeof(LinuxProcessList));
   ProcessList* pl = &(this->super);

   ProcessList_init(pl, Class(LinuxProcess), usersTable, pidMatchList, userId);
   LinuxProcessList_initTtyDrivers(this);

   // Initialize page size
   pageSize = sysconf(_SC_PAGESIZE);
   if (pageSize == -1)
      CRT_fatalError("Cannot get pagesize by sysconf(_SC_PAGESIZE)");
   pageSizeKB = pageSize / ONE_K;

   // Initialize clock ticks
   jiffy = sysconf(_SC_CLK_TCK);
   if (jiffy == -1)
      CRT_fatalError("Cannot get clock ticks by sysconf(_SC_CLK_TCK)");

   // Test /proc/PID/smaps_rollup availability (faster to parse, Linux 4.14+)
   this->haveSmapsRollup = (access(PROCDIR "/self/smaps_rollup", R_OK) == 0);

   // Read btime (the kernel boot time, as number of seconds since the epoch)
   FILE* statfile = fopen(PROCSTATFILE, "r");
   if (statfile == NULL)
      CRT_fatalError("Cannot open " PROCSTATFILE);
   while (true) {
      char buffer[PROC_LINE_LENGTH + 1];
      if (fgets(buffer, sizeof(buffer), statfile) == NULL)
         break;
      if (String_startsWith(buffer, "btime ") == false)
         continue;
      if (sscanf(buffer, "btime %lld\n", &btime) == 1)
         break;
      CRT_fatalError("Failed to parse btime from " PROCSTATFILE);
   }

   if (btime == -1)
      CRT_fatalError("No btime in " PROCSTATFILE);

   rewind(statfile);

   // Initialize CPU count
   LinuxProcessList_updateCPUcount(pl, statfile);

   fclose(statfile);

   return pl;
}

void ProcessList_delete(ProcessList* pl) {
   LinuxProcessList* this = (LinuxProcessList*) pl;
   ProcessList_done(pl);
   free(this->cpus);
   if (this->ttyDrivers) {
      for (int i = 0; this->ttyDrivers[i].path; i++) {
         free(this->ttyDrivers[i].path);
      }
      free(this->ttyDrivers);
   }
   #ifdef HAVE_DELAYACCT
   if (this->netlink_socket) {
      nl_close(this->netlink_socket);
      nl_socket_free(this->netlink_socket);
   }
   #endif
   free(this);
}

static inline unsigned long long LinuxProcessList_adjustTime(unsigned long long t) {
   return t * 100 / jiffy;
}

static bool LinuxProcessList_readStatFile(Process* process, openat_arg_t procFd, char* command, size_t commLen) {
   LinuxProcess* lp = (LinuxProcess*) process;

   char buf[MAX_READ + 1];
   ssize_t r = xReadfileat(procFd, "stat", buf, sizeof(buf));
   if (r < 0)
      return false;

   /* (1) pid   -  %d */
   assert(process->pid == atoi(buf));
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
   process->state = location[0];
   location += 2;

   /* (4) ppid  -  %d */
   process->ppid = strtol(location, &location, 10);
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

   /* Skip (9) flags  -  %u */
   location = strchr(location, ' ') + 1;

   /* (10) minflt  -  %lu */
   process->minflt = strtoull(location, &location, 10);
   location += 1;

   /* (11) cminflt  -  %lu */
   lp->cminflt = strtoull(location, &location, 10);
   location += 1;

   /* (12) majflt  -  %lu */
   process->majflt = strtoull(location, &location, 10);
   location += 1;

   /* (13) cmajflt  -  %lu */
   lp->cmajflt = strtoull(location, &location, 10);
   location += 1;

   /* (14) utime  -  %lu */
   lp->utime = LinuxProcessList_adjustTime(strtoull(location, &location, 10));
   location += 1;

   /* (15) stime  -  %lu */
   lp->stime = LinuxProcessList_adjustTime(strtoull(location, &location, 10));
   location += 1;

   /* (16) cutime  -  %ld */
   lp->cutime = LinuxProcessList_adjustTime(strtoull(location, &location, 10));
   location += 1;

   /* (17) cstime  -  %ld */
   lp->cstime = LinuxProcessList_adjustTime(strtoull(location, &location, 10));
   location += 1;

   /* (18) priority  -  %ld */
   process->priority = strtol(location, &location, 10);
   location += 1;

   /* (19) nice  -  %ld */
   process->nice = strtol(location, &location, 10);
   location += 1;

   /* (20) num_threads  -  %ld */
   process->nlwp = strtol(location, &location, 10);
   location += 1;

   /* Skip (21) itrealvalue  -  %ld */
   location = strchr(location, ' ') + 1;

   /* (22) starttime  -  %llu */
   if (process->starttime_ctime == 0) {
      process->starttime_ctime = btime + LinuxProcessList_adjustTime(strtoll(location, &location, 10)) / 100;
   } else {
      location = strchr(location, ' ');
   }
   location += 1;

   /* Skip (23) - (38) */
   for (int i = 0; i < 16; i++) {
      location = strchr(location, ' ') + 1;
   }

   assert(location != NULL);

   /* (39) processor  -  %d */
   process->processor = strtol(location, &location, 10);

   /* Ignore further fields */

   process->time = lp->utime + lp->stime;

   return true;
}


static bool LinuxProcessList_updateUser(ProcessList* processList, Process* process, openat_arg_t procFd) {
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
      process->user = UsersTable_getRef(processList->usersTable, sstat.st_uid);
   }

   return true;
}

static void LinuxProcessList_readIoFile(LinuxProcess* process, openat_arg_t procFd, unsigned long long realtimeMs) {
   char buffer[1024];
   ssize_t r = xReadfileat(procFd, "io", buffer, sizeof(buffer));
   if (r < 0) {
      process->io_rate_read_bps = NAN;
      process->io_rate_write_bps = NAN;
      process->io_rchar = ULLONG_MAX;
      process->io_wchar = ULLONG_MAX;
      process->io_syscr = ULLONG_MAX;
      process->io_syscw = ULLONG_MAX;
      process->io_read_bytes = ULLONG_MAX;
      process->io_write_bytes = ULLONG_MAX;
      process->io_cancelled_write_bytes = ULLONG_MAX;
      process->io_last_scan_time_ms = realtimeMs;
      return;
   }

   unsigned long long last_read = process->io_read_bytes;
   unsigned long long last_write = process->io_write_bytes;
   char* buf = buffer;
   const char* line;
   while ((line = strsep(&buf, "\n")) != NULL) {
      switch (line[0]) {
      case 'r':
         if (line[1] == 'c' && String_startsWith(line + 2, "har: ")) {
            process->io_rchar = strtoull(line + 7, NULL, 10);
         } else if (String_startsWith(line + 1, "ead_bytes: ")) {
            process->io_read_bytes = strtoull(line + 12, NULL, 10);
            process->io_rate_read_bps = (process->io_read_bytes - last_read) * /*ms to s*/1000 / (realtimeMs - process->io_last_scan_time_ms);
         }
         break;
      case 'w':
         if (line[1] == 'c' && String_startsWith(line + 2, "har: ")) {
            process->io_wchar = strtoull(line + 7, NULL, 10);
         } else if (String_startsWith(line + 1, "rite_bytes: ")) {
            process->io_write_bytes = strtoull(line + 13, NULL, 10);
            process->io_rate_write_bps = (process->io_write_bytes - last_write) * /*ms to s*/1000 / (realtimeMs - process->io_last_scan_time_ms);
         }
         break;
      case 's':
         if (line[4] == 'r' && String_startsWith(line + 1, "yscr: ")) {
            process->io_syscr = strtoull(line + 7, NULL, 10);
         } else if (String_startsWith(line + 1, "yscw: ")) {
            process->io_syscw = strtoull(line + 7, NULL, 10);
         }
         break;
      case 'c':
         if (String_startsWith(line + 1, "ancelled_write_bytes: ")) {
            process->io_cancelled_write_bytes = strtoull(line + 23, NULL, 10);
         }
      }
   }

   process->io_last_scan_time_ms = realtimeMs;
}

typedef struct LibraryData_ {
    uint64_t size;
    bool exec;
} LibraryData;

static inline uint64_t fast_strtoull_dec(char **str, int maxlen) {
   register uint64_t result = 0;

   if (!maxlen)
      --maxlen;

   while (maxlen-- && **str >= '0' && **str <= '9') {
      result *= 10;
      result += **str - '0';
      (*str)++;
   }

   return result;
}

static inline uint64_t fast_strtoull_hex(char **str, int maxlen) {
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

static void LinuxProcessList_calcLibSize_helper(ATTR_UNUSED ht_key_t key, void* value, void* data) {
   if (!data)
      return;

   if (!value)
      return;

   const LibraryData* v = (const LibraryData *)value;
   uint64_t* d = (uint64_t *)data;
   if (!v->exec)
      return;

   *d += v->size;
}

static void LinuxProcessList_readMaps(LinuxProcess* process, openat_arg_t procFd, bool calcSize, bool checkDeletedLib) {
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
      char *readptr = buffer;

      map_start = fast_strtoull_hex(&readptr, 16);
      if ('-' != *readptr++)
         continue;

      map_end = fast_strtoull_hex(&readptr, 16);
      if (' ' != *readptr++)
         continue;

      map_execute = (readptr[2] == 'x');
      readptr += 4;
      if (' ' != *readptr++)
         continue;

      while(*readptr > ' ')
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
      Hashtable_foreach(ht, LinuxProcessList_calcLibSize_helper, &total_size);

      Hashtable_delete(ht);

      process->m_lrs = total_size / pageSize;
   }
}

static bool LinuxProcessList_readStatmFile(LinuxProcess* process, openat_arg_t procFd) {
   FILE* statmfile = fopenat(procFd, "statm", "r");
   if (!statmfile)
      return false;

   long int dummy;

   int r = fscanf(statmfile, "%ld %ld %ld %ld %ld %ld %ld",
                  &process->super.m_virt,
                  &process->super.m_resident,
                  &process->m_share,
                  &process->m_trs,
                  &dummy, /* unused since Linux 2.6; always 0 */
                  &process->m_drs,
                  &process->m_dt);
   fclose(statmfile);

   if (r == 7) {
      process->super.m_virt *= pageSizeKB;
      process->super.m_resident *= pageSizeKB;
   }

   return r == 7;
}

static bool LinuxProcessList_readSmapsFile(LinuxProcess* process, openat_arg_t procFd, bool haveSmapsRollup) {
   //http://elixir.free-electrons.com/linux/v4.10/source/fs/proc/task_mmu.c#L719
   //kernel will return data in chunks of size PAGE_SIZE or less.
   FILE* f = fopenat(procFd, haveSmapsRollup ? "smaps_rollup" : "smaps", "r");
   if (!f)
      return false;

   process->m_pss   = 0;
   process->m_swap  = 0;
   process->m_psswp = 0;

   char buffer[256];
   while (fgets(buffer, sizeof(buffer), f)) {
      if (!strchr(buffer, '\n')) {
         // Partial line, skip to end of this line
         while (fgets(buffer, sizeof(buffer), f)) {
            if (strchr(buffer, '\n')) {
               break;
            }
         }
         continue;
      }

      if (String_startsWith(buffer, "Pss:")) {
         process->m_pss += strtol(buffer + 4, NULL, 10);
      } else if (String_startsWith(buffer, "Swap:")) {
         process->m_swap += strtol(buffer + 5, NULL, 10);
      } else if (String_startsWith(buffer, "SwapPss:")) {
         process->m_psswp += strtol(buffer + 8, NULL, 10);
      }
   }

   fclose(f);
   return true;
}

#ifdef HAVE_OPENVZ

static void LinuxProcessList_readOpenVZData(LinuxProcess* process, openat_arg_t procFd) {
   if (access(PROCDIR "/vz", R_OK) != 0) {
      free(process->ctid);
      process->ctid = NULL;
      process->vpid = process->super.pid;
      return;
   }

   FILE* file = fopenat(procFd, "status", "r");
   if (!file) {
      free(process->ctid);
      process->ctid = NULL;
      process->vpid = process->super.pid;
      return;
   }

   bool foundEnvID = false;
   bool foundVPid = false;
   char linebuf[256];
   while (fgets(linebuf, sizeof(linebuf), file) != NULL) {
      if (strchr(linebuf, '\n') == NULL) {
         // Partial line, skip to end of this line
         while (fgets(linebuf, sizeof(linebuf), file) != NULL) {
            if (strchr(linebuf, '\n') != NULL) {
               break;
            }
         }
         continue;
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

      while(*value_end > 32) {
         value_end++;
      }

      if (name_value_sep == value_end) {
         continue;
      }

      *value_end = '\0';

      switch(field) {
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
      process->vpid = process->super.pid;
   }
}

#endif

static void LinuxProcessList_readCGroupFile(LinuxProcess* process, openat_arg_t procFd) {
   FILE* file = fopenat(procFd, "cgroup", "r");
   if (!file) {
      if (process->cgroup) {
         free(process->cgroup);
         process->cgroup = NULL;
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

      char* group = strchr(buffer, ':');
      if (!group)
         break;

      if (at != output) {
         *at = ';';
         at++;
         left--;
      }
      int wrote = snprintf(at, left, "%s", group);
      left -= wrote;
   }
   fclose(file);
   free_and_xStrdup(&process->cgroup, output);
}

#ifdef HAVE_VSERVER

static void LinuxProcessList_readVServerData(LinuxProcess* process, openat_arg_t procFd) {
   FILE* file = fopenat(procFd, "status", "r");
   if (!file)
      return;

   char buffer[PROC_LINE_LENGTH + 1];
   process->vxid = 0;
   while (fgets(buffer, PROC_LINE_LENGTH, file)) {
      if (String_startsWith(buffer, "VxID:")) {
         int vxid;
         int ok = sscanf(buffer, "VxID:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #if defined HAVE_ANCIENT_VSERVER
      else if (String_startsWith(buffer, "s_context:")) {
         int vxid;
         int ok = sscanf(buffer, "s_context:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #endif
   }
   fclose(file);
}

#endif

static void LinuxProcessList_readOomData(LinuxProcess* process, openat_arg_t procFd) {
   FILE* file = fopenat(procFd, "oom_score", "r");
   if (!file)
      return;

   char buffer[PROC_LINE_LENGTH + 1];
   if (fgets(buffer, PROC_LINE_LENGTH, file)) {
      unsigned int oom;
      int ok = sscanf(buffer, "%u", &oom);
      if (ok >= 1) {
         process->oom = oom;
      }
   }
   fclose(file);
}

static void LinuxProcessList_readCtxtData(LinuxProcess* process, openat_arg_t procFd) {
   FILE* file = fopenat(procFd, "status", "r");
   if (!file)
      return;

   char buffer[PROC_LINE_LENGTH + 1];
   unsigned long ctxt = 0;
   while (fgets(buffer, PROC_LINE_LENGTH, file)) {
      if (String_startsWith(buffer, "voluntary_ctxt_switches:")) {
         unsigned long vctxt;
         int ok = sscanf(buffer, "voluntary_ctxt_switches:\t%lu", &vctxt);
         if (ok >= 1) {
            ctxt += vctxt;
         }
      } else if (String_startsWith(buffer, "nonvoluntary_ctxt_switches:")) {
         unsigned long nvctxt;
         int ok = sscanf(buffer, "nonvoluntary_ctxt_switches:\t%lu", &nvctxt);
         if (ok >= 1) {
            ctxt += nvctxt;
         }
      }
   }
   fclose(file);
   process->ctxt_diff = (ctxt > process->ctxt_total) ? (ctxt - process->ctxt_total) : 0;
   process->ctxt_total = ctxt;
}

static void LinuxProcessList_readSecattrData(LinuxProcess* process, openat_arg_t procFd) {
   FILE* file = fopenat(procFd, "attr/current", "r");
   if (!file) {
      free(process->secattr);
      process->secattr = NULL;
      return;
   }

   char buffer[PROC_LINE_LENGTH + 1];
   const char* res = fgets(buffer, sizeof(buffer), file);
   fclose(file);
   if (!res) {
      free(process->secattr);
      process->secattr = NULL;
      return;
   }
   char* newline = strchr(buffer, '\n');
   if (newline) {
      *newline = '\0';
   }
   if (process->secattr && String_eq(process->secattr, buffer)) {
      return;
   }
   free_and_xStrdup(&process->secattr, buffer);
}

static void LinuxProcessList_readCwd(LinuxProcess* process, openat_arg_t procFd) {
   char pathBuffer[PATH_MAX + 1] = {0};

#if defined(HAVE_READLINKAT) && defined(HAVE_OPENAT)
   ssize_t r = readlinkat(procFd, "cwd", pathBuffer, sizeof(pathBuffer) - 1);
#else
   char filename[MAX_NAME + 1];
   xSnprintf(filename, sizeof(filename), "%s/cwd", procFd);
   ssize_t r = readlink(filename, pathBuffer, sizeof(pathBuffer) - 1);
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

#ifdef HAVE_DELAYACCT

static int handleNetlinkMsg(struct nl_msg* nlmsg, void* linuxProcess) {
   struct nlmsghdr* nlhdr;
   struct nlattr* nlattrs[TASKSTATS_TYPE_MAX + 1];
   const struct nlattr* nlattr;
   struct taskstats stats;
   int rem;
   LinuxProcess* lp = (LinuxProcess*) linuxProcess;

   nlhdr = nlmsg_hdr(nlmsg);

   if (genlmsg_parse(nlhdr, 0, nlattrs, TASKSTATS_TYPE_MAX, NULL) < 0) {
      return NL_SKIP;
   }

   if ((nlattr = nlattrs[TASKSTATS_TYPE_AGGR_PID]) || (nlattr = nlattrs[TASKSTATS_TYPE_NULL])) {
      memcpy(&stats, nla_data(nla_next(nla_data(nlattr), &rem)), sizeof(stats));
      assert(lp->super.pid == (pid_t)stats.ac_pid);

      unsigned long long int timeDelta = stats.ac_etime * 1000 - lp->delay_read_time;
      #define BOUNDS(x) (isnan(x) ? 0.0 : ((x) > 100) ? 100.0 : (x))
      #define DELTAPERC(x,y) BOUNDS((float) ((x) - (y)) / timeDelta * 100)
      lp->cpu_delay_percent = DELTAPERC(stats.cpu_delay_total, lp->cpu_delay_total);
      lp->blkio_delay_percent = DELTAPERC(stats.blkio_delay_total, lp->blkio_delay_total);
      lp->swapin_delay_percent = DELTAPERC(stats.swapin_delay_total, lp->swapin_delay_total);
      #undef DELTAPERC
      #undef BOUNDS

      lp->swapin_delay_total = stats.swapin_delay_total;
      lp->blkio_delay_total = stats.blkio_delay_total;
      lp->cpu_delay_total = stats.cpu_delay_total;
      lp->delay_read_time = stats.ac_etime * 1000;
   }
   return NL_OK;
}

static void LinuxProcessList_readDelayAcctData(LinuxProcessList* this, LinuxProcess* process) {
   struct nl_msg* msg;

   if (!this->netlink_socket) {
       LinuxProcessList_initNetlinkSocket(this);
       if (!this->netlink_socket) {
          goto delayacct_failure;
       }
   }

   if (nl_socket_modify_cb(this->netlink_socket, NL_CB_VALID, NL_CB_CUSTOM, handleNetlinkMsg, process) < 0) {
      goto delayacct_failure;
   }

   if (! (msg = nlmsg_alloc())) {
      goto delayacct_failure;
   }

   if (! genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, this->netlink_family, 0, NLM_F_REQUEST, TASKSTATS_CMD_GET, TASKSTATS_VERSION)) {
      nlmsg_free(msg);
   }

   if (nla_put_u32(msg, TASKSTATS_CMD_ATTR_PID, process->super.pid) < 0) {
      nlmsg_free(msg);
   }

   if (nl_send_sync(this->netlink_socket, msg) < 0) {
      goto delayacct_failure;
   }

   if (nl_recvmsgs_default(this->netlink_socket) < 0) {
      goto delayacct_failure;
   }

   return;

delayacct_failure:
   process->swapin_delay_percent = NAN;
   process->blkio_delay_percent = NAN;
   process->cpu_delay_percent = NAN;
}

#endif

static bool LinuxProcessList_readCmdlineFile(Process* process, openat_arg_t procFd) {
   char command[4096 + 1]; // max cmdline length on Linux
   ssize_t amtRead = xReadfileat(procFd, "cmdline", command, sizeof(command));
   if (amtRead < 0)
      return false;

   if (amtRead == 0) {
      if (process->state != 'Z') {
         process->isKernelThread = true;
      }
      Process_updateCmdline(process, NULL, 0, 0);
      return true;
   }

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

   /* /proc/[pid]/comm could change, so should be updated */
   if ((amtRead = xReadfileat(procFd, "comm", command, sizeof(command))) > 0) {
      command[amtRead - 1] = '\0';
      Process_updateComm(process, command);
   } else if (process->procComm) {
      Process_updateComm(process, NULL);
   }

   char filename[MAX_NAME + 1];

   /* execve could change /proc/[pid]/exe, so procExe should be updated */
#if defined(HAVE_READLINKAT) && defined(HAVE_OPENAT)
   amtRead = readlinkat(procFd, "exe", filename, sizeof(filename) - 1);
#else
   char path[4096];
   xSnprintf(path, sizeof(path), "%s/exe", procFd);
   amtRead = readlink(path, filename, sizeof(filename) - 1);
#endif
   if (amtRead > 0) {
      filename[amtRead] = 0;
      if (!process->procExe ||
         (!process->procExeDeleted && !String_eq(filename, process->procExe)) ||
         (process->procExeDeleted && !String_startsWith(filename, process->procExe))) {

         const char* deletedMarker = " (deleted)";
         const size_t markerLen = strlen(deletedMarker);
         const size_t filenameLen = strlen(filename);

         if (filenameLen > markerLen) {
            bool oldExeDeleted = process->procExeDeleted;

            process->procExeDeleted = String_eq(filename + filenameLen - markerLen, deletedMarker);

            if (process->procExeDeleted)
               filename[filenameLen - markerLen] = '\0';

            process->mergedCommand.exeChanged |= oldExeDeleted ^ process->procExeDeleted;
         }

         Process_updateExe(process, filename);
      }
   } else if (process->procExe) {
      Process_updateExe(process, NULL);
      process->procExeDeleted = false;
   }

   return true;
}

static char* LinuxProcessList_updateTtyDevice(TtyDriver* ttyDrivers, unsigned long int tty_nr) {
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
      struct stat sstat;
      char* fullPath;
      for (;;) {
         xAsprintf(&fullPath, "%s/%d", ttyDrivers[i].path, idx);
         int err = stat(fullPath, &sstat);
         if (err == 0 && major(sstat.st_rdev) == maj && minor(sstat.st_rdev) == min) {
            return fullPath;
         }
         free(fullPath);

         xAsprintf(&fullPath, "%s%d", ttyDrivers[i].path, idx);
         err = stat(fullPath, &sstat);
         if (err == 0 && major(sstat.st_rdev) == maj && minor(sstat.st_rdev) == min) {
            return fullPath;
         }
         free(fullPath);

         if (idx == min) {
            break;
         }

         idx = min;
      }
      int err = stat(ttyDrivers[i].path, &sstat);
      if (err == 0 && tty_nr == sstat.st_rdev) {
         return xStrdup(ttyDrivers[i].path);
      }
   }
   char* out;
   xAsprintf(&out, "/dev/%u:%u", maj, min);
   return out;
}

static bool LinuxProcessList_recurseProcTree(LinuxProcessList* this, openat_arg_t parentFd, const char* dirname, const Process* parent, double period) {
   ProcessList* pl = (ProcessList*) this;
   const struct dirent* entry;
   const Settings* settings = pl->settings;

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

   unsigned int cpus = pl->cpuCount;
   bool hideKernelThreads = settings->hideKernelThreads;
   bool hideUserlandThreads = settings->hideUserlandThreads;
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
      if (parent && pid == parent->pid)
         continue;

      bool preExisting;
      Process* proc = ProcessList_getProcess(pl, pid, &preExisting, LinuxProcess_new);
      LinuxProcess* lp = (LinuxProcess*) proc;

      proc->tgid = parent ? parent->pid : pid;
      proc->isUserlandThread = proc->pid != proc->tgid;

#ifdef HAVE_OPENAT
      int procFd = openat(dirFd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
      if (procFd < 0)
         goto errorReadingProcess;
#else
      char procFd[4096];
      xSnprintf(procFd, sizeof(procFd), "%s/%s", dirFd, entry->d_name);
#endif

      LinuxProcessList_recurseProcTree(this, procFd, "task", proc, period);

      /*
       * These conditions will not trigger on first occurrence, cause we need to
       * add the process to the ProcessList and do all one time scans
       * (e.g. parsing the cmdline to detect a kernel thread)
       * But it will short-circuit subsequent scans.
       */
      if (preExisting && hideKernelThreads && Process_isKernelThread(proc)) {
         proc->updated = true;
         proc->show = false;
         pl->kernelThreads++;
         pl->totalTasks++;
         Compat_openatArgClose(procFd);
         continue;
      }
      if (preExisting && hideUserlandThreads && Process_isUserlandThread(proc)) {
         proc->updated = true;
         proc->show = false;
         pl->userlandThreads++;
         pl->totalTasks++;
         Compat_openatArgClose(procFd);
         continue;
      }

      if (settings->flags & PROCESS_FLAG_IO)
         LinuxProcessList_readIoFile(lp, procFd, pl->realtimeMs);

      if (!LinuxProcessList_readStatmFile(lp, procFd))
         goto errorReadingProcess;

      {
         bool prev = proc->usesDeletedLib;

         if ((lp->m_lrs == 0 && (settings->flags & PROCESS_FLAG_LINUX_LRS_FIX)) ||
             (settings->highlightDeletedExe && !proc->procExeDeleted && !proc->isKernelThread && !proc->isUserlandThread)) {
            // Check if we really should recalculate the M_LRS value for this process
            uint64_t passedTimeInMs = pl->realtimeMs - lp->last_mlrs_calctime;

            uint64_t recheck = ((uint64_t)rand()) % 2048;

            if (passedTimeInMs > 2000 || passedTimeInMs > recheck) {
               lp->last_mlrs_calctime = pl->realtimeMs;
               LinuxProcessList_readMaps(lp, procFd, settings->flags & PROCESS_FLAG_LINUX_LRS_FIX, settings->highlightDeletedExe);
            }
         } else {
            /* Copy from process structure in threads and reset if setting got disabled */
            proc->usesDeletedLib = (proc->isUserlandThread && parent) ? parent->usesDeletedLib : false;
         }

         proc->mergedCommand.exeChanged |= prev ^ proc->usesDeletedLib;
      }

      if ((settings->flags & PROCESS_FLAG_LINUX_SMAPS) && !Process_isKernelThread(proc)) {
         if (!parent) {
            // Read smaps file of each process only every second pass to improve performance
            static int smaps_flag = 0;
            if ((pid & 1) == smaps_flag) {
               LinuxProcessList_readSmapsFile(lp, procFd, this->haveSmapsRollup);
            }
            if (pid == 1) {
               smaps_flag = !smaps_flag;
            }
         } else {
            lp->m_pss = ((const LinuxProcess*)parent)->m_pss;
         }
      }

      char statCommand[MAX_NAME + 1];
      unsigned long long int lasttimes = (lp->utime + lp->stime);
      unsigned long int tty_nr = proc->tty_nr;
      if (! LinuxProcessList_readStatFile(proc, procFd, statCommand, sizeof(statCommand)))
         goto errorReadingProcess;

      if (tty_nr != proc->tty_nr && this->ttyDrivers) {
         free(proc->tty_name);
         proc->tty_name = LinuxProcessList_updateTtyDevice(this->ttyDrivers, proc->tty_nr);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_IOPRIO) {
         LinuxProcess_updateIOPriority(lp);
      }

      /* period might be 0 after system sleep */
      float percent_cpu = (period < 1E-6) ? 0.0F : ((lp->utime + lp->stime - lasttimes) / period * 100.0);
      proc->percent_cpu = CLAMP(percent_cpu, 0.0F, cpus * 100.0F);
      proc->percent_mem = proc->m_resident / (double)(pl->totalMem) * 100.0;

      if (! LinuxProcessList_updateUser(pl, proc, procFd))
         goto errorReadingProcess;

      if (!preExisting) {

         #ifdef HAVE_OPENVZ
         if (settings->flags & PROCESS_FLAG_LINUX_OPENVZ) {
            LinuxProcessList_readOpenVZData(lp, procFd);
         }
         #endif

         #ifdef HAVE_VSERVER
         if (settings->flags & PROCESS_FLAG_LINUX_VSERVER) {
            LinuxProcessList_readVServerData(lp, procFd);
         }
         #endif

         if (! LinuxProcessList_readCmdlineFile(proc, procFd)) {
            goto errorReadingProcess;
         }

         Process_fillStarttimeBuffer(proc);

         ProcessList_add(pl, proc);
      } else {
         if (settings->updateProcessNames && proc->state != 'Z') {
            if (! LinuxProcessList_readCmdlineFile(proc, procFd)) {
               goto errorReadingProcess;
            }
         }
      }

      #ifdef HAVE_DELAYACCT
      if (settings->flags & PROCESS_FLAG_LINUX_DELAYACCT) {
         LinuxProcessList_readDelayAcctData(this, lp);
      }
      #endif

      if (settings->flags & PROCESS_FLAG_LINUX_CGROUP) {
         LinuxProcessList_readCGroupFile(lp, procFd);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_OOM) {
         LinuxProcessList_readOomData(lp, procFd);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_CTXT) {
         LinuxProcessList_readCtxtData(lp, procFd);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_SECATTR) {
         LinuxProcessList_readSecattrData(lp, procFd);
      }

      if (settings->flags & PROCESS_FLAG_CWD) {
         LinuxProcessList_readCwd(lp, procFd);
      }

      if (proc->state == 'Z' && !proc->cmdline && statCommand[0]) {
         Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
      } else if (Process_isThread(proc)) {
         if ((settings->showThreadNames || Process_isKernelThread(proc)) && statCommand[0]) {
            Process_updateCmdline(proc, statCommand, 0, strlen(statCommand));
         }

         if (Process_isKernelThread(proc)) {
            pl->kernelThreads++;
         } else {
            pl->userlandThreads++;
         }
      }

      /* Set at the end when we know if a new entry is a thread */
      proc->show = ! ((hideKernelThreads && Process_isKernelThread(proc)) || (hideUserlandThreads && Process_isUserlandThread(proc)));

      pl->totalTasks++;
      /* runningTasks is set in LinuxProcessList_scanCPUTime() from /proc/stat */
      proc->updated = true;
      Compat_openatArgClose(procFd);
      continue;

      // Exception handler.

errorReadingProcess:
      {
#ifdef HAVE_OPENAT
         if (procFd >= 0)
            close(procFd);
#endif

         if (preExisting) {
            ProcessList_remove(pl, proc);
         } else {
            Process_delete((Object*)proc);
         }
      }
   }
   closedir(dir);
   return true;
}

static inline void LinuxProcessList_scanMemoryInfo(ProcessList* this) {
   memory_t availableMem = 0;
   memory_t freeMem = 0;
   memory_t totalMem = 0;
   memory_t buffersMem = 0;
   memory_t cachedMem = 0;
   memory_t sharedMem = 0;
   memory_t swapTotalMem = 0;
   memory_t swapCacheMem = 0;
   memory_t swapFreeMem = 0;
   memory_t sreclaimableMem = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (!file)
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);

   char buffer[128];
   while (fgets(buffer, sizeof(buffer), file)) {

      #define tryRead(label, variable)                                       \
         if (String_startsWith(buffer, label)) {                             \
            memory_t parsed_;                                                \
            if (sscanf(buffer + strlen(label), "%llu kB", &parsed_) == 1) {  \
               (variable) = parsed_;                                         \
            }                                                                \
            break;                                                           \
         }

      switch (buffer[0]) {
      case 'M':
         tryRead("MemAvailable:", availableMem);
         tryRead("MemFree:", freeMem);
         tryRead("MemTotal:", totalMem);
         break;
      case 'B':
         tryRead("Buffers:", buffersMem);
         break;
      case 'C':
         tryRead("Cached:", cachedMem);
         break;
      case 'S':
         switch (buffer[1]) {
         case 'h':
            tryRead("Shmem:", sharedMem);
            break;
         case 'w':
            tryRead("SwapTotal:", swapTotalMem);
            tryRead("SwapCached:", swapCacheMem);
            tryRead("SwapFree:", swapFreeMem);
            break;
         case 'R':
            tryRead("SReclaimable:", sreclaimableMem);
            break;
         }
         break;
      }

      #undef tryRead
   }

   fclose(file);

   /*
    * Compute memory partition like procps(free)
    *  https://gitlab.com/procps-ng/procps/-/blob/master/proc/sysinfo.c
    *
    * Adjustments:
    *  - Shmem in part of Cached (see https://lore.kernel.org/patchwork/patch/648763/),
    *    do not show twice by subtracting from Cached and do not subtract twice from used.
    */
   this->totalMem = totalMem;
   this->cachedMem = cachedMem + sreclaimableMem - sharedMem;
   this->sharedMem = sharedMem;
   const memory_t usedDiff = freeMem + cachedMem + sreclaimableMem + buffersMem;
   this->usedMem = (totalMem >= usedDiff) ? totalMem - usedDiff : totalMem - freeMem;
   this->buffersMem = buffersMem;
   this->availableMem = availableMem != 0 ? MINIMUM(availableMem, totalMem) : freeMem;
   this->totalSwap = swapTotalMem;
   this->usedSwap = swapTotalMem - swapFreeMem - swapCacheMem;
   this->cachedSwap = swapCacheMem;
}

static void LinuxProcessList_scanHugePages(LinuxProcessList* this) {
   this->totalHugePageMem = 0;
   for (unsigned i = 0; i < HTOP_HUGEPAGE_COUNT; i++) {
      this->usedHugePageMem[i] = MEMORY_MAX;
   }

   DIR* dir = opendir("/sys/kernel/mm/hugepages");
   if (!dir)
      return;

   const struct dirent* entry;
   while ((entry = readdir(dir)) != NULL) {
      const char* name = entry->d_name;

      /* Ignore all non-directories */
      if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN)
         continue;

      if (!String_startsWith(name, "hugepages-"))
         continue;

      char* endptr;
      unsigned long int hugePageSize = strtoul(name + strlen("hugepages-"), &endptr, 10);
      if (!endptr || *endptr != 'k')
         continue;

      char content[64];
      char hugePagePath[128];
      ssize_t r;

      xSnprintf(hugePagePath, sizeof(hugePagePath), "/sys/kernel/mm/hugepages/%s/nr_hugepages", name);
      r = xReadfile(hugePagePath, content, sizeof(content));
      if (r <= 0)
         continue;

      memory_t total = strtoull(content, NULL, 10);
      if (total == 0)
         continue;

      xSnprintf(hugePagePath, sizeof(hugePagePath), "/sys/kernel/mm/hugepages/%s/free_hugepages", name);
      r = xReadfile(hugePagePath, content, sizeof(content));
      if (r <= 0)
         continue;

      memory_t free = strtoull(content, NULL, 10);

      int shift = ffsl(hugePageSize) - 1 - (HTOP_HUGEPAGE_BASE_SHIFT - 10);
      assert(shift >= 0 && shift < HTOP_HUGEPAGE_COUNT);

      this->totalHugePageMem += total * hugePageSize;
      this->usedHugePageMem[shift] = (total - free) * hugePageSize;
   }

   closedir(dir);
}

static inline void LinuxProcessList_scanZramInfo(LinuxProcessList* this) {
   memory_t totalZram = 0;
   memory_t usedZramComp = 0;
   memory_t usedZramOrig = 0;

   char mm_stat[34];
   char disksize[34];

   unsigned int i = 0;
   for (;;) {
      xSnprintf(mm_stat, sizeof(mm_stat), "/sys/block/zram%u/mm_stat", i);
      xSnprintf(disksize, sizeof(disksize), "/sys/block/zram%u/disksize", i);
      i++;
      FILE* disksize_file = fopen(disksize, "r");
      FILE* mm_stat_file = fopen(mm_stat, "r");
      if (disksize_file == NULL || mm_stat_file == NULL) {
         if (disksize_file) {
            fclose(disksize_file);
         }
         if (mm_stat_file) {
            fclose(mm_stat_file);
         }
         break;
      }
      memory_t size = 0;
      memory_t orig_data_size = 0;
      memory_t compr_data_size = 0;

      if (!fscanf(disksize_file, "%llu\n", &size) ||
          !fscanf(mm_stat_file, "    %llu       %llu", &orig_data_size, &compr_data_size)) {
         fclose(disksize_file);
         fclose(mm_stat_file);
         break;
      }

      totalZram += size;
      usedZramComp += compr_data_size;
      usedZramOrig += orig_data_size;

      fclose(disksize_file);
      fclose(mm_stat_file);
   }

   this->zram.totalZram = totalZram / 1024;
   this->zram.usedZramComp = usedZramComp / 1024;
   this->zram.usedZramOrig = usedZramOrig / 1024;
}

static inline void LinuxProcessList_scanZfsArcstats(LinuxProcessList* lpl) {
   memory_t dbufSize = 0;
   memory_t dnodeSize = 0;
   memory_t bonusSize = 0;

   FILE* file = fopen(PROCARCSTATSFILE, "r");
   if (file == NULL) {
      lpl->zfs.enabled = 0;
      return;
   }
   char buffer[128];
   while (fgets(buffer, 128, file)) {
      #define tryRead(label, variable)                                         \
         if (String_startsWith(buffer, label)) {                               \
            sscanf(buffer + strlen(label), " %*2u %32llu", variable);          \
            break;                                                             \
         }
      #define tryReadFlag(label, variable, flag)                               \
         if (String_startsWith(buffer, label)) {                               \
            (flag) = sscanf(buffer + strlen(label), " %*2u %32llu", variable); \
            break;                                                             \
         }

      switch (buffer[0]) {
      case 'c':
         tryRead("c_max", &lpl->zfs.max);
         tryReadFlag("compressed_size", &lpl->zfs.compressed, lpl->zfs.isCompressed);
         break;
      case 'u':
         tryRead("uncompressed_size", &lpl->zfs.uncompressed);
         break;
      case 's':
         tryRead("size", &lpl->zfs.size);
         break;
      case 'h':
         tryRead("hdr_size", &lpl->zfs.header);
         break;
      case 'd':
         tryRead("dbuf_size", &dbufSize);
         tryRead("dnode_size", &dnodeSize);
         break;
      case 'b':
         tryRead("bonus_size", &bonusSize);
         break;
      case 'a':
         tryRead("anon_size", &lpl->zfs.anon);
         break;
      case 'm':
         tryRead("mfu_size", &lpl->zfs.MFU);
         tryRead("mru_size", &lpl->zfs.MRU);
         break;
      }
      #undef tryRead
      #undef tryReadFlag
   }
   fclose(file);

   lpl->zfs.enabled = (lpl->zfs.size > 0 ? 1 : 0);
   lpl->zfs.size    /= 1024;
   lpl->zfs.max    /= 1024;
   lpl->zfs.MFU    /= 1024;
   lpl->zfs.MRU    /= 1024;
   lpl->zfs.anon   /= 1024;
   lpl->zfs.header /= 1024;
   lpl->zfs.other   = (dbufSize + dnodeSize + bonusSize) / 1024;
   if ( lpl->zfs.isCompressed ) {
      lpl->zfs.compressed /= 1024;
      lpl->zfs.uncompressed /= 1024;
   }
}

static inline double LinuxProcessList_scanCPUTime(ProcessList* super) {
   LinuxProcessList* this = (LinuxProcessList*) super;

   FILE* file = fopen(PROCSTATFILE, "r");
   if (!file)
      CRT_fatalError("Cannot open " PROCSTATFILE);

   LinuxProcessList_updateCPUcount(super, file);

   rewind(file);

   unsigned int cpus = super->cpuCount;
   for (unsigned int i = 0; i <= cpus; i++) {
      char buffer[PROC_LINE_LENGTH + 1];
      unsigned long long int usertime, nicetime, systemtime, idletime;
      unsigned long long int ioWait = 0, irq = 0, softIrq = 0, steal = 0, guest = 0, guestnice = 0;
      // Depending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      const char* ok = fgets(buffer, sizeof(buffer), file);
      if (!ok)
         break;

      if (i == 0) {
         (void) sscanf(buffer,   "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",         &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
      } else {
         unsigned int cpuid;
         (void) sscanf(buffer, "cpu%4u %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         assert(cpuid == i - 1);
      }
      // Guest time is already accounted in usertime
      usertime -= guest;
      nicetime -= guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      unsigned long long int idlealltime = idletime + ioWait;
      unsigned long long int systemalltime = systemtime + irq + softIrq;
      unsigned long long int virtalltime = guest + guestnice;
      unsigned long long int totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      CPUData* cpuData = &(this->cpus[i]);
      // Since we do a subtraction (usertime - guest) and cputime64_to_clock_t()
      // used in /proc/stat rounds down numbers, it can lead to a case where the
      // integer overflow.
      #define WRAP_SUBTRACT(a,b) (((a) > (b)) ? (a) - (b) : 0)
      cpuData->userPeriod = WRAP_SUBTRACT(usertime, cpuData->userTime);
      cpuData->nicePeriod = WRAP_SUBTRACT(nicetime, cpuData->niceTime);
      cpuData->systemPeriod = WRAP_SUBTRACT(systemtime, cpuData->systemTime);
      cpuData->systemAllPeriod = WRAP_SUBTRACT(systemalltime, cpuData->systemAllTime);
      cpuData->idleAllPeriod = WRAP_SUBTRACT(idlealltime, cpuData->idleAllTime);
      cpuData->idlePeriod = WRAP_SUBTRACT(idletime, cpuData->idleTime);
      cpuData->ioWaitPeriod = WRAP_SUBTRACT(ioWait, cpuData->ioWaitTime);
      cpuData->irqPeriod = WRAP_SUBTRACT(irq, cpuData->irqTime);
      cpuData->softIrqPeriod = WRAP_SUBTRACT(softIrq, cpuData->softIrqTime);
      cpuData->stealPeriod = WRAP_SUBTRACT(steal, cpuData->stealTime);
      cpuData->guestPeriod = WRAP_SUBTRACT(virtalltime, cpuData->guestTime);
      cpuData->totalPeriod = WRAP_SUBTRACT(totaltime, cpuData->totalTime);
      #undef WRAP_SUBTRACT
      cpuData->userTime = usertime;
      cpuData->niceTime = nicetime;
      cpuData->systemTime = systemtime;
      cpuData->systemAllTime = systemalltime;
      cpuData->idleAllTime = idlealltime;
      cpuData->idleTime = idletime;
      cpuData->ioWaitTime = ioWait;
      cpuData->irqTime = irq;
      cpuData->softIrqTime = softIrq;
      cpuData->stealTime = steal;
      cpuData->guestTime = virtalltime;
      cpuData->totalTime = totaltime;
   }

   double period = (double)this->cpus[0].totalPeriod / cpus;

   char buffer[PROC_LINE_LENGTH + 1];
   while (fgets(buffer, sizeof(buffer), file)) {
      if (String_startsWith(buffer, "procs_running")) {
         super->runningTasks = strtoul(buffer + strlen("procs_running"), NULL, 10);
         break;
      }
   }

   fclose(file);

   return period;
}

static int scanCPUFreqencyFromSysCPUFreq(LinuxProcessList* this) {
   unsigned int cpus = this->super.cpuCount;
   int numCPUsWithFrequency = 0;
   unsigned long totalFrequency = 0;

   /*
    * On some AMD and Intel CPUs read()ing scaling_cur_freq is quite slow (> 1ms). This delay
    * accumulates for every core. For details see issue#471.
    * If the read on CPU 0 takes longer than 500us bail out and fall back to reading the
    * frequencies from /proc/cpuinfo.
    * Once the condition has been met, bail out early for the next couple of scans.
    */
   static int timeout = 0;

   if (timeout > 0) {
      timeout--;
      return -1;
   }

   for (unsigned int i = 0; i < cpus; ++i) {
      char pathBuffer[64];
      xSnprintf(pathBuffer, sizeof(pathBuffer), "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", i);

      struct timespec start;
      if (i == 0)
         clock_gettime(CLOCK_MONOTONIC, &start);

      FILE* file = fopen(pathBuffer, "r");
      if (!file)
         return -errno;

      unsigned long frequency;
      if (fscanf(file, "%lu", &frequency) == 1) {
         /* convert kHz to MHz */
         frequency = frequency / 1000;
         this->cpus[i + 1].frequency = frequency;
         numCPUsWithFrequency++;
         totalFrequency += frequency;
      }

      fclose(file);

      if (i == 0) {
         struct timespec end;
         clock_gettime(CLOCK_MONOTONIC, &end);
         const time_t timeTakenUs = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
         if (timeTakenUs > 500) {
            timeout = 30;
            return -1;
         }
      }

   }

   if (numCPUsWithFrequency > 0)
      this->cpus[0].frequency = (double)totalFrequency / numCPUsWithFrequency;

   return 0;
}

static void scanCPUFreqencyFromCPUinfo(LinuxProcessList* this) {
   FILE* file = fopen(PROCCPUINFOFILE, "r");
   if (file == NULL)
      return;

   unsigned int cpus = this->super.cpuCount;
   int numCPUsWithFrequency = 0;
   double totalFrequency = 0;
   int cpuid = -1;

   while (!feof(file)) {
      double frequency;
      char buffer[PROC_LINE_LENGTH];

      if (fgets(buffer, PROC_LINE_LENGTH, file) == NULL)
         break;

      if (
         (sscanf(buffer, "processor : %d", &cpuid) == 1) ||
         (sscanf(buffer, "processor: %d", &cpuid) == 1)
      ) {
         continue;
      } else if (
         (sscanf(buffer, "cpu MHz : %lf", &frequency) == 1) ||
         (sscanf(buffer, "cpu MHz: %lf", &frequency) == 1) ||
         (sscanf(buffer, "clock : %lfMHz", &frequency) == 1) ||
         (sscanf(buffer, "clock: %lfMHz", &frequency) == 1)
      ) {
         if (cpuid < 0 || (unsigned int)cpuid > (cpus - 1)) {
            continue;
         }

         CPUData* cpuData = &(this->cpus[cpuid + 1]);
         /* do not override sysfs data */
         if (isnan(cpuData->frequency)) {
            cpuData->frequency = frequency;
         }
         numCPUsWithFrequency++;
         totalFrequency += frequency;
      } else if (buffer[0] == '\n') {
         cpuid = -1;
      }
   }
   fclose(file);

   if (numCPUsWithFrequency > 0) {
      this->cpus[0].frequency = totalFrequency / numCPUsWithFrequency;
   }
}

static void LinuxProcessList_scanCPUFrequency(LinuxProcessList* this) {
   unsigned int cpus = this->super.cpuCount;

   for (unsigned int i = 0; i <= cpus; i++) {
      this->cpus[i].frequency = NAN;
   }

   if (scanCPUFreqencyFromSysCPUFreq(this) == 0) {
      return;
   }

   scanCPUFreqencyFromCPUinfo(this);
}

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   LinuxProcessList* this = (LinuxProcessList*) super;
   const Settings* settings = super->settings;

   LinuxProcessList_scanMemoryInfo(super);
   LinuxProcessList_scanHugePages(this);
   LinuxProcessList_scanZfsArcstats(this);
   LinuxProcessList_scanZramInfo(this);

   double period = LinuxProcessList_scanCPUTime(super);

   if (settings->showCPUFrequency) {
      LinuxProcessList_scanCPUFrequency(this);
   }

   #ifdef HAVE_SENSORS_SENSORS_H
   if (settings->showCPUTemperature)
      LibSensors_getCPUTemperatures(this->cpus, this->super.cpuCount);
   #endif

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   /* PROCDIR is an absolute path */
   assert(PROCDIR[0] == '/');
#ifdef HAVE_OPENAT
   openat_arg_t rootFd = AT_FDCWD;
#else
   openat_arg_t rootFd = "";
#endif

   LinuxProcessList_recurseProcTree(this, rootFd, PROCDIR, NULL, period);
}
