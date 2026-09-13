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

/* The task group id of the process currently running on the CPU */
static __always_inline __u32 netio_currentPid(void) {
   return (__u32) (bpf_get_current_pid_tgid() >> 32);
}

/* Per-call byte budgets. A single stream send/recv hands at most a few
 * MiB to the application; datagrams are at most 64 KiB. Clamping each
 * probe call bounds the cumulative counters by (packets * budget), so a
 * miscounted or inflated return value cannot inflate the displayed rate. */
#define NET_MAX_STREAM_BYTES (1 << 20)
#define NET_MAX_DATAGRAM_BYTES (64 << 10)

static __always_inline __u64 netio_bounded(long ret, __u64 max) {
   __u64 bytes = ret > 0 ? (__u64) ret : 0;
   return bytes > max ? max : bytes;
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

/* All send paths count on return: the number of bytes actually accepted by
 * the protocol stack, which may be less than the size requested on entry
 * (partial sends, interrupted transfers, corked streams). */
SEC("kretprobe/tcp_sendmsg")
int htop_tcp_tx(struct pt_regs *ctx) {
   long ret = PT_REGS_RC(ctx);
   netio_count(netio_currentPid(), 0, netio_bounded(ret, NET_MAX_STREAM_BYTES));
   return 0;
}

SEC("kretprobe/tcp_recvmsg")
int htop_tcp_rx(struct pt_regs *ctx) {
   long ret = PT_REGS_RC(ctx);
   netio_count(netio_currentPid(), netio_bounded(ret, NET_MAX_STREAM_BYTES), 0);
   return 0;
}

SEC("kretprobe/udp_sendmsg")
int htop_udp_tx(struct pt_regs *ctx) {
   long ret = PT_REGS_RC(ctx);
   netio_count(netio_currentPid(), 0, netio_bounded(ret, NET_MAX_DATAGRAM_BYTES));
   return 0;
}

SEC("kretprobe/udp_recvmsg")
int htop_udp_rx(struct pt_regs *ctx) {
   long ret = PT_REGS_RC(ctx);
   netio_count(netio_currentPid(), netio_bounded(ret, NET_MAX_DATAGRAM_BYTES), 0);
   return 0;
}

SEC("kretprobe/ping_v4_sendmsg")
int htop_icmp4_tx(struct pt_regs *ctx) {
   long ret = PT_REGS_RC(ctx);
   netio_count(netio_currentPid(), 0, netio_bounded(ret, NET_MAX_DATAGRAM_BYTES));
   return 0;
}

SEC("kretprobe/ping_v6_sendmsg")
int htop_icmp6_tx(struct pt_regs *ctx) {
   long ret = PT_REGS_RC(ctx);
   netio_count(netio_currentPid(), 0, netio_bounded(ret, NET_MAX_DATAGRAM_BYTES));
   return 0;
}

/* ping_recvmsg() serves both ICMPv4 and ICMPv6 ping sockets */
SEC("kretprobe/ping_recvmsg")
int htop_icmp_rx(struct pt_regs *ctx) {
   long ret = PT_REGS_RC(ctx);
   netio_count(netio_currentPid(), netio_bounded(ret, NET_MAX_DATAGRAM_BYTES), 0);
   return 0;
}

char LICENSE[] SEC("license") = "GPL";