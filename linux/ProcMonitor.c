/*
htop - linux/ProcMonitor.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/ProcMonitor.h"

#ifdef HAVE_EBPF_PROCMON

#include <stdint.h>
#include <unistd.h>

#include "Macros.h"
#include "XUtils.h"
#include "linux/LibBpf.h"


/* eBPF object embedded into the binary by objcopy */
extern const unsigned char _binary_ProcMonitor_bpf_o_start[];
extern const unsigned char _binary_ProcMonitor_bpf_o_end[];

typedef enum ProcMonitorState_ {
   PROCMONITOR_UNDECIDED,     /* not asked yet */
   PROCMONITOR_DECLINED,      /* not wanted when asked */
   PROCMONITOR_ACTIVE,
   PROCMONITOR_FAILED,
} ProcMonitorState;

static ProcMonitorState state = PROCMONITOR_UNDECIDED;
static bool collecting = false;

static LibBpfObject bpf = { .name = "procmon" };
static void* ringBuffer = NULL;
static int wakeupFd = -1;
static int configMapFd = -1;
static int lostMapFd = -1;

/* Thread tracing as last written to the config map; -1 if not written yet */
static int tracingThreads = -1;

/* Receiver of the events while ProcMonitor_consume() runs */
static ProcMonitor_EventFn consumeFn = NULL;
static void* consumeCtx = NULL;
static size_t consumed = 0;

static uint64_t lostReported = 0;

#ifndef NDEBUG
/* Events received per ProcMonitorEventType, reported on exit in debug builds */
static unsigned long long eventCounts[PROCMONITOR_EVENT_THREAD_EXIT + 1];
#endif

static int ProcMonitor_handleEvent(void* ctx, void* data, size_t size) {
   (void) ctx;

   if (size < PROCMONITOR_EVENT_BASE_SIZE)
      return 0;

   const ProcMonitorEvent* ev = data;
   if (ev->type == PROCMONITOR_EVENT_EXEC && size < sizeof(ProcMonitorEvent))
      return 0;

   consumed++;
#ifndef NDEBUG
   if (ev->type < ARRAYSIZE(eventCounts))
      eventCounts[ev->type]++;
#endif
   if (collecting && consumeFn)
      consumeFn(ev, consumeCtx);

   return 0;
}

/* The event PIDs are the kernel's global ones, which only match /proc
 * when htop runs in the initial PID namespace, see Platform_init() */
static bool ProcMonitor_inInitialPidNamespace(void) {
   char target[64];
   ssize_t ret = readlink(PROCDIR "/self/ns/pid", target, sizeof(target) - 1);
   if (ret <= 0)
      return true;

   target[ret] = '\0';
   return String_eq("pid:[4026531836]", target); // PROC_PID_INIT_INO from include/linux/proc_ns.h
}

static void ProcMonitor_freeBPF(void) {
   LibBpf_ringBufferFree(ringBuffer);
   ringBuffer = NULL;
   wakeupFd = -1;
   configMapFd = -1;
   lostMapFd = -1;
   tracingThreads = -1;
   LibBpf_closeObject(&bpf);
}

static bool ProcMonitor_loadBPF(void) {
   if (!ProcMonitor_inInitialPidNamespace()) {
      LibBpf_debug(bpf.name, "not in the initial PID namespace, process events would not match /proc\n");
      return false;
   }

   if (!LibBpf_openObject(&bpf, _binary_ProcMonitor_bpf_o_start, _binary_ProcMonitor_bpf_o_end, NULL))
      return false;

   configMapFd = LibBpf_mapFd(&bpf, "procmon_config");
   lostMapFd = LibBpf_mapFd(&bpf, "procmon_lost");
   ringBuffer = LibBpf_ringBufferNew(LibBpf_mapFd(&bpf, "procmon_events"), ProcMonitor_handleEvent, NULL);
   if (configMapFd < 0 || lostMapFd < 0 || !ringBuffer) {
      LibBpf_debug(bpf.name, "cannot set up the maps or the ring buffer consumer\n");
      ProcMonitor_freeBPF();
      return false;
   }

   wakeupFd = LibBpf_ringBufferEpollFd(ringBuffer);
   return true;
}

static void ProcMonitor_setTraceThreads(bool traceThreads) {
   if (tracingThreads == (int)traceThreads)
      return;

   uint32_t key = 0;
   ProcMonitorConfig config = { .traceThreads = traceThreads };
   if (LibBpf_mapUpdateElem(configMapFd, &key, &config) != 0) {
      LibBpf_debug(bpf.name, "cannot update the configuration map\n");
      return;
   }

   tracingThreads = traceThreads;
}

static void ProcMonitor_reportLost(void) {
#ifndef NDEBUG
   uint32_t key = 0;
   uint64_t lost = 0;
   if (LibBpf_mapLookupElem(lostMapFd, &key, &lost) != 0 || lost == lostReported)
      return;

   LibBpf_debug(bpf.name, "%llu events lost, the ring buffer was full\n", (unsigned long long)(lost - lostReported));
   lostReported = lost;
#endif
}

void ProcMonitor_init(void) {
   state = PROCMONITOR_UNDECIDED;
   collecting = false;
   ringBuffer = NULL;
   wakeupFd = -1;
   configMapFd = -1;
   lostMapFd = -1;
   tracingThreads = -1;
   lostReported = 0;
}

void ProcMonitor_done(void) {
#ifndef NDEBUG
   if (state == PROCMONITOR_ACTIVE) {
      ProcMonitor_reportLost();
      LibBpf_debug(bpf.name, "events received: fork=%llu exec=%llu exit=%llu thread_start=%llu thread_exit=%llu\n",
                   eventCounts[PROCMONITOR_EVENT_FORK], eventCounts[PROCMONITOR_EVENT_EXEC], eventCounts[PROCMONITOR_EVENT_EXIT],
                   eventCounts[PROCMONITOR_EVENT_THREAD_START], eventCounts[PROCMONITOR_EVENT_THREAD_EXIT]);
   }
#endif
   ProcMonitor_freeBPF();
   state = PROCMONITOR_UNDECIDED;
   collecting = false;
}

void ProcMonitor_update(bool wanted, bool traceThreads) {
   switch (state) {
      case PROCMONITOR_UNDECIDED:
      case PROCMONITOR_DECLINED:
         if (!wanted) {
            state = PROCMONITOR_DECLINED;
            break;
         }
         /* Once the load-time capabilities are gone (see
          * ProcessTable_goThroughEntries()), a monitor declined at startup
          * cannot load anymore; the attempt then fails like any other. */
         state = ProcMonitor_loadBPF() ? PROCMONITOR_ACTIVE : PROCMONITOR_FAILED;
         break;
      case PROCMONITOR_ACTIVE:
      case PROCMONITOR_FAILED:
         break;
   }

   collecting = wanted && state == PROCMONITOR_ACTIVE;
   if (state != PROCMONITOR_ACTIVE)
      return;

   /* While disabled no thread events are needed either */
   ProcMonitor_setTraceThreads(collecting && traceThreads);
   ProcMonitor_reportLost();
}

bool ProcMonitor_isActive(void) {
   return collecting;
}

bool ProcMonitor_loadSettled(void) {
   return state != PROCMONITOR_UNDECIDED;
}

int ProcMonitor_wakeupFd(void) {
   return collecting ? wakeupFd : -1;
}

size_t ProcMonitor_consume(ProcMonitor_EventFn fn, void* ctx) {
   if (state != PROCMONITOR_ACTIVE)
      return 0;

   /* While disabled the program stays attached and its events are
    * drained unseen, so the ring buffer does not hold stale ones */
   consumeFn = fn;
   consumeCtx = ctx;
   consumed = 0;

   int r = LibBpf_ringBufferConsume(ringBuffer);
   if (r < 0)
      LibBpf_debug(bpf.name, "consuming the ring buffer failed: %d\n", r);

   consumeFn = NULL;
   consumeCtx = NULL;
   return collecting ? consumed : 0;
}

#endif /* HAVE_EBPF_PROCMON */
