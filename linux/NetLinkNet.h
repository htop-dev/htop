#ifndef HEADER_NetLinkNet
#define HEADER_NetLinkNet
/*
htop - linux/NetLinkNet.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

Per-process network bandwidth accounting via the kernel sock_diag
interface (NETLINK_SOCK_DIAG). Sockets are discovered by scanning
/proc/<pid>/fd, TCP byte counters come from the per-socket tcp_info
structure and are exact, while UDP and ICMP counters come from
per-socket queue sizes and approximate the traffic. When eBPF is
unavailable the callers fall back to this layer, and to the
rchar/wchar estimate when netlink is unavailable too.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <sys/types.h>

#ifdef HAVE_LIBNL_NET

void NetLinkNet_init(void);
void NetLinkNet_done(void);
bool NetLinkNet_isActive(void);
void NetLinkNet_update(void);
bool NetLinkNet_getNetBytes(pid_t pid, unsigned long long* rx, unsigned long long* tx);

#else /* !HAVE_LIBNL_NET */

static inline void NetLinkNet_init(void) { }
static inline void NetLinkNet_done(void) { }
static inline bool NetLinkNet_isActive(void) { return false; }
static inline void NetLinkNet_update(void) { }
static inline bool NetLinkNet_getNetBytes(pid_t pid, unsigned long long* rx, unsigned long long* tx) { (void) pid; (void) rx; (void) tx; return false; }

#endif /* HAVE_LIBNL_NET */

#endif /* HEADER_NetLinkNet */