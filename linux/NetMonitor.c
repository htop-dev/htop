/*
htop - linux/NetMonitor.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/NetMonitor.h"

#ifdef HAVE_EBPF_NET

#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "XUtils.h"
#include "linux/Platform.h"


/* eBPF object embedded into the binary by objcopy */
extern const unsigned char _binary_NetMonitor_bpf_o_start[];
extern const unsigned char _binary_NetMonitor_bpf_o_end[];

/* Shared lens with the kernel side of linux/NetMonitor.bpf.c */
typedef struct NetIOBpfCounts_ {
   uint64_t rxBytes;
   uint64_t txBytes;
   uint64_t rxPackets;
   uint64_t txPackets;
} NetIOBpfCounts;

/* libbpf function pointers, resolved via dlopen */
typedef void* (*BpfObjectOpenMemFn)(const void*, size_t, const void*);
typedef int (*BpfObjectLoadFn)(void*);
typedef void* (*BpfObjectNextProgramFn)(const void*, void*);
typedef void* (*BpfProgramAttachFn)(void*);
typedef const char* (*BpfProgramSectionFn)(const void*);
typedef int (*BpfLinkDestroyFn)(void*);
typedef void* (*BpfObjectFindMapByNameFn)(const void*, const char*);
typedef int (*BpfMapFdFn)(const void*);
typedef int (*BpfMapLookupElemFn)(int, const void*, void*);
typedef int (*BpfMapDeleteElemFn)(int, const void*);
typedef int (*BpfMapGetNextKeyFn)(int, const void*, void*);
typedef void (*BpfObjectCloseFn)(void*);
typedef long (*BpfGetErrorFn)(const void*);

static BpfObjectOpenMemFn sym_bpf_object__open_mem;
static BpfObjectLoadFn sym_bpf_object__load;
static BpfObjectNextProgramFn sym_bpf_object__next_program;
static BpfProgramAttachFn sym_bpf_program__attach;
static BpfProgramSectionFn sym_bpf_program__section_name;
static BpfLinkDestroyFn sym_bpf_link__destroy;
static BpfObjectFindMapByNameFn sym_bpf_object__find_map_by_name;
static BpfMapFdFn sym_bpf_map__fd;
static BpfMapLookupElemFn sym_bpf_map_lookup_elem;
static BpfMapDeleteElemFn sym_bpf_map_delete_elem;
static BpfMapGetNextKeyFn sym_bpf_map_get_next_key;
static BpfObjectCloseFn sym_bpf_object__close;
static BpfGetErrorFn sym_libbpf_get_error;

static void* dlopenHandle = NULL;
static const char* dlopenName = NULL;

/* libbpf_print_fn_t: int (*)(int level, const char* format, va_list args) */
typedef int (*LibbpfPrintFn)(int, const char*, va_list);
typedef LibbpfPrintFn (*LibbpfSetPrintFn)(LibbpfPrintFn);

static int NetMonitor_quietLibbpfPrint(int level, const char* format, va_list args) {
   (void) level;
   (void) format;
   (void) args;
   return 0;
}

/* Keep libbpf's own messages out of stderr; our own diagnostics
 * (when compiled in) tell the same story more concisely. */
static LibbpfSetPrintFn sym_libbpf_set_print;

/* In release (NDEBUG) builds diagnostic output is compiled out entirely */
static void NetMonitor_debug(const char* fmt, ...) ATTR_FORMAT(printf, 1, 2);
static void NetMonitor_debug(const char* fmt, ...) {
#ifdef NDEBUG
   (void) fmt;
#else
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, "htop-netmon: ");
   vfprintf(stderr, fmt, ap);
   va_end(ap);
#endif
}

/* When a probe fails to attach because its target symbol is absent, map the
 * probe back to the kernel range its symbol was introduced on. The per-family
 * ping senders were introduced in Linux 3.18 when IPv6 ping sockets landed;
 * ping_recvmsg() itself predates that split. */
#if !defined(NDEBUG)
static const char* NetMonitor_expectedMissing(const char* section) {
   static const struct {
      const char* section;
      const char* note;
   } expectedMissing[] = {
      { "kprobe/ping_v4_sendmsg", "kernel >= 3.18" },
      { "kprobe/ping_v6_sendmsg", "kernel >= 3.18" },
      { "kretprobe/ping_v4_sendmsg", "kernel >= 3.18" },
      { "kretprobe/ping_v6_sendmsg", "kernel >= 3.18" },
   };
   for (size_t i = 0; i < ARRAYSIZE(expectedMissing); i++)
      if (String_eq(section, expectedMissing[i].section))
         return expectedMissing[i].note;
   return NULL;
}
#endif

static bool eBPFLoadAttempted = false;
static bool eBPFActive = false;
static int netioMapFd = -1;

/* Interval (in scan cycles) between sweeps that drop stale map entries */
#define NETMONITOR_CLEANUP_INTERVAL 16

/* Upper bound on the number of eBPF programs attached (one per SEC section) */
#define NETMONITOR_MAX_PROGRAMS 24

/* Retained from the load attempt so NetMonitor_freeBPF() can tear the
 * object and every attached probe down; bpf_object__close() alone does not
 * destroy bpf_link instances. */
static void* bpfObj = NULL;
static void* bpfLinks[NETMONITOR_MAX_PROGRAMS];
static size_t nBpfLinks = 0;

/* dlopen "libbpf.so.1"/"libbpf.so.0", fall back to the unversioned name */
static bool NetMonitor_resolveLibbpf(void) {
   if (dlopenHandle)
      return true;

   static const char* const libbpf_candidates[] = {
      "libbpf.so.1",
      "libbpf.so.0",
      "libbpf.so",
      NULL,
   };
   const char* lastError = NULL;
   for (size_t i = 0; libbpf_candidates[i]; i++) {
      dlopenHandle = dlopen(libbpf_candidates[i], RTLD_LAZY);
      if (dlopenHandle) {
         dlopenName = libbpf_candidates[i];
         break;
      }
      lastError = dlerror();
   }
   if (!dlopenHandle) {
      char* tried = String_join(", ", libbpf_candidates);
      NetMonitor_debug("eBPF network monitoring needs the libbpf runtime library, none of %s could be loaded%s%s\n",
                       tried, lastError ? ": " : "", lastError ? lastError : "");
      free(tried);
      return false;
   }

   #define resolve(symbolname) do {                                        \
      dlerror();                                                           \
      *(void **)(&sym_##symbolname) = dlsym(dlopenHandle, #symbolname);    \
      if (!sym_##symbolname) {                                             \
         const char* e = dlerror();                                        \
         NetMonitor_debug("%s is missing symbol %s%s%s\n",                 \
                          dlopenName, #symbolname, e ? ": " : "", e ? e : ""); \
         dlclose(dlopenHandle);                                            \
         dlopenHandle = NULL;                                              \
         return false;                                                     \
      }                                                                    \
   } while (0)

   resolve(bpf_object__open_mem);
   resolve(bpf_object__load);
   resolve(bpf_object__next_program);
   resolve(bpf_program__attach);
   resolve(bpf_program__section_name);
   resolve(bpf_link__destroy);
   resolve(bpf_object__find_map_by_name);
   resolve(bpf_map__fd);
   resolve(bpf_map_lookup_elem);
   resolve(bpf_map_delete_elem);
   resolve(bpf_map_get_next_key);
   resolve(bpf_object__close);
   resolve(libbpf_get_error);

   #undef resolve

   dlerror();
   *((void **)(&sym_libbpf_set_print)) = dlsym(dlopenHandle, "libbpf_set_print");
   if (sym_libbpf_set_print)
      sym_libbpf_set_print(NetMonitor_quietLibbpfPrint);

   return true;
}

static void NetMonitor_freeBPF(void) {
   for (size_t i = 0; i < nBpfLinks; i++) {
      if (bpfLinks[i]) {
         sym_bpf_link__destroy(bpfLinks[i]);
         bpfLinks[i] = NULL;
      }
   }
   nBpfLinks = 0;
   if (bpfObj) {
      sym_bpf_object__close(bpfObj);
      bpfObj = NULL;
   }
   netioMapFd = -1;
   eBPFActive = false;
}

/* One line summarizing why the kernel refused to load our eBPF programs.
 * Numbers are the stable Linux capability ids for CAP_SYS_ADMIN(21) and
 * CAP_BPF(39). */
#ifndef NDEBUG
static void NetMonitor_reportLoadFailure(void) {
   unsigned long long capEff = 0;
   FILE* fp = fopen("/proc/self/status", "re");
   if (fp) {
      char* line;
      bool found = false;
      while (!found && (line = String_readLine(fp)) != NULL) {
         if (String_startsWith(line, "CapEff:")) {
            capEff = strtoull(line + strlen("CapEff:"), NULL, 16);
            found = true;
         }
         free(line);
      }
      fclose(fp);
   }

   char lockdown[64] = "unreadable";
   fp = fopen("/sys/kernel/security/lockdown", "r");
   if (fp) {
      char* line = String_readLine(fp);
      fclose(fp);
      if (line) {
         String_safeStrncpy(lockdown, line, sizeof(lockdown));
         free(line);
      }
   }

   char paranoid[32] = "unreadable";
   fp = fopen("/proc/sys/kernel/perf_event_paranoid", "r");
   if (fp) {
      char* line = String_readLine(fp);
      fclose(fp);
      if (line) {
         String_safeStrncpy(paranoid, line, sizeof(paranoid));
         free(line);
      }
   }

   NetMonitor_debug("CapEff=0x%llx (need CAP_BPF or CAP_SYS_ADMIN), lockdown=%s, perf_event_paranoid=%s\n",
                    capEff, lockdown, paranoid);
}
#endif

static bool NetMonitor_loadBPF(void) {
   if (!NetMonitor_resolveLibbpf())
      return false;

   size_t objSize = (size_t) (_binary_NetMonitor_bpf_o_end - _binary_NetMonitor_bpf_o_start);
   if (!objSize) {
      NetMonitor_debug("embedded eBPF object is empty\n");
      return false;
   }

   void* obj = sym_bpf_object__open_mem(_binary_NetMonitor_bpf_o_start, objSize, NULL);
   if (sym_libbpf_get_error(obj) != 0) {
      int e = errno;
      NetMonitor_debug("bpf_object__open_mem failed%s%s\n", e ? ": " : "", e ? strerror(e) : "");
      return false;
   }
   bpfObj = obj;

   if (sym_bpf_object__load(obj) != 0) {
      int e = errno;
      NetMonitor_debug("bpf_object__load failed (kernel refused the eBPF programs or maps%s%s)\n",
                       e ? ": " : "", e ? strerror(e) : "");
#ifndef NDEBUG
      if (e == EPERM)
         NetMonitor_reportLoadFailure();
#endif
      goto fail;
   }

/* Attach every program parsed from SEC().
 * A probe whose target symbol does not exist on this kernel (e.g.
 * ping_v6_sendmsg() without CONFIG_IPV6) is skipped rather than fatal;
 * the probes that did attach still account the traffic they see. */
   nBpfLinks = 0;
#if !defined(NDEBUG)
   struct NetMonitorAttachResult {
      const char* section;
      bool attached;
      int error;
   } results[NETMONITOR_MAX_PROGRAMS];
   size_t nResults = 0;
   bool anyNotAllowed = false;
#endif
   void* prog = NULL;
   while ((prog = sym_bpf_object__next_program(obj, prog)) != NULL) {
      void* link = sym_bpf_program__attach(prog);
      /* libbpf 0.x returns ERR_PTR for failures, 1.x returns NULL. */
      long linkErr = sym_libbpf_get_error(link);
#if !defined(NDEBUG)
      int error = linkErr ? (int) -linkErr : 0;
      if (nResults < NETMONITOR_MAX_PROGRAMS) {
         results[nResults].section = sym_bpf_program__section_name(prog);
         results[nResults].attached = !linkErr;
         results[nResults].error = error;
         nResults++;
      }
#endif
      if (linkErr) {
#if !defined(NDEBUG)
         if (error == EPERM)
            anyNotAllowed = true;
#endif
      } else if (nBpfLinks < NETMONITOR_MAX_PROGRAMS) {
         bpfLinks[nBpfLinks++] = link;
      } else {
         sym_bpf_link__destroy(link);
      }
   }
   if (!nBpfLinks)
      goto fail;
#if !defined(NDEBUG)
   NetMonitor_debug("eBPF probe attach summary (load succeeded):\n");
   for (size_t i = 0; i < nResults; i++) {
      const char* section = results[i].section ? results[i].section : "<unknown>";
      if (results[i].attached) {
         NetMonitor_debug("  %-26s attached\n", section);
      } else if (results[i].error == ENOENT) {
         const char* note = NetMonitor_expectedMissing(section);
         NetMonitor_debug("  %-26s skipped: expected missing (%s)\n", section, note ? note : "symbol not found");
      } else {
         NetMonitor_debug("  %-26s skipped: %s\n", section, results[i].error ? strerror(results[i].error) : "attach failed");
      }
   }
   if (anyNotAllowed) {
      char paranoid[32] = "unreadable";
      FILE* fp = fopen("/proc/sys/kernel/perf_event_paranoid", "r");
      if (fp) {
         char* line = String_readLine(fp);
         fclose(fp);
         if (line) {
            String_safeStrncpy(paranoid, line, sizeof(paranoid));
            free(line);
         }
      }
      NetMonitor_debug("kprobe attach needs CAP_SYS_ADMIN or perf_event_paranoid<=2 (current: %s; a \"Lockdown:\" line in dmesg means kernel lockdown is the cause)\n", paranoid);
   }
#endif

   void* map = sym_bpf_object__find_map_by_name(obj, "netio_map");
   if (!map) {
      NetMonitor_debug("map netio_map not found in eBPF object\n");
      goto fail;
   }

   netioMapFd = sym_bpf_map__fd(map);
   if (netioMapFd < 0) {
      NetMonitor_debug("cannot get file descriptor for map netio_map\n");
      goto fail;
   }

   eBPFActive = true;
   NetMonitor_debug("eBPF network monitoring active via %s\n", dlopenName);
   return true;

fail:
   NetMonitor_freeBPF();
   return false;
}

static void NetMonitor_cleanupMap(void) {
   uint32_t key, nextKey;

   if (sym_bpf_map_get_next_key(netioMapFd, NULL, &key) != 0)
      return;

   do {
      bool keep = (kill((pid_t) key, 0) == 0 || errno == EPERM);
      int next = sym_bpf_map_get_next_key(netioMapFd, &key, &nextKey);
      if (!keep)
         sym_bpf_map_delete_elem(netioMapFd, &key);
      if (next != 0)
         return;
      key = nextKey;
   } while (true);
}

void NetMonitor_init(void) {
   eBPFLoadAttempted = false;
   eBPFActive = false;
   netioMapFd = -1;
   nBpfLinks = 0;
}

void NetMonitor_done(void) {
   NetMonitor_freeBPF();
   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }
}

bool NetMonitor_isActive(void) {
   return eBPFActive;
}

void NetMonitor_update(void) {
   if (!eBPFActive) {
      if (eBPFLoadAttempted)
         return;
      eBPFLoadAttempted = true;
      bool loaded = NetMonitor_loadBPF();
      /* Load-time-only privileges (CAP_BPF, CAP_PERFMON, CAP_SYSLOG,
       * CAP_SYS_ADMIN) are no longer needed once the maps exist, whether the
       * attempt succeeded or not. dropCapabilities() already kills htop, so
       * the below is mostly cosmetics / debugging and should go when the
       * feature is being committed to main.
       * !!!TODO DL260913 */
      int capResult = Platform_dropEBPFCapabilities();
      if (capResult != 0)
         NetMonitor_debug("could not drop the eBPF load-time capabilities\n");
      if (!loaded)
         return;
   }

   static unsigned int cleanupTicker = 0;
   if ((++cleanupTicker % NETMONITOR_CLEANUP_INTERVAL) == 0)
      NetMonitor_cleanupMap();
}

bool NetMonitor_getNetIO(pid_t pid, NetIOData* data) {
   if (!eBPFActive || netioMapFd < 0)
      return false;

   uint32_t key = (uint32_t) pid;
   NetIOBpfCounts counts;
   if (sym_bpf_map_lookup_elem(netioMapFd, &key, &counts) != 0)
      return false;

   data->rxBytes = counts.rxBytes;
   data->txBytes = counts.txBytes;
   data->rxPackets = counts.rxPackets;
   data->txPackets = counts.txPackets;
   return true;
}

#endif /* HAVE_EBPF_NET */
