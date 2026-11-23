#ifndef HEADER_ProcMonitor
#define HEADER_ProcMonitor
/*
htop - linux/ProcMonitor.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

Process and thread start and exit tracking via an eBPF program (fork,
exec and exit scheduler tracepoints). Events are delivered as soon as
they are read from the ring buffer, so that the process list can show
started and ended processes without waiting for the next scan of /proc.
*/

#include "config.h" // IWYU pragma: keep

#include <stdbool.h>
#include <stddef.h>

#include "linux/ProcMonitorEvent.h"


/* Called for every event; the EXEC only members of ev are valid for
 * PROCMONITOR_EVENT_EXEC records only */
typedef void (*ProcMonitor_EventFn)(const ProcMonitorEvent* ev, void* ctx);

#ifdef HAVE_EBPF_PROCMON

void ProcMonitor_init(void);
void ProcMonitor_done(void);

/* Loads the eBPF program on the first call with wanted set. While not
 * wanted, events of an already loaded program are discarded. Thread
 * events are only reported with traceThreads set. */
void ProcMonitor_update(bool wanted, bool traceThreads);

bool ProcMonitor_isActive(void);

/* Whether the monitor made its one decision on loading, i.e. the
 * load-time capabilities are no longer needed on its behalf */
bool ProcMonitor_loadSettled(void);

/* Readable while unconsumed events are pending, -1 while inactive */
int ProcMonitor_wakeupFd(void);

/* Hands every pending event to fn without blocking; returns their number */
size_t ProcMonitor_consume(ProcMonitor_EventFn fn, void* ctx);

#else /* !HAVE_EBPF_PROCMON */

static inline void ProcMonitor_init(void) { }
static inline void ProcMonitor_done(void) { }
static inline void ProcMonitor_update(bool wanted, bool traceThreads) { (void) wanted; (void) traceThreads; }
static inline bool ProcMonitor_isActive(void) { return false; }
static inline bool ProcMonitor_loadSettled(void) { return true; }
static inline int ProcMonitor_wakeupFd(void) { return -1; }
static inline size_t ProcMonitor_consume(ProcMonitor_EventFn fn, void* ctx) { (void) fn; (void) ctx; return 0; }

#endif /* HAVE_EBPF_PROCMON */

#endif /* HEADER_ProcMonitor */
