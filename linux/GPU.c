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

#include "XUtils.h"

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

static void update_machine_gpu(LinuxProcessTable* lpt, unsigned long long int time, const char* engine, size_t engine_len) {
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
         .prevTime = 0,
         .curTime  = 0,
         .key      = xStrndup(engine, engine_len),
         .next     = NULL,
      };

      *engineData = newData;
   }

   (*engineData)->curTime += time;
   lhost->curGpuTime += time;
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
      ssize_t ret = xReadfileat(dirfd(fdinfoDir), ename, buffer, sizeof(buffer));
#else
      ssize_t ret = xReadfileat(fdinfoPathBuf, ename, buffer, sizeof(buffer));
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

            const char* engineStart = line + strlen("engine-");

            if (String_startsWith(engineStart, "capacity-"))
               continue;

            const char* delim = strchr(line, ':');

            char* endptr;
            errno = 0;
            unsigned long long int value = strtoull(delim + 1, &endptr, 10);
            if (errno == 0 && String_startsWith(endptr, " ns")) {
               if (sstate == SECST_UNKNOWN) {
                  if (client_id != INVALID_CLIENT_ID && !is_duplicate_client(parsed_ids, client_id, pdev))
                     sstate = SECST_NEW;
                  else
                     sstate = SECST_DUPLICATE;
               }

               if (sstate == SECST_NEW) {
                  new_gpu_time += value;
                  update_machine_gpu(lpt, value, engineStart, delim - engineStart);
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

   if (new_gpu_time > 0) {
      unsigned long long int gputimeDelta;
      uint64_t monotonicTimeDelta;

      Row_updateFieldWidth(GPU_TIME, ceil(log10(new_gpu_time)));

      gputimeDelta = saturatingSub(new_gpu_time, lp->gpu_time);
      monotonicTimeDelta = host->monotonicMs - host->prevMonotonicMs;
      lp->gpu_percent = 100.0F * gputimeDelta / (1000 * 1000) / monotonicTimeDelta;

      lp->gpu_activityMs = 0;
   } else
      lp->gpu_percent = 0.0F;

out:

   lp->gpu_time = new_gpu_time;

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
