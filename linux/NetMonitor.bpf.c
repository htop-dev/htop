/*
htop - linux/NetMonitor.bpf.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

eBPF program for per-process network bandwidth accounting.
Compiled at build time with clang into an ELF object which is
embedded into the htop binary and loaded via libbpf.
*/

#include <linux/bpf.h>
#include <linux/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>


struct sock;
struct msghdr;

typedef struct NetIOBpfCounts_ {
   __u64 rxBytes;
   __u64 txBytes;
   __u64 rxPackets;
   __u64 txPackets;
} NetIOBpfCounts;

struct {
   __uint(type, BPF_MAP_TYPE_HASH);
   __uint(max_entries, 65536);
   __type(key, __u32);
   __type(value, NetIOBpfCounts);
} netio_map SEC(".maps");

/* Stashed request length for each in-flight probe pair. The key is the
 * full pid_tgid so that a blocked syscall returning on a different CPU
 * still finds its own entry. LRU evicts stale entries if a return probe
 * never fires (missed or filtered). */
struct {
   __uint(type, BPF_MAP_TYPE_LRU_HASH);
   __uint(max_entries, 65536);
   __type(key, __u64);
   __type(value, __u64);
} netio_req_map SEC(".maps");

/* The task group id of the process currently running on the CPU */
static __always_inline __u32 netio_currentPid(void) {
   return (__u32) (bpf_get_current_pid_tgid() >> 32);
}

/* Per-call byte budgets. A single stream send/recv hands at most a few
 * MiB to the application; datagrams are at most 64 KiB. The budget is
 * a secondary safety net; the primary guard is the entry-length check
 * in netio_validBytes(). */
#define NET_MAX_STREAM_BYTES (1 << 20)
#define NET_MAX_DATAGRAM_BYTES (64 << 10)

static __always_inline void netio_stampLen(__u64 len) {
   __u64 key = bpf_get_current_pid_tgid();
   bpf_map_update_elem(&netio_req_map, &key, &len, BPF_ANY);
}

/* Retrieve the length stashed at entry, validate the return value and
 * apply the per-call budget. On some kernels, kretprobe return samples
 * carry corrupted register values that can exceed the size the
 * application actually requested. A call can never validly return more
 * than its request; such values are silently discarded. */
static __always_inline __u64 netio_validBytes(long ret, __u64 budget) {
   if (ret <= 0)
      return 0;

   __u64 key = bpf_get_current_pid_tgid();
   __u64* req = bpf_map_lookup_elem(&netio_req_map, &key);
   __u64 len = req ? *req : 0;
   if (req)
      bpf_map_delete_elem(&netio_req_map, &key);

   __u64 bytes = (__u64) ret;
   if (len && bytes > len)
      return 0;
   return bytes > budget ? budget : bytes;
}

static __always_inline void netio_addToCounts(NetIOBpfCounts* counts, __u64 rx, __u64 tx) {
   if (rx) {
      __sync_fetch_and_add(&counts->rxBytes, rx);
      __sync_fetch_and_add(&counts->rxPackets, 1);
   }
   if (tx) {
      __sync_fetch_and_add(&counts->txBytes, tx);
      __sync_fetch_and_add(&counts->txPackets, 1);
   }
}

static __always_inline void netio_count(__u32 pid, __u64 rxBytes, __u64 txBytes) {
   NetIOBpfCounts* counts = bpf_map_lookup_elem(&netio_map, &pid);
   if (counts) {
      netio_addToCounts(counts, rxBytes, txBytes);
      return;
   }

   NetIOBpfCounts init = {
      .rxBytes = rxBytes,
      .txBytes = txBytes,
      .rxPackets = rxBytes ? 1 : 0,
      .txPackets = txBytes ? 1 : 0,
   };
   if (bpf_map_update_elem(&netio_map, &pid, &init, BPF_NOEXIST) != 0) {
      /* Lost the race: another CPU created the entry while we looked for it. */
      counts = bpf_map_lookup_elem(&netio_map, &pid);
      if (counts)
         netio_addToCounts(counts, rxBytes, txBytes);
   }
}

/* Entry + return probe pairs
 * Entry probes stash the requested length (arg3) in netio_req_map.
 * Return probes validate the return value against that length and
 * count the result, applying the budget clamp as a secondary limit. */

SEC("kprobe/tcp_sendmsg")
int htop_tcp_tx_entry(struct pt_regs *ctx) {
   netio_stampLen(PT_REGS_PARM3(ctx));
   return 0;
}

SEC("kretprobe/tcp_sendmsg")
int htop_tcp_tx(struct pt_regs *ctx) {
   netio_count(netio_currentPid(), 0, netio_validBytes(PT_REGS_RC(ctx), NET_MAX_STREAM_BYTES));
   return 0;
}

SEC("kprobe/tcp_recvmsg")
int htop_tcp_rx_entry(struct pt_regs *ctx) {
   netio_stampLen(PT_REGS_PARM3(ctx));
   return 0;
}

SEC("kretprobe/tcp_recvmsg")
int htop_tcp_rx(struct pt_regs *ctx) {
   netio_count(netio_currentPid(), netio_validBytes(PT_REGS_RC(ctx), NET_MAX_STREAM_BYTES), 0);
   return 0;
}

SEC("kprobe/udp_sendmsg")
int htop_udp_tx_entry(struct pt_regs *ctx) {
   netio_stampLen(PT_REGS_PARM3(ctx));
   return 0;
}

SEC("kretprobe/udp_sendmsg")
int htop_udp_tx(struct pt_regs *ctx) {
   netio_count(netio_currentPid(), 0, netio_validBytes(PT_REGS_RC(ctx), NET_MAX_DATAGRAM_BYTES));
   return 0;
}

SEC("kprobe/udp_recvmsg")
int htop_udp_rx_entry(struct pt_regs *ctx) {
   netio_stampLen(PT_REGS_PARM3(ctx));
   return 0;
}

SEC("kretprobe/udp_recvmsg")
int htop_udp_rx(struct pt_regs *ctx) {
   netio_count(netio_currentPid(), netio_validBytes(PT_REGS_RC(ctx), NET_MAX_DATAGRAM_BYTES), 0);
   return 0;
}

SEC("kprobe/ping_v4_sendmsg")
int htop_icmp4_tx_entry(struct pt_regs *ctx) {
   netio_stampLen(PT_REGS_PARM3(ctx));
   return 0;
}

SEC("kretprobe/ping_v4_sendmsg")
int htop_icmp4_tx(struct pt_regs *ctx) {
   netio_count(netio_currentPid(), 0, netio_validBytes(PT_REGS_RC(ctx), NET_MAX_DATAGRAM_BYTES));
   return 0;
}

SEC("kprobe/ping_v6_sendmsg")
int htop_icmp6_tx_entry(struct pt_regs *ctx) {
   netio_stampLen(PT_REGS_PARM3(ctx));
   return 0;
}

SEC("kretprobe/ping_v6_sendmsg")
int htop_icmp6_tx(struct pt_regs *ctx) {
   netio_count(netio_currentPid(), 0, netio_validBytes(PT_REGS_RC(ctx), NET_MAX_DATAGRAM_BYTES));
   return 0;
}

SEC("kprobe/ping_recvmsg")
int htop_icmp_rx_entry(struct pt_regs *ctx) {
   netio_stampLen(PT_REGS_PARM3(ctx));
   return 0;
}

/* ping_recvmsg() serves both ICMPv4 and ICMPv6 ping sockets */
SEC("kretprobe/ping_recvmsg")
int htop_icmp_rx(struct pt_regs *ctx) {
   netio_count(netio_currentPid(), netio_validBytes(PT_REGS_RC(ctx), NET_MAX_DATAGRAM_BYTES), 0);
   return 0;
}

char LICENSE[] SEC("license") = "GPL";
