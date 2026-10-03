/*
htop - GPU.c
(C) 2023 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/GPU.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#include "linux/Compat.h"
#include "linux/LinuxMachine.h"


typedef unsigned long long int ClientID;
#define INVALID_CLIENT_ID ((ClientID)-1)


typedef struct ClientInfo_ {
   char* pdev;
   ClientID id;
   struct ClientInfo_* next;
} ClientInfo;

enum section_state {
   SECST_UNKNOWN,
   SECST_DUPLICATE,
   SECST_NEW,
};

static bool is_duplicate_client(const ClientInfo* parsed, ClientID id, const char* pdev) {
   for (; parsed; parsed = parsed->next) {
      if (id == parsed->id && String_eq_nullable(pdev, parsed->pdev)) {
         return true;
      }
   }

   return false;
}

static GPUEngineData* get_machine_gpu_engine(LinuxProcessTable* lpt, const char* engine, size_t engine_len) {
   Machine* host = lpt->super.super.host;
   LinuxMachine* lhost = (LinuxMachine*) host;
   GPUEngineData** engineData = &lhost->gpuEngineData;

   while (*engineData) {
      if (strncmp((*engineData)->key, engine, engine_len) == 0 && (*engineData)->key[engine_len] == '\0')
         break;

      engineData = &((*engineData)->next);
   }

   if (!*engineData) {
      GPUEngineData* newData = xMalloc(sizeof(*newData));
      *newData = (GPUEngineData) {
         .prevTime        = 0,
         .curTime         = 0,
         .prevCycles      = 0,
         .curCycles       = 0,
         .prevTotalCycles = 0,
         .curTotalCycles  = 0,
         .key             = xStrndup(engine, engine_len),
         .next            = NULL,
      };

      *engineData = newData;
   }

   return *engineData;
}

static void update_machine_gpu(LinuxProcessTable* lpt, unsigned long long int time, const char* engine, size_t engine_len) {
   LinuxMachine* lhost = (LinuxMachine*) lpt->super.super.host;

   get_machine_gpu_engine(lpt, engine, engine_len)->curTime += time;
   lhost->curGpuTime += time;
}

static void update_machine_gpu_cycles(LinuxProcessTable* lpt, unsigned long long int cycles, const char* engine, size_t engine_len) {
   get_machine_gpu_engine(lpt, engine, engine_len)->curCycles += cycles;
}

/* drm-total-cycles-* is a device global counter, all clients of a device
 * report the same value, so aggregate by taking the maximum. */
static void update_machine_gpu_total_cycles(LinuxProcessTable* lpt, unsigned long long int totalCycles, const char* engine, size_t engine_len) {
   GPUEngineData* engineData = get_machine_gpu_engine(lpt, engine, engine_len);

   if (totalCycles > engineData->curTotalCycles)
      engineData->curTotalCycles = totalCycles;
}

static bool count_section(enum section_state* sstate, ClientID client_id, const char* pdev, const ClientInfo* parsed_ids) {
   if (*sstate == SECST_UNKNOWN) {
      if (client_id != INVALID_CLIENT_ID && !is_duplicate_client(parsed_ids, client_id, pdev))
         *sstate = SECST_NEW;
      else
         *sstate = SECST_DUPLICATE;
   }

   return *sstate == SECST_NEW;
}

/*
 * Parses a "<prefix><engine>: <value><unit>" line, e.g. "engine-rcs: 1234 ns".
 * An empty unit requires the value to be the last item on the line.
 */
static bool parse_engine_value(const char* line, const char* prefix, const char* unit,
                               const char** engine, size_t* engine_len, unsigned long long int* value) {
   const char* engineStart = line + strlen(prefix);

   const char* delim = strchr(engineStart, ':');
   if (!delim)
      return false;

   const char* numStart = delim + 1;
   while (isspace((unsigned char)*numStart))
      numStart++;

   /* strtoull() would accept a sign and wrap the result around */
   if (!isdigit((unsigned char)*numStart))
      return false;

   char* endptr;
   errno = 0;
   unsigned long long int parsed = strtoull(numStart, &endptr, 10);
   if (errno != 0)
      return false;

   if (unit[0] ? !String_startsWith(endptr, unit) : *endptr != '\0')
      return false;

   *engine = engineStart;
   *engine_len = delim - engineStart;
   *value = parsed;
   return true;
}

/*
 * Documentation reference:
 * https://www.kernel.org/doc/html/latest/gpu/drm-usage-stats.html
 */
void GPU_readProcessData(LinuxProcessTable* lpt, LinuxProcess* lp, openat_arg_t procFd) {
   const Machine* host = lp->super.super.host;
   int fdinfoFd = -1;
   DIR* fdinfoDir = NULL;
   ClientInfo* parsed_ids = NULL;
   unsigned long long int new_gpu_time = 0;
   unsigned long long int new_gpu_cycles = 0;
   unsigned long long int new_gpu_totalCycles = 0;

   /* check only if active in last check or last scan was more than 5s ago */
   if (lp->gpu_activityMs != 0 && host->monotonicMs - lp->gpu_activityMs < 5000) {
      lp->gpu_percent = 0.0F;
      return;
   }
   lp->gpu_activityMs = host->monotonicMs;

   fdinfoFd = Compat_openat(procFd, "fdinfo", O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC);
   if (fdinfoFd == -1)
      goto out;

   fdinfoDir = fdopendir(fdinfoFd);
   if (!fdinfoDir)
      goto out;
   fdinfoFd = -1;

#ifndef HAVE_OPENAT
   char fdinfoPathBuf[32];
   xSnprintf(fdinfoPathBuf, sizeof(fdinfoPathBuf), PROCDIR "/%u/fdinfo", Process_getPid(&lp->super));
#endif

   while (true) {
      char* pdev = NULL;
      ClientID client_id = INVALID_CLIENT_ID;
      enum section_state sstate = SECST_UNKNOWN;

      const struct dirent* entry = readdir(fdinfoDir);
      if (!entry)
         break;
      const char* ename = entry->d_name;

      if (ename[0] == '.' && (ename[1] == '\0' || (ename[1] == '.' && ename[2] == '\0')))
         continue;

      char buffer[4096];
#ifdef HAVE_OPENAT
      ssize_t ret = Compat_readfileat(dirfd(fdinfoDir), ename, buffer, sizeof(buffer));
#else
      ssize_t ret = Compat_readfileat(fdinfoPathBuf, ename, buffer, sizeof(buffer));
#endif
      /* eventfd information can be huge */
      if (ret <= 0 || (size_t)ret >= sizeof(buffer) - 1)
         continue;

      char* buf = buffer;
      const char* line;
      while ((line = strsep(&buf, "\n")) != NULL) {
         if (!String_startsWith(line, "drm-"))
            continue;
         line += strlen("drm-");

         if (line[0] == 'c' && String_startsWith(line, "client-id:")) {
            if (sstate == SECST_NEW) {
               assert(client_id != INVALID_CLIENT_ID);

               ClientInfo* new = xMalloc(sizeof(*new));
               *new = (ClientInfo) {
                  .id = client_id,
                  .pdev = pdev,
                  .next = parsed_ids,
               };
               pdev = NULL;

               parsed_ids = new;
            }

            sstate = SECST_UNKNOWN;

            char *endptr;
            errno = 0;
            client_id = strtoull(line + strlen("client-id:"), &endptr, 10);
            if (errno || *endptr != '\0')
               client_id = INVALID_CLIENT_ID;
         } else if (line[0] == 'p' && String_startsWith(line, "pdev:")) {
            const char* p = line + strlen("pdev:");

            while (isspace((unsigned char)*p))
               p++;

            assert(!pdev || String_eq(pdev, p));
            if (!pdev)
               pdev = xStrdup(p);
         } else if (line[0] == 'e' && String_startsWith(line, "engine-")) {
            if (sstate == SECST_DUPLICATE)
               continue;

            if (String_startsWith(line + strlen("engine-"), "capacity-"))
               continue;

            const char* engine;
            size_t engine_len;
            unsigned long long int value;
            if (parse_engine_value(line, "engine-", " ns", &engine, &engine_len, &value)) {
               if (count_section(&sstate, client_id, pdev, parsed_ids)) {
                  new_gpu_time += value;
                  update_machine_gpu(lpt, value, engine, engine_len);
               }
            }
         } else if (line[0] == 'c' && String_startsWith(line, "cycles-")) {
            /* Drivers that cannot provide a nanosecond resolution timestamp
             * (e.g. Intel Xe) export the busy cycles of an engine together with
             * the cycles elapsed on that engine. */
            if (sstate == SECST_DUPLICATE)
               continue;

            const char* engine;
            size_t engine_len;
            unsigned long long int value;
            if (parse_engine_value(line, "cycles-", "", &engine, &engine_len, &value)) {
               if (count_section(&sstate, client_id, pdev, parsed_ids)) {
                  new_gpu_cycles += value;
                  update_machine_gpu_cycles(lpt, value, engine, engine_len);
               }
            }
         } else if (line[0] == 't' && String_startsWith(line, "total-cycles-")) {
            if (sstate == SECST_DUPLICATE)
               continue;

            const char* engine;
            size_t engine_len;
            unsigned long long int value;
            if (parse_engine_value(line, "total-cycles-", "", &engine, &engine_len, &value)) {
               if (count_section(&sstate, client_id, pdev, parsed_ids)) {
                  /* The same free running counter is reported for all engines
                   * of a device, so don't accumulate it. */
                  if (value > new_gpu_totalCycles)
                     new_gpu_totalCycles = value;
                  update_machine_gpu_total_cycles(lpt, value, engine, engine_len);
               }
            }
         }
      } /* finished parsing lines */

      if (sstate == SECST_NEW) {
         assert(client_id != INVALID_CLIENT_ID);

         ClientInfo* new = xMalloc(sizeof(*new));
         *new = (ClientInfo) {
            .id = client_id,
            .pdev = pdev,
            .next = parsed_ids,
         };
         pdev = NULL;

         parsed_ids = new;
      }

      free(pdev);
   } /* finished parsing fdinfo entries */

   {
      uint64_t monotonicTimeDelta = host->monotonicMs - host->prevMonotonicMs;
      unsigned long long int gputimeDelta = saturatingSub(new_gpu_time, lp->gpu_timeRaw);

      /* Cycle based accounting only yields a ratio of busy to elapsed cycles,
       * which is turned into a busy time using the sampling interval. */
      unsigned long long int cyclesDelta = saturatingSub(new_gpu_cycles, lp->gpu_cycles);
      unsigned long long int totalCyclesDelta = lp->gpu_totalCycles ? saturatingSub(new_gpu_totalCycles, lp->gpu_totalCycles) : 0;
      if (cyclesDelta > 0 && totalCyclesDelta > 0)
         gputimeDelta += (unsigned long long int)((double)cyclesDelta / totalCyclesDelta * monotonicTimeDelta * (1000 * 1000));

      if (gputimeDelta > 0 && monotonicTimeDelta > 0) {
         lp->gpu_time += gputimeDelta;
         lp->gpu_percent = 100.0F * gputimeDelta / (1000 * 1000) / monotonicTimeDelta;
      } else {
         lp->gpu_percent = 0.0F;
      }

      /* Keep visiting a process as long as it holds a counter, even while it is
       * idle: the machine wide totals are summed up from the counters of the
       * processes seen in this pass, so skipping one makes the sum drop. */
      if (new_gpu_time > 0 || new_gpu_cycles > 0)
         lp->gpu_activityMs = 0;

      lp->gpu_timeRaw = new_gpu_time;
      lp->gpu_cycles = new_gpu_cycles;
      lp->gpu_totalCycles = new_gpu_totalCycles;
   }

   goto cleanup;

out:
   /* Hold on to the counters of the last successful read: failing to look at a
    * process is not the same as it having released the GPU, and starting over
    * from zero would account for the whole counter a second time. */
   lp->gpu_percent = 0.0F;

cleanup:

   while (parsed_ids) {
      ClientInfo* next = parsed_ids->next;
      free(parsed_ids->pdev);
      free(parsed_ids);
      parsed_ids = next;
   }

   if (fdinfoDir)
      closedir(fdinfoDir);
   if (fdinfoFd != -1)
      close(fdinfoFd);
}
