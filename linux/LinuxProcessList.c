/*
htop - LinuxProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "LinuxProcessList.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

#include "CRT.h"
#include "Compat.h"
#include "LinuxProcess.h"
#include "Macros.h"
#include "Object.h"
#include "Process.h"
#include "Settings.h"
#include "XUtils.h"

#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS)
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_LIBSENSORS
#include <sensors/sensors.h>
#endif


static ssize_t xread(int fd, void* buf, size_t count) {
   // Read some bytes. Retry on EINTR and when we don't get as many bytes as we requested.
   size_t alreadyRead = 0;
   for (;;) {
      ssize_t res = read(fd, buf, count);
      if (res == -1) {
         if (errno == EINTR)
            continue;
         return -1;
      }

      if (res > 0) {
         buf = ((char*)buf) + res;
         count -= res;
         alreadyRead += res;
      }

      if (count == 0 || res == 0) {
         return alreadyRead;
      }
   }
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
   int fd = open(PROCTTYDRIVERSFILE, O_RDONLY);
   if (fd == -1)
      return;

   char* buf = NULL;
   int bufSize = MAX_READ;
   int bufLen = 0;
   for (;;) {
      buf = xRealloc(buf, bufSize);
      int size = xread(fd, buf + bufLen, MAX_READ);
      if (size <= 0) {
         buf[bufLen] = '\0';
         close(fd);
         break;
      }
      bufLen += size;
      bufSize += MAX_READ;
   }
   if (bufLen == 0) {
      free(buf);
      return;
   }
   int numDrivers = 0;
   int allocd = 10;
   ttyDrivers = xMalloc(sizeof(TtyDriver) * allocd);
   char* at = buf;
   while (*at != '\0') {
      at = strchr(at, ' ');    // skip first token
      while (*at == ' ') at++; // skip spaces
      char* token = at;        // mark beginning of path
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
         ttyDrivers = xRealloc(ttyDrivers, sizeof(TtyDriver) * allocd);
      }
   }
   free(buf);
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

static int LinuxProcessList_computeCPUcount(void) {
   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }

   int cpus = 0;
   char buffer[PROC_LINE_LENGTH + 1];
   while (fgets(buffer, sizeof(buffer), file)) {
      if (String_startsWith(buffer, "cpu")) {
         cpus++;
      }
   }

   fclose(file);

   /* subtract raw cpu entry */
   if (cpus > 0) {
      cpus--;
   }

   return cpus;
}

static void LinuxProcessList_updateCPUcount(LinuxProcessList* this) {
   ProcessList* pl = &(this->super);
   int cpus = LinuxProcessList_computeCPUcount();
   if (cpus == 0 || cpus == pl->cpuCount)
      return;

   pl->cpuCount = cpus;
   free(this->cpus);
   this->cpus = xCalloc(cpus + 1, sizeof(CPUData));

   for (int i = 0; i <= cpus; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }
}

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidMatchList, uid_t userId) {
   LinuxProcessList* this = xCalloc(1, sizeof(LinuxProcessList));
   ProcessList* pl = &(this->super);

   ProcessList_init(pl, Class(LinuxProcess), usersTable, pidMatchList, userId);
   LinuxProcessList_initTtyDrivers(this);

   #ifdef HAVE_DELAYACCT
   LinuxProcessList_initNetlinkSocket(this);
   #endif

   // Check for /proc/*/smaps_rollup availability (improves smaps parsing speed, Linux 4.14+)
   FILE* file = fopen(PROCDIR "/self/smaps_rollup", "r");
   if (file != NULL) {
      this->haveSmapsRollup = true;
      fclose(file);
   } else {
      this->haveSmapsRollup = false;
   }

   // Read btime
   {
      FILE* statfile = fopen(PROCSTATFILE, "r");
      if (statfile == NULL) {
         CRT_fatalError("Cannot open " PROCSTATFILE);
      }

      while (true) {
         char buffer[PROC_LINE_LENGTH + 1];
         if (fgets(buffer, sizeof(buffer), statfile) == NULL) {
            CRT_fatalError("No btime in " PROCSTATFILE);
         } else if (String_startsWith(buffer, "btime ")) {
            if (sscanf(buffer, "btime %lld\n", &btime) != 1) {
               CRT_fatalError("Failed to parse btime from " PROCSTATFILE);
            }
            break;
         }
      }

      fclose(statfile);
   }

   // Initialize CPU count
   {
      int cpus = LinuxProcessList_computeCPUcount();
      pl->cpuCount = MAXIMUM(cpus, 1);
      this->cpus = xCalloc(cpus + 1, sizeof(CPUData));

      for (int i = 0; i <= cpus; i++) {
         this->cpus[i].totalTime = 1;
         this->cpus[i].totalPeriod = 1;
      }
   }

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

static inline unsigned long long LinuxProcess_adjustTime(unsigned long long t) {
   static long jiffy = -1;
   if (jiffy == -1) {
      errno = 0;
      jiffy = sysconf(_SC_CLK_TCK);
      if (errno || -1 == jiffy) {
         jiffy = -1;
         return t; // Assume 100Hz clock
      }
   }
   return t * 100 / jiffy;
}

static bool LinuxProcessList_readStatFile(Process* process, const char* dirname, const char* name, char* command, int* commLen) {
   LinuxProcess* lp = (LinuxProcess*) process;
   const int commLenIn = *commLen;
   *commLen = 0;
   char filename[MAX_NAME + 1];
   xSnprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;

   static char buf[MAX_READ + 1];

   int size = xread(fd, buf, MAX_READ);
   close(fd);
   if (size <= 0)
      return false;
   buf[size] = '\0';

   assert(process->pid == atoi(buf));
   char* location = strchr(buf, ' ');
   if (!location)
      return false;

   location += 2;
   char* end = strrchr(location, ')');
   if (!end)
      return false;

   int commsize = MINIMUM(end - location, commLenIn - 1);
   //  deepcode ignore BufferOverflow: commsize is bounded by the allocated length passed in by commLen, saved into commLenIn
   memcpy(command, location, commsize);
   command[commsize] = '\0';
   *commLen = commsize;
   location = end + 2;

   process->state = location[0];
   location += 2;
   process->ppid = strtol(location, &location, 10);
   location += 1;
   process->pgrp = strtoul(location, &location, 10);
   location += 1;
   process->session = strtoul(location, &location, 10);
   location += 1;
   process->tty_nr = strtoul(location, &location, 10);
   location += 1;
   process->tpgid = strtol(location, &location, 10);
   location += 1;
   process->flags = strtoul(location, &location, 10);
   location += 1;
   process->minflt = strtoull(location, &location, 10);
   location += 1;
   lp->cminflt = strtoull(location, &location, 10);
   location += 1;
   process->majflt = strtoull(location, &location, 10);
   location += 1;
   lp->cmajflt = strtoull(location, &location, 10);
   location += 1;
   lp->utime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   location += 1;
   lp->stime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   location += 1;
   lp->cutime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   location += 1;
   lp->cstime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   location += 1;
   process->priority = strtol(location, &location, 10);
   location += 1;
   process->nice = strtol(location, &location, 10);
   location += 1;
   process->nlwp = strtol(location, &location, 10);
   location += 1;
   location = strchr(location, ' ') + 1;
   if (process->starttime_ctime == 0) {
      process->starttime_ctime = btime + LinuxProcess_adjustTime(strtoll(location, &location, 10)) / 100;
   } else {
      location = strchr(location, ' ') + 1;
   }
   location += 1;
   for (int i = 0; i < 15; i++) {
      location = strchr(location, ' ') + 1;
   }
   process->exit_signal = strtol(location, &location, 10);
   location += 1;
   assert(location != NULL);
   process->processor = strtol(location, &location, 10);

   process->time = lp->utime + lp->stime;

   return true;
}


static bool LinuxProcessList_statProcessDir(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME + 1];
   filename[MAX_NAME] = '\0';

   xSnprintf(filename, MAX_NAME, "%s/%s", dirname, name);
   struct stat sstat;
   int statok = stat(filename, &sstat);
   if (statok == -1)
      return false;
   process->st_uid = sstat.st_uid;
   return true;
}

#ifdef HAVE_TASKSTATS

static void LinuxProcessList_readIoFile(LinuxProcess* process, const char* dirname, const char* name, unsigned long long now) {
   char filename[MAX_NAME + 1];
   filename[MAX_NAME] = '\0';

   xSnprintf(filename, MAX_NAME, "%s/%s/io", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1) {
      process->io_rate_read_bps = NAN;
      process->io_rate_write_bps = NAN;
      process->io_rchar = -1LL;
      process->io_wchar = -1LL;
      process->io_syscr = -1LL;
      process->io_syscw = -1LL;
      process->io_read_bytes = -1LL;
      process->io_write_bytes = -1LL;
      process->io_cancelled_write_bytes = -1LL;
      process->io_rate_read_time = -1LL;
      process->io_rate_write_time = -1LL;
      return;
   }

   char buffer[1024];
   ssize_t buflen = xread(fd, buffer, 1023);
   close(fd);
   if (buflen < 1)
      return;

   buffer[buflen] = '\0';
   unsigned long long last_read = process->io_read_bytes;
   unsigned long long last_write = process->io_write_bytes;
   char* buf = buffer;
   char* line = NULL;
   while ((line = strsep(&buf, "\n")) != NULL) {
      switch (line[0]) {
      case 'r':
         if (line[1] == 'c' && String_startsWith(line + 2, "har: ")) {
            process->io_rchar = strtoull(line + 7, NULL, 10);
         } else if (String_startsWith(line + 1, "ead_bytes: ")) {
            process->io_read_bytes = strtoull(line + 12, NULL, 10);
            process->io_rate_read_bps =
               ((double)(process->io_read_bytes - last_read)) / (((double)(now - process->io_rate_read_time)) / 1000);
            process->io_rate_read_time = now;
         }
         break;
      case 'w':
         if (line[1] == 'c' && String_startsWith(line + 2, "har: ")) {
            process->io_wchar = strtoull(line + 7, NULL, 10);
         } else if (String_startsWith(line + 1, "rite_bytes: ")) {
            process->io_write_bytes = strtoull(line + 13, NULL, 10);
            process->io_rate_write_bps =
               ((double)(process->io_write_bytes - last_write)) / (((double)(now - process->io_rate_write_time)) / 1000);
            process->io_rate_write_time = now;
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
}

#endif



static bool LinuxProcessList_readStatmFile(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME + 1];
   xSnprintf(filename, sizeof(filename), "%s/%s/statm", dirname, name);
   FILE* statmfile = fopen(filename, "r");
   if (!statmfile)
      return false;

   int r = fscanf(statmfile, "%ld %ld %ld %ld %ld %ld %ld",
                  &process->super.m_virt,
                  &process->super.m_resident,
                  &process->m_share,
                  &process->m_trs,
                  &process->m_lrs,
                  &process->m_drs,
                  &process->m_dt);
   fclose(statmfile);
   return r == 7;
}

static bool LinuxProcessList_readSmapsFile(LinuxProcess* process, const char* dirname, const char* name, bool haveSmapsRollup) {
   //http://elixir.free-electrons.com/linux/v4.10/source/fs/proc/task_mmu.c#L719
   //kernel will return data in chunks of size PAGE_SIZE or less.

   char buffer[256];

   if (haveSmapsRollup) {// only available in Linux 4.14+
      xSnprintf(buffer, sizeof(buffer), "%s/%s/smaps_rollup", dirname, name);
   } else {
      xSnprintf(buffer, sizeof(buffer), "%s/%s/smaps", dirname, name);
   }

   FILE* f = fopen(buffer, "r");
   if (!f)
      return false;

   process->m_pss   = 0;
   process->m_swap  = 0;
   process->m_psswp = 0;

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

static void LinuxProcessList_readOpenVZData(LinuxProcess* process, const char* dirname, const char* name) {
   if ( (access(PROCDIR "/vz", R_OK) != 0)) {
      free(process->ctid);
      process->ctid = NULL;
      process->vpid = process->super.pid;
      return;
   }

   char filename[MAX_NAME + 1];
   xSnprintf(filename, sizeof(filename), "%s/%s/status", dirname, name);
   FILE* file = fopen(filename, "r");
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
         if (!String_eq(name_value_sep, process->ctid ? process->ctid : "")) {
            free(process->ctid);
            process->ctid = xStrdup(name_value_sep);
         }
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

#ifdef HAVE_CGROUP

static void LinuxProcessList_readCGroupFile(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME + 1];
   xSnprintf(filename, MAX_NAME, "%s/%s/cgroup", dirname, name);
   FILE* file = fopen(filename, "r");
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
      char* ok = fgets(buffer, PROC_LINE_LENGTH, file);
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
   free(process->cgroup);
   process->cgroup = xStrdup(output);
}

#endif

#ifdef HAVE_VSERVER

static void LinuxProcessList_readVServerData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME + 1];
   xSnprintf(filename, MAX_NAME, "%s/%s/status", dirname, name);
   FILE* file = fopen(filename, "r");
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

static void LinuxProcessList_readOomData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME + 1];
   xSnprintf(filename, MAX_NAME, "%s/%s/oom_score", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      return;
   }
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

static void LinuxProcessList_readCtxtData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME + 1];
   xSnprintf(filename, MAX_NAME, "%s/%s/status", dirname, name);
   FILE* file = fopen(filename, "r");
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

static void LinuxProcessList_readSecattrData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME + 1];
   xSnprintf(filename, sizeof(filename), "%s/%s/attr/current", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      free(process->secattr);
      process->secattr = NULL;
      return;
   }
   char buffer[PROC_LINE_LENGTH + 1];
   char* res = fgets(buffer, sizeof(buffer), file);
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
   free(process->secattr);
   process->secattr = xStrdup(buffer);
}

#ifdef HAVE_DELAYACCT

static int handleNetlinkMsg(struct nl_msg* nlmsg, void* linuxProcess) {
   struct nlmsghdr* nlhdr;
   struct nlattr* nlattrs[TASKSTATS_TYPE_MAX + 1];
   struct nlattr* nlattr;
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

   if (nl_socket_modify_cb(this->netlink_socket, NL_CB_VALID, NL_CB_CUSTOM, handleNetlinkMsg, process) < 0) {
      return;
   }

   if (! (msg = nlmsg_alloc())) {
      return;
   }

   if (! genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, this->netlink_family, 0, NLM_F_REQUEST, TASKSTATS_CMD_GET, TASKSTATS_VERSION)) {
      nlmsg_free(msg);
   }

   if (nla_put_u32(msg, TASKSTATS_CMD_ATTR_PID, process->super.pid) < 0) {
      nlmsg_free(msg);
   }

   if (nl_send_sync(this->netlink_socket, msg) < 0) {
      process->swapin_delay_percent = NAN;
      process->blkio_delay_percent = NAN;
      process->cpu_delay_percent = NAN;
      return;
   }

   if (nl_recvmsgs_default(this->netlink_socket) < 0) {
      return;
   }
}

#endif

static void setCommand(Process* process, const char* command, int len) {
   if (process->comm && process->commLen >= len) {
      strncpy(process->comm, command, len + 1);
   } else {
      free(process->comm);
      process->comm = xStrdup(command);
   }
   process->commLen = len;
}

static bool LinuxProcessList_readCmdlineFile(Process* process, const char* dirname, const char* name) {
   LinuxProcess *lp = (LinuxProcess *)process;
   char filename[MAX_NAME + 1];
   xSnprintf(filename, MAX_NAME, "%s/%s/cmdline", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;

   char command[4096 + 1]; // max cmdline length on Linux
   int amtRead = xread(fd, command, sizeof(command) - 1);
   close(fd);

   if (amtRead == 0) {
      if (process->state == 'Z') {
         process->basenameOffset = 0;
      } else {
         ((LinuxProcess*)process)->isKernelThread = true;
      }
      return true;
   } else if (amtRead < 0) {
      return false;
   }

   int tokenEnd = 0;
   int tokenStart = 0;
   int lastChar = 0;
   bool argSepNUL = false;
   bool argSepSpace = false;

   for (int i = 0; i < amtRead; i++) {
      /* newline used as delimiter - when forming the mergedCommand, newline is
       * converted to space by LinuxProcess_makeCommandStr */
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
               if (command[i] == '/') {
                  tokenStart = i + 1;
               } else if (command[i] == '\\' && (!tokenStart || command[tokenStart - 1] == '\\')) {
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
   }

   if (tokenEnd == 0) {
      tokenEnd = lastChar + 1;
   }

   lp->mergedCommand.maxLen = lastChar + 1;  /* accomodate cmdline */
   if (!process->comm || strcmp(command, process->comm)) {
      process->basenameOffset = tokenEnd;
      setCommand(process, command, lastChar + 1);
      lp->procCmdlineBasenameOffset = tokenStart;
      lp->procCmdlineBasenameEnd = tokenEnd;
      lp->mergedCommand.cmdlineChanged = true;
   }

   /* /proc/[pid]/comm could change, so should be udpated */
   xSnprintf(filename, MAX_NAME, "%s/%s/comm", dirname, name);
   if ((fd = open(filename, O_RDONLY)) != -1 &&
       (amtRead = xread(fd, command, sizeof(command) - 1)) > 0) {
      close(fd);
      command[amtRead - 1] = 0;
      lp->mergedCommand.maxLen += amtRead - 1;  /* accomodate comm */
      if (!lp->procComm || strcmp(command, lp->procComm)) {
         free(lp->procComm);
         lp->procComm = xStrdup(command);
         lp->mergedCommand.commChanged = true;
      }
   } else if (lp->procComm) {
      free(lp->procComm);
      lp->procComm = NULL;
      lp->mergedCommand.commChanged = true;
   }

   /* execve could change /proc/[pid]/exe, so procExe should be udpated */
   xSnprintf(command, sizeof(command), "%s/%s/exe", dirname, name);
   if ((amtRead = readlink(command, filename, sizeof(filename) - 1)) > 0) {
      filename[amtRead] = 0;
      lp->mergedCommand.maxLen += amtRead;  /* accomodate exe */
      if (!lp->procExe || strcmp(filename, lp->procExe)) {
         free(lp->procExe);
         lp->procExe = xStrdup(filename);
         lp->procExeLen = amtRead;
         /* exe is guaranteed to contain at least one /, but validate anyway */
         while (amtRead && filename[--amtRead] != '/')
            ;
         lp->procExeBasenameOffset = amtRead + 1;
         lp->mergedCommand.exeChanged = true;

         const char* deletedMarker = " (deleted)";
         if (strlen(lp->procExe) > strlen(deletedMarker)) {
            lp->procExeDeleted = String_eq(lp->procExe + strlen(lp->procExe) - strlen(deletedMarker), deletedMarker);

            if (lp->procExeDeleted && strlen(lp->procExe) - strlen(deletedMarker) == 1 && lp->procExe[0] == '/') {
               lp->procExeBasenameOffset = 0;
            }
         }
      }
   } else if (lp->procExe) {
      free(lp->procExe);
      lp->procExe = NULL;
      lp->procExeLen = 0;
      lp->procExeBasenameOffset = 0;
      lp->procExeDeleted = false;
      lp->mergedCommand.exeChanged = true;
   }

   return true;
}

static char* LinuxProcessList_updateTtyDevice(TtyDriver* ttyDrivers, unsigned int tty_nr) {
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

static bool LinuxProcessList_recurseProcTree(LinuxProcessList* this, const char* dirname, Process* parent, double period, struct timeval tv) {
   ProcessList* pl = (ProcessList*) this;
   DIR* dir;
   const struct dirent* entry;
   const Settings* settings = pl->settings;

   #ifdef HAVE_TASKSTATS
   unsigned long long now = tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
   #endif

   dir = opendir(dirname);
   if (!dir)
      return false;

   int cpus = pl->cpuCount;
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
      int pid = atoi(name);

      if (parent && pid == parent->pid)
         continue;

      if (pid <= 0)
         continue;

      bool preExisting = false;
      Process* proc = ProcessList_getProcess(pl, pid, &preExisting, LinuxProcess_new);
      proc->tgid = parent ? parent->pid : pid;

      LinuxProcess* lp = (LinuxProcess*) proc;

      char subdirname[MAX_NAME + 1];
      xSnprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
      LinuxProcessList_recurseProcTree(this, subdirname, proc, period, tv);

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
         continue;
      }
      if (preExisting && hideUserlandThreads && Process_isUserlandThread(proc)) {
         proc->updated = true;
         proc->show = false;
         pl->userlandThreads++;
         pl->totalTasks++;
         continue;
      }

      #ifdef HAVE_TASKSTATS
      if (settings->flags & PROCESS_FLAG_IO)
         LinuxProcessList_readIoFile(lp, dirname, name, now);
      #endif

      if (! LinuxProcessList_readStatmFile(lp, dirname, name))
         goto errorReadingProcess;

      if ((settings->flags & PROCESS_FLAG_LINUX_SMAPS) && !Process_isKernelThread(proc)) {
         if (!parent) {
            // Read smaps file of each process only every second pass to improve performance
            static int smaps_flag = 0;
            if ((pid & 1) == smaps_flag) {
               LinuxProcessList_readSmapsFile(lp, dirname, name, this->haveSmapsRollup);
            }
            if (pid == 1) {
               smaps_flag = !smaps_flag;
            }
         } else {
            lp->m_pss = ((LinuxProcess*)parent)->m_pss;
         }
      }

      char command[MAX_NAME + 1];
      unsigned long long int lasttimes = (lp->utime + lp->stime);
      int commLen = sizeof(command);
      unsigned int tty_nr = proc->tty_nr;
      if (! LinuxProcessList_readStatFile(proc, dirname, name, command, &commLen))
         goto errorReadingProcess;

      if (tty_nr != proc->tty_nr && this->ttyDrivers) {
         free(lp->ttyDevice);
         lp->ttyDevice = LinuxProcessList_updateTtyDevice(this->ttyDrivers, proc->tty_nr);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_IOPRIO) {
         LinuxProcess_updateIOPriority(lp);
      }

      float percent_cpu = (lp->utime + lp->stime - lasttimes) / period * 100.0;
      proc->percent_cpu = isnan(percent_cpu) ? 0.0 : CLAMP(percent_cpu, 0.0, cpus * 100.0);
      proc->percent_mem = (proc->m_resident * CRT_pageSizeKB) / (double)(pl->totalMem) * 100.0;

      if (!preExisting) {

         if (! LinuxProcessList_statProcessDir(proc, dirname, name))
            goto errorReadingProcess;

         proc->user = UsersTable_getRef(pl->usersTable, proc->st_uid);

         #ifdef HAVE_OPENVZ
         if (settings->flags & PROCESS_FLAG_LINUX_OPENVZ) {
            LinuxProcessList_readOpenVZData(lp, dirname, name);
         }
         #endif

         #ifdef HAVE_VSERVER
         if (settings->flags & PROCESS_FLAG_LINUX_VSERVER) {
            LinuxProcessList_readVServerData(lp, dirname, name);
         }
         #endif

         if (! LinuxProcessList_readCmdlineFile(proc, dirname, name)) {
            goto errorReadingProcess;
         }

         Process_fillStarttimeBuffer(proc);

         ProcessList_add(pl, proc);
      } else {
         if (settings->updateProcessNames && proc->state != 'Z') {
            if (! LinuxProcessList_readCmdlineFile(proc, dirname, name)) {
               goto errorReadingProcess;
            }
         }
      }
      /* (Re)Generate the Command string, but only if the process is:
       * - not a kernel thread, and
       * - not a zombie or it became zombie under htop's watch, and
       * - not a user thread or if showThreadNames is not set */
      if (!Process_isKernelThread(proc) &&
          (proc->state != 'Z' || lp->mergedCommand.str) &&
          (!Process_isUserlandThread(proc) || !settings->showThreadNames)) {
         LinuxProcess_makeCommandStr(proc);
      }

      #ifdef HAVE_DELAYACCT
      LinuxProcessList_readDelayAcctData(this, lp);
      #endif

      #ifdef HAVE_CGROUP
      if (settings->flags & PROCESS_FLAG_LINUX_CGROUP) {
         LinuxProcessList_readCGroupFile(lp, dirname, name);
      }
      #endif

      if (settings->flags & PROCESS_FLAG_LINUX_OOM) {
         LinuxProcessList_readOomData(lp, dirname, name);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_CTXT) {
         LinuxProcessList_readCtxtData(lp, dirname, name);
      }

      if (settings->flags & PROCESS_FLAG_LINUX_SECATTR) {
         LinuxProcessList_readSecattrData(lp, dirname, name);
      }

      if (proc->state == 'Z' && (proc->basenameOffset == 0)) {
         proc->basenameOffset = -1;
         setCommand(proc, command, commLen);
      } else if (Process_isThread(proc)) {
         if (settings->showThreadNames || Process_isKernelThread(proc) || (proc->state == 'Z' && proc->basenameOffset == 0)) {
            proc->basenameOffset = -1;
            setCommand(proc, command, commLen);
         } else if (settings->showThreadNames) {
            if (! LinuxProcessList_readCmdlineFile(proc, dirname, name)) {
               goto errorReadingProcess;
            }
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
      if (proc->state == 'R')
         pl->runningTasks++;
      proc->updated = true;
      continue;

      // Exception handler.

errorReadingProcess:
      {
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
   unsigned long long int swapFree = 0;
   unsigned long long int shmem = 0;
   unsigned long long int sreclaimable = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCMEMINFOFILE);
   }
   char buffer[128];
   while (fgets(buffer, 128, file)) {

      #define tryRead(label, variable)                                         \
         if (String_startsWith(buffer, label)) {                               \
            sscanf(buffer + strlen(label), " %32llu kB", variable);            \
            break;                                                             \
         }

      switch (buffer[0]) {
      case 'M':
         tryRead("MemTotal:", &this->totalMem);
         tryRead("MemFree:", &this->freeMem);
         tryRead("MemShared:", &this->sharedMem);
         break;
      case 'B':
         tryRead("Buffers:", &this->buffersMem);
         break;
      case 'C':
         tryRead("Cached:", &this->cachedMem);
         break;
      case 'S':
         switch (buffer[1]) {
         case 'w':
            tryRead("SwapTotal:", &this->totalSwap);
            tryRead("SwapFree:", &swapFree);
            break;
         case 'h':
            tryRead("Shmem:", &shmem);
            break;
         case 'R':
            tryRead("SReclaimable:", &sreclaimable);
            break;
         }
         break;
      }
      #undef tryRead
   }

   this->usedMem = this->totalMem - this->freeMem;
   this->cachedMem = this->cachedMem + sreclaimable - shmem;
   this->usedSwap = this->totalSwap - swapFree;
   fclose(file);
}

static inline void LinuxProcessList_scanZramInfo(LinuxProcessList* this) {
   unsigned long long int totalZram = 0;
   unsigned long long int usedZramComp = 0;
   unsigned long long int usedZramOrig = 0;

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
      unsigned long long int size = 0;
      unsigned long long int orig_data_size = 0;
      unsigned long long int compr_data_size = 0;

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
   unsigned long long int dbufSize = 0;
   unsigned long long int dnodeSize = 0;
   unsigned long long int bonusSize = 0;

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

static inline double LinuxProcessList_scanCPUTime(LinuxProcessList* this) {

   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE);
   }
   int cpus = this->super.cpuCount;
   assert(cpus > 0);
   for (int i = 0; i <= cpus; i++) {
      char buffer[PROC_LINE_LENGTH + 1];
      unsigned long long int usertime, nicetime, systemtime, idletime;
      unsigned long long int ioWait, irq, softIrq, steal, guest, guestnice;
      ioWait = irq = softIrq = steal = guest = guestnice = 0;
      // Depending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      char* ok = fgets(buffer, PROC_LINE_LENGTH, file);
      if (!ok) {
         buffer[0] = '\0';
      }

      if (i == 0) {
         (void) sscanf(buffer,   "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",         &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
      } else {
         int cpuid;
         (void) sscanf(buffer, "cpu%4d %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         assert(cpuid == i - 1);
      }
      // Guest time is already accounted in usertime
      usertime = usertime - guest;
      nicetime = nicetime - guestnice;
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
   fclose(file);
   return period;
}

static int scanCPUFreqencyFromSysCPUFreq(LinuxProcessList* this) {
   int cpus = this->super.cpuCount;
   int numCPUsWithFrequency = 0;
   unsigned long totalFrequency = 0;

   for (int i = 0; i < cpus; ++i) {
      char pathBuffer[64];
      xSnprintf(pathBuffer, sizeof(pathBuffer), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);

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
   }

   if (numCPUsWithFrequency > 0)
      this->cpus[0].frequency = (double)totalFrequency / numCPUsWithFrequency;

   return 0;
}

static void scanCPUFreqencyFromCPUinfo(LinuxProcessList* this) {
   FILE* file = fopen(PROCCPUINFOFILE, "r");
   if (file == NULL)
      return;

   int cpus = this->super.cpuCount;
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
         (sscanf(buffer, "cpu MHz: %lf", &frequency) == 1)
      ) {
         if (cpuid < 0 || cpuid > (cpus - 1)) {
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
   int cpus = this->super.cpuCount;
   assert(cpus > 0);

   for (int i = 0; i <= cpus; i++) {
      this->cpus[i].frequency = NAN;
   }

   if (scanCPUFreqencyFromSysCPUFreq(this) == 0) {
      return;
   }

   scanCPUFreqencyFromCPUinfo(this);
}

#ifdef HAVE_LIBSENSORS
static int getCPUTemperatures(CPUData* cpus, int cpuCount) {
   int tempCount = 0;

   int n = 0;
   for (const sensors_chip_name *chip = sensors_get_detected_chips(NULL, &n); chip; chip = sensors_get_detected_chips(NULL, &n)) {
      char buffer[32];
      sensors_snprintf_chip_name(buffer, sizeof(buffer), chip);
      if (!String_startsWith(buffer, "coretemp") && !String_startsWith(buffer, "cpu_thermal"))
         continue;

      int m = 0;
      for (const sensors_feature *feature = sensors_get_features(chip, &m); feature; feature = sensors_get_features(chip, &m)) {
         if (feature->type != SENSORS_FEATURE_TEMP)
            continue;

         if (feature->number > cpuCount)
            continue;

         const sensors_subfeature *sub_feature = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
         if (sub_feature) {
            double temp;
            int r = sensors_get_value(chip, sub_feature->number, &temp);
            if (r != 0)
               continue;

            cpus[feature->number].temperature = temp;
            tempCount++;
         }
      }
   }

   return tempCount;
}

static void LinuxProcessList_scanCPUTemperature(LinuxProcessList* this) {
   const int cpuCount = this->super.cpuCount;

   for (int i = 0; i <= cpuCount; i++) {
      this->cpus[i].temperature = NAN;
   }

   int r = getCPUTemperatures(this->cpus, cpuCount);

   /* No temperature - nothing to do */
   if (r == 0)
      return;

   /* Only package temperature - copy to all cpus */
   if (r == 1 && !isnan(this->cpus[0].temperature)) {
      double packageTemp = this->cpus[0].temperature;
      for (int i = 1; i <= cpuCount; i++) {
         this->cpus[i].temperature = packageTemp;
      }

      return;
   }

   /* Half the temperatures, probably HT/SMT - copy to second half */
   if (r >= 2 && (r - 1) == (cpuCount / 2)) {
      for (int i = cpuCount / 2 + 1; i <= cpuCount; i++) {
         this->cpus[i].temperature = this->cpus[i/2].temperature;
      }

      return;
   }
}
#endif

void ProcessList_goThroughEntries(ProcessList* super, bool pauseProcessUpdate) {
   LinuxProcessList* this = (LinuxProcessList*) super;
   const Settings* settings = super->settings;

   LinuxProcessList_scanMemoryInfo(super);
   LinuxProcessList_scanZfsArcstats(this);
   LinuxProcessList_updateCPUcount(this);
   LinuxProcessList_scanZramInfo(this);

   double period = LinuxProcessList_scanCPUTime(this);

   if (settings->showCPUFrequency) {
      LinuxProcessList_scanCPUFrequency(this);
   }

   #ifdef HAVE_LIBSENSORS
   if (settings->showCPUTemperature)
      LinuxProcessList_scanCPUTemperature(this);
   #endif

   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseProcessUpdate) {
      return;
   }

   struct timeval tv;
   gettimeofday(&tv, NULL);
   LinuxProcessList_recurseProcTree(this, PROCDIR, NULL, period, tv);
}
