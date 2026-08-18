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


/* All byte counts are stored in kB (bytes / 1024) to match htop's memory_t convention. */
typedef struct CGroupMemData_ {
   bool active;               /* a finite memory.max < host total was found */
   bool swapActive;           /* a finite memory.swap.max was found */
   uint64_t limit;            /* memory.max, kB */
   uint64_t current;          /* memory.current, kB */
   uint64_t file;             /* memory.stat 'file', kB */
   uint64_t swapLimit;        /* memory.swap.max, kB (may be 0 = swap disabled) */
   uint64_t swapCurrent;      /* memory.swap.current, kB */
} CGroupMemData;

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

/* Read the cgroup files under nodeDir and fill `data`. hostTotalMemKB gates activation
   (active only when a finite limit < host total). Tolerant of missing files. */
void CGroupMem_readNode(const char* nodeDir, uint64_t hostTotalMemKB, CGroupMemData* data);

/* Resolve (and cache) our cgroup v2 node, then read it. Fills data->active = false on any failure. */
void CGroupMem_scan(uint64_t hostTotalMemKB, CGroupMemData* data);

#endif /* HEADER_CGroupMem */
