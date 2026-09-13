#ifndef HEADER_NetMonitor
#define HEADER_NetMonitor
/*
htop - linux/NetMonitor.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

Per-process network bandwidth accounting via an eBPF program.
The eBPF object is compiled at build time, embedded into the htop
binary and loaded through libbpf (dlopened at runtime).
When eBPF is unavailable the callers fall back to rchar/wchar deltas.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct NetIOData_ {
   uint64_t rxBytes;
   uint64_t txBytes;
   uint64_t rxPackets;
   uint64_t txPackets;
} NetIOData;

#ifdef HAVE_EBPF_NET

void NetMonitor_init(void);
void NetMonitor_done(void);
bool NetMonitor_isActive(void);
void NetMonitor_update(void);
bool NetMonitor_getNetIO(pid_t pid, NetIOData* data);

#else /* !HAVE_EBPF_NET */

static inline void NetMonitor_init(void) { }
static inline void NetMonitor_done(void) { }
static inline bool NetMonitor_isActive(void) { return false; }
static inline void NetMonitor_update(void) { }
static inline bool NetMonitor_getNetIO(pid_t pid, NetIOData* data) { (void) pid; (void) data; return false; }

#endif /* HAVE_EBPF_NET */

#endif /* HEADER_NetMonitor */