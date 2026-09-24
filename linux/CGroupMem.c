/*
htop - CGroupMem.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/CGroupMem.h"

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Macros.h"
#include "XUtils.h"

#include "linux/Compat.h"


bool CGroupMem_parseUsage(const char* content, uint64_t* outKB) {
   if (!content)
      return false;

   char* end;
   unsigned long long bytes = strtoull(content, &end, 10);
   if (end == content)
      return false;

   *outKB = (uint64_t)(bytes / 1024);
   return true;
}

bool CGroupMem_parseLimit(const char* content, uint64_t* outKB) {
   if (!content)
      return false;

   while (*content == ' ' || *content == '\t')
      content++;

   /* "max" means unlimited; otherwise it is a plain byte count like a usage file. */
   if (String_startsWith(content, "max"))
      return false;

   return CGroupMem_parseUsage(content, outKB);
}

bool CGroupMem_parseStat(const char* content, const char* key, uint64_t* outKB) {
   if (!content || !key)
      return false;

   size_t keyLen = strlen(key);
   const char* p = content;
   while (*p) {
      const char* eol = String_strchrnul(p, '\n');

      /* match "key " at the start of the line (exact token, space-delimited) */
      if (String_startsWith(p, key) && p[keyLen] == ' ') {
         char* end;
         unsigned long long bytes = strtoull(p + keyLen + 1, &end, 10);
         if (end != p + keyLen + 1) {
            *outKB = (uint64_t)(bytes / 1024);
            return true;
         }
      }

      if (*eol == '\0')
         break;
      p = eol + 1;
   }
   return false;
}

bool CGroupMem_parseMountinfo(const char* content, char* mountBuf, size_t mountSize) {
   if (!content || mountSize == 0)
      return false;

   const char* p = content;
   while (*p) {
      const char* eol = String_strchrnul(p, '\n');

      /* the filesystem type is the first token after the " - " separator */
      const char* sep = strstr(p, " - ");
      if (sep && sep < eol) {
         const char* fstype = sep + 3;
         if (String_startsWith(fstype, "cgroup2") && (fstype[7] == ' ' || fstype[7] == '\t')) {
            /* mount point is the 5th space-separated field (index 4) before the separator */
            const char* q = p;
            int field = 0;
            while (field < 4 && q < sep) {
               q = String_strchrnul(q, ' ');
               if (q >= sep)
                  break;
               q++;
               field++;
            }
            if (field == 4 && q < sep) {
               const char* mpEnd = String_strchrnul(q, ' ');
               if (mpEnd <= sep) {
                  size_t mpLen = (size_t)(mpEnd - q);
                  String_safeStrncpy(mountBuf, q, MINIMUM(mpLen + 1, mountSize));
                  return true;
               }
            }
         }
      }

      if (*eol == '\0')
         break;
      p = eol + 1;
   }
   return false;
}

bool CGroupMem_parseSelfCgroup(const char* content, char* pathBuf, size_t bufSize) {
   if (!content || bufSize == 0)
      return false;

   const char* p = content;
   while (*p) {
      const char* eol = String_strchrnul(p, '\n');

      if (String_startsWith(p, "0::")) {
         const char* path = p + 3;
         size_t pathLen = (size_t)(eol - path);
         String_safeStrncpy(pathBuf, path, MINIMUM(pathLen + 1, bufSize));
         return true;
      }

      if (*eol == '\0')
         break;
      p = eol + 1;
   }
   return false;
}

void CGroupMem_readNode(const char* nodeDir, uint64_t hostTotalMemKB, CGroupMemData* data) {
   char buf[4096];

   data->active = false;
   data->swapActive = false;
   data->limit = data->current = data->file = 0;
   data->swapLimit = data->swapCurrent = 0;

   int dirFd = Compat_openat(AT_FDCWD, nodeDir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
   if (dirFd < 0)
      return;

   if (Compat_readfileat(dirFd, "memory.max", buf, sizeof(buf)) > 0) {
      uint64_t kb;
      if (CGroupMem_parseLimit(buf, &kb) && kb > 0 && kb < hostTotalMemKB) {
         data->limit = kb;
         data->active = true;
      }
   }

   if (data->active) {
      if (Compat_readfileat(dirFd, "memory.current", buf, sizeof(buf)) > 0)
         CGroupMem_parseUsage(buf, &data->current);
      if (Compat_readfileat(dirFd, "memory.stat", buf, sizeof(buf)) > 0)
         CGroupMem_parseStat(buf, "file", &data->file);
   }

   /* Only enter swap cgroup mode when memory is also limited, so the Memory and
      Swap meters stay consistent (a finite memory.swap.max with memory.max=max
      must not flip Swap to cgroup mode while Memory stays on host totals). */
   if (data->active && Compat_readfileat(dirFd, "memory.swap.max", buf, sizeof(buf)) > 0) {
      uint64_t kb;
      if (CGroupMem_parseLimit(buf, &kb)) {
         data->swapLimit = kb;
         data->swapActive = true;
         if (Compat_readfileat(dirFd, "memory.swap.current", buf, sizeof(buf)) > 0)
            CGroupMem_parseUsage(buf, &data->swapCurrent);
      }
   }

   close(dirFd);
}

/* Resolve our own cgroup v2 node directory: the cgroup2 mount point joined with the
   "0::" path from /proc/self/cgroup ("/" = the mount point itself). false if not v2. */
static bool CGroupMem_resolveNodeDir(char* nodeDir, size_t size) {
   char fileBuf[8192];
   char mountBuf[PATH_MAX];
   char cgPath[PATH_MAX];

   int selfFd = Compat_openat(AT_FDCWD, "/proc/self", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
   if (selfFd < 0)
      return false;

   bool ok = false;
   if (Compat_readfileat(selfFd, "mountinfo", fileBuf, sizeof(fileBuf)) > 0 &&
       CGroupMem_parseMountinfo(fileBuf, mountBuf, sizeof(mountBuf)) &&
       Compat_readfileat(selfFd, "cgroup", fileBuf, sizeof(fileBuf)) > 0 &&
       CGroupMem_parseSelfCgroup(fileBuf, cgPath, sizeof(cgPath))) {
      /* node dir = mount point + cgroup path; "0::/" → the mount point itself. Plain
         snprintf (not xSnprintf) so an over-long cgroup path falls back to host mode
         gracefully instead of aborting htop. */
      int n = (cgPath[0] == '/' && cgPath[1] == '\0')
         ? snprintf(nodeDir, size, "%s", mountBuf)
         : snprintf(nodeDir, size, "%s%s", mountBuf, cgPath);
      ok = (n > 0 && (size_t)n < size);
   }

   close(selfFd);
   return ok;
}

void CGroupMem_scan(uint64_t hostTotalMemKB, CGroupMemData* data) {
   static bool resolved = false;
   static bool available = false;
   static char nodeDir[PATH_MAX];

   if (!resolved) {
      resolved = true;
      available = CGroupMem_resolveNodeDir(nodeDir, sizeof(nodeDir));
   }

   if (!available) {
      data->active = false;
      data->swapActive = false;
      return;
   }

   CGroupMem_readNode(nodeDir, hostTotalMemKB, data);
}
