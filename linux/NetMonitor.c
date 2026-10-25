/*
htop - linux/NetMonitor.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/NetMonitor.h"

#ifdef HAVE_EBPF_NETIO

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#include "XUtils.h"
#include "linux/LibBpf.h"


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

/* When a probe fails to attach because its target symbol is absent, map the
 * probe back to the kernel range its symbol was introduced on. The per-family
 * ping senders were introduced in Linux 3.18 when IPv6 ping sockets landed;
 * ping_recvmsg() itself predates that split. */
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

static bool eBPFLoadAttempted = false;
static bool eBPFActive = false;
static int netioMapFd = -1;

static LibBpfObject bpf = { .name = "netmon" };

/* Interval (in scan cycles) between sweeps that drop stale map entries */
#define NETMONITOR_CLEANUP_INTERVAL 16

static void NetMonitor_freeBPF(void) {
   LibBpf_closeObject(&bpf);
   netioMapFd = -1;
   eBPFActive = false;
}

static bool NetMonitor_loadBPF(void) {
   if (!LibBpf_openObject(&bpf, _binary_NetMonitor_bpf_o_start, _binary_NetMonitor_bpf_o_end, NetMonitor_expectedMissing))
      return false;

   netioMapFd = LibBpf_mapFd(&bpf, "netio_map");
   if (netioMapFd < 0) {
      NetMonitor_freeBPF();
      return false;
   }

   eBPFActive = true;
   return true;
}

static void NetMonitor_cleanupMap(void) {
   uint32_t key, nextKey;

   if (LibBpf_mapGetNextKey(netioMapFd, NULL, &key) != 0)
      return;

   do {
      bool keep = (kill((pid_t) key, 0) == 0 || errno == EPERM);
      int next = LibBpf_mapGetNextKey(netioMapFd, &key, &nextKey);
      if (!keep)
         LibBpf_mapDeleteElem(netioMapFd, &key);
      if (next != 0)
         return;
      key = nextKey;
   } while (true);
}

void NetMonitor_init(void) {
   eBPFLoadAttempted = false;
   eBPFActive = false;
   netioMapFd = -1;
}

void NetMonitor_done(void) {
   NetMonitor_freeBPF();
}

bool NetMonitor_isActive(void) {
   return eBPFActive;
}

bool NetMonitor_loadAttempted(void) {
   return eBPFLoadAttempted;
}

void NetMonitor_update(void) {
   if (!eBPFActive) {
      if (eBPFLoadAttempted)
         return;
      eBPFLoadAttempted = true;
      /* The load-time capabilities are dropped by the caller once every
       * eBPF monitor has had its load attempt, see
       * ProcessTable_goThroughEntries(). */
      if (!NetMonitor_loadBPF())
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
   if (LibBpf_mapLookupElem(netioMapFd, &key, &counts) != 0)
      return false;

   data->rxBytes = counts.rxBytes;
   data->txBytes = counts.txBytes;
   data->rxPackets = counts.rxPackets;
   data->txPackets = counts.txPackets;
   return true;
}

#endif /* HAVE_EBPF_NETIO */
