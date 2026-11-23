/*
htop - linux/ProcMonitor.bpf.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

eBPF program reporting process creation, exec and termination.
Compiled at build time into an ELF object which is embedded into
the htop binary and loaded via libbpf.

The programs attach to the BTF enabled scheduler tracepoints and read
task data with CO-RE relocations. Instead of a generated vmlinux.h,
minimal views on the kernel structures are declared below; libbpf
relocates every access against the running kernel's BTF at load time.
This requires CONFIG_DEBUG_INFO_BTF (/sys/kernel/btf/vmlinux) and
Linux >= 5.8 for the ring buffer.
*/

#include <stdbool.h>
#include <linux/bpf.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "linux/ProcMonitorEvent.h"


/* Minimal views on kernel structures: only the members used below are
 * declared. Neither member order nor structure size matter, CO-RE resolves
 * each access by name. */
typedef struct {
   int counter;
} atomic_t;

typedef struct {
   __u32 val;
} kuid_t;

struct cred {
   kuid_t euid;
} __attribute__((preserve_access_index));

struct mm_struct {
   unsigned long arg_start;
   unsigned long arg_end;
} __attribute__((preserve_access_index));

struct sched_entity {
   __u64 sum_exec_runtime;
} __attribute__((preserve_access_index));

struct signal_struct {
   atomic_t live;
   __u64 sum_sched_runtime;
} __attribute__((preserve_access_index));

struct task_struct {
   unsigned int flags;
   int exit_code;
   int pid;
   int tgid;
   struct task_struct* real_parent;
   const struct cred* real_cred;
   struct mm_struct* mm;
   struct signal_struct* signal;
   struct sched_entity se;
   __u64 start_boottime;
   char comm[PROCMONITOR_COMM_LEN];
} __attribute__((preserve_access_index));

struct linux_binprm {
   const char* filename;
} __attribute__((preserve_access_index));

/* Sized for bursts: a parallel build may start and end thousands of
 * processes in the time htop needs to get scheduled */
struct {
   __uint(type, BPF_MAP_TYPE_RINGBUF);
   __uint(max_entries, 1024 * 1024);
} procmon_events SEC(".maps");

struct {
   __uint(type, BPF_MAP_TYPE_ARRAY);
   __uint(max_entries, 1);
   __type(key, __u32);
   __type(value, ProcMonitorConfig);
} procmon_config SEC(".maps");

/* Records that did not fit into the ring buffer */
struct {
   __uint(type, BPF_MAP_TYPE_ARRAY);
   __uint(max_entries, 1);
   __type(key, __u32);
   __type(value, __u64);
} procmon_lost SEC(".maps");

static __always_inline bool procmon_traceThreads(void) {
   __u32 key = 0;
   const ProcMonitorConfig* cfg = bpf_map_lookup_elem(&procmon_config, &key);
   return cfg && cfg->traceThreads;
}

static __always_inline ProcMonitorEvent* procmon_reserve(__u64 size) {
   ProcMonitorEvent* e = bpf_ringbuf_reserve(&procmon_events, size, 0);
   if (!e) {
      __u32 key = 0;
      __u64* lost = bpf_map_lookup_elem(&procmon_lost, &key);
      if (lost)
         __sync_fetch_and_add(lost, 1);
   }
   return e;
}

static __always_inline void procmon_fillIdentity(ProcMonitorEvent* e, __u32 type, const struct task_struct* task) {
   e->type = type;
   e->pid = (__u32) BPF_CORE_READ(task, tgid);
   e->tid = (__u32) BPF_CORE_READ(task, pid);
   e->ppid = (__u32) BPF_CORE_READ(task, real_parent, tgid);
   e->uid = BPF_CORE_READ(task, real_cred, euid.val);
   e->flags = BPF_CORE_READ(task, flags);
   e->exitCode = 0;
   e->reserved = 0;
   e->startBootNs = BPF_CORE_READ(task, start_boottime);
   e->cpuNs = 0;
   if (BPF_CORE_READ_STR_INTO(&e->comm, task, comm) < 0)
      e->comm[0] = '\0';
}

/* A new process (fork(2), vfork(2), clone(2)) or, with CLONE_THREAD, a new thread */
SEC("tp_btf/sched_process_fork")
int BPF_PROG(htop_proc_fork, struct task_struct* parent, struct task_struct* child) {
   (void) parent;

   const bool isThread = BPF_CORE_READ(child, pid) != BPF_CORE_READ(child, tgid);
   if (isThread && !procmon_traceThreads())
      return 0;

   ProcMonitorEvent* e = procmon_reserve(PROCMONITOR_EVENT_BASE_SIZE);
   if (!e)
      return 0;

   procmon_fillIdentity(e, isThread ? PROCMONITOR_EVENT_THREAD_START : PROCMONITOR_EVENT_FORK, child);
   bpf_ringbuf_submit(e, 0);
   return 0;
}

/* A successful execve(2); de_thread() already made the caller the thread group leader */
SEC("tp_btf/sched_process_exec")
int BPF_PROG(htop_proc_exec, struct task_struct* task, int oldPid, struct linux_binprm* bprm) {
   (void) oldPid;

   ProcMonitorEvent* e = procmon_reserve(sizeof(*e));
   if (!e)
      return 0;

   procmon_fillIdentity(e, PROCMONITOR_EVENT_EXEC, task);
   e->reserved2 = 0;

   const char* filename = BPF_CORE_READ(bprm, filename);
   if (bpf_probe_read_kernel_str(e->filename, sizeof(e->filename), filename) < 0)
      e->filename[0] = '\0';

   /* The new argv has just been copied onto the fresh stack, so its pages
    * are present. Masking keeps the length provably within the buffer for
    * the verifier, at the price of one byte. */
   unsigned long argStart = BPF_CORE_READ(task, mm, arg_start);
   unsigned long argEnd = BPF_CORE_READ(task, mm, arg_end);
   __u32 len = 0;
   if (argEnd > argStart) {
      unsigned long n = argEnd - argStart;
      if (n > PROCMONITOR_ARGS_LEN - 1)
         n = PROCMONITOR_ARGS_LEN - 1;
      len = (__u32) n & (PROCMONITOR_ARGS_LEN - 1);
      if (bpf_probe_read_user(e->args, len, (const void*) argStart) != 0)
         len = 0;
   }
   e->argsLen = len;

   bpf_ringbuf_submit(e, 0);
   return 0;
}

/* The end of a thread; the last one of the group ends the process */
SEC("tp_btf/sched_process_exit")
int BPF_PROG(htop_proc_exit, struct task_struct* task) {
   /* do_exit() decrements signal->live before this tracepoint fires */
   const bool groupDead = BPF_CORE_READ(task, signal, live.counter) == 0;

   if (!groupDead) {
      /* A thread group leader leaving early stays a zombie until the
       * group ends; userspace keeps showing the process for it */
      if (BPF_CORE_READ(task, pid) == BPF_CORE_READ(task, tgid))
         return 0;
      if (!procmon_traceThreads())
         return 0;
   }

   ProcMonitorEvent* e = procmon_reserve(PROCMONITOR_EVENT_BASE_SIZE);
   if (!e)
      return 0;

   procmon_fillIdentity(e, groupDead ? PROCMONITOR_EVENT_EXIT : PROCMONITOR_EVENT_THREAD_EXIT, task);
   e->exitCode = BPF_CORE_READ(task, exit_code);

   /* Threads that ended before were folded into signal->sum_sched_runtime;
    * the exiting thread itself is accounted in its scheduling entity. */
   e->cpuNs = BPF_CORE_READ(task, se.sum_exec_runtime);
   if (groupDead)
      e->cpuNs += BPF_CORE_READ(task, signal, sum_sched_runtime);

   bpf_ringbuf_submit(e, 0);
   return 0;
}

char LICENSE[] SEC("license") = "GPL";
