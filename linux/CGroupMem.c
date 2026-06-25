/*
htop - CGroupMem.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/CGroupMem.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Macros.h"
#include "XUtils.h"


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
