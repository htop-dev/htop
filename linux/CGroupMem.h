#ifndef HEADER_CGroupMem
#define HEADER_CGroupMem
/*
htop - CGroupMem.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/* Pure parsers (testable with string inputs). */

/* Parse a cgroup limit file body ("max\n" or "<bytes>\n"). Returns false for
   "max"/empty/invalid (unlimited or unreadable); true with *outKB set otherwise. */
bool CGroupMem_parseLimit(const char* content, uint64_t* outKB);

/* Parse a cgroup usage file body ("<bytes>\n"). false if not a number. */
bool CGroupMem_parseUsage(const char* content, uint64_t* outKB);

/* Parse memory.stat body for `key` (e.g. "file"); value is bytes → kB. false if key absent. */
bool CGroupMem_parseStat(const char* content, const char* key, uint64_t* outKB);

/* Parse /proc/self/cgroup body; copy the v2 ("0::") path into pathBuf. false if no 0:: line. */
bool CGroupMem_parseSelfCgroup(const char* content, char* pathBuf, size_t bufSize);

/* Parse /proc/self/mountinfo body; copy the cgroup2 mount point into mountBuf. false if none. */
bool CGroupMem_parseMountinfo(const char* content, char* mountBuf, size_t mountSize);

#endif /* HEADER_CGroupMem */
