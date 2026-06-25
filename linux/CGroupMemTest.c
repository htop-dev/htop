/*
htop - CGroupMemTest.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/CGroupMem.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>


/* XUtils' allocation-failure path references CRT_done(); this test links XUtils.o
   but never triggers OOM, so a no-op stub satisfies the linker without pulling in
   CRT and the whole TUI. */
void CRT_done(void);
void CRT_done(void) { }

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                                          \
   do {                                                                      \
      checks++;                                                              \
      if (!(cond)) {                                                         \
         failures++;                                                         \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      }                                                                      \
   } while (0)


static void test_parseLimit(void) {
   uint64_t kb = 999;
   CHECK(CGroupMem_parseLimit("max\n", &kb) == false);
   CHECK(CGroupMem_parseLimit("", &kb) == false);
   CHECK(CGroupMem_parseLimit("4294967296\n", &kb) == true);
   CHECK(kb == 4194304ULL); /* 4 GiB / 1024 */
}

static void test_parseUsage(void) {
   uint64_t kb = 0;
   CHECK(CGroupMem_parseUsage("2202009600\n", &kb) == true);
   CHECK(kb == 2150400ULL); /* 2202009600 / 1024 */
   CHECK(CGroupMem_parseUsage("garbage\n", &kb) == false);
}

static void test_parseStat(void) {
   const char* stat =
      "anon 1048576\n"
      "file 524288000\n"
      "kernel_stack 16384\n"
      "file_mapped 100\n";
   uint64_t kb = 0;
   CHECK(CGroupMem_parseStat(stat, "file", &kb) == true);
   CHECK(kb == 512000ULL); /* 524288000 / 1024 */
   CHECK(CGroupMem_parseStat(stat, "anon", &kb) == true);
   CHECK(kb == 1024ULL);
   /* must not match the "file_mapped" prefix when asked for "file" — exact token only */
   CHECK(CGroupMem_parseStat(stat, "slab", &kb) == false);
}

static void test_parseSelfCgroup(void) {
   char buf[256];
   /* namespaced container: path is the namespace root */
   CHECK(CGroupMem_parseSelfCgroup("0::/\n", buf, sizeof(buf)) == true);
   CHECK(buf[0] == '/' && buf[1] == '\0');
   /* hybrid: v1 controller lines then a v2 line */
   CHECK(CGroupMem_parseSelfCgroup("4:memory:/foo\n0::/bar/baz\n", buf, sizeof(buf)) == true);
   CHECK(CGroupMem_parseSelfCgroup("0::/bar/baz\n", buf, sizeof(buf)) == true);
   /* compare the second result */
   CHECK(buf[0] == '/' && buf[1] == 'b' && buf[2] == 'a' && buf[3] == 'r');
   /* pure v1: no 0:: line */
   CHECK(CGroupMem_parseSelfCgroup("4:memory:/foo\n", buf, sizeof(buf)) == false);
}

static void test_parseMountinfo(void) {
   char buf[256];
   const char* mi =
      "23 28 0:22 / /proc rw,nosuid - proc proc rw\n"
      "29 23 0:25 / /sys/fs/cgroup ro,nosuid,nodev,noexec - cgroup2 cgroup2 rw,nsdelegate\n";
   CHECK(CGroupMem_parseMountinfo(mi, buf, sizeof(buf)) == true);
   CHECK(strcmp(buf, "/sys/fs/cgroup") == 0);
   /* no cgroup2 mount → false */
   const char* none = "23 28 0:22 / /proc rw - proc proc rw\n";
   CHECK(CGroupMem_parseMountinfo(none, buf, sizeof(buf)) == false);
}

int main(void) {
   test_parseLimit();
   test_parseUsage();
   test_parseStat();
   test_parseSelfCgroup();
   test_parseMountinfo();

   if (failures) {
      fprintf(stderr, "%d/%d checks FAILED\n", failures, checks);
      return 1;
   }
   printf("All %d checks passed\n", checks);
   return 0;
}
