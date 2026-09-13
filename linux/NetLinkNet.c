/*
htop - linux/NetLinkNet.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/NetLinkNet.h"

#ifdef HAVE_LIBNL_NET

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/tcp.h>

#include <netinet/in.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "Macros.h"
#include "XUtils.h"
#include "linux/LinuxMachine.h"
#include "linux/Platform.h"


/* libnl is only used as netlink socket plumbing. Its headers pull in
 * netinet/tcp.h, which clashes with the linux/tcp.h definitions of
 * struct tcphdr and struct tcp_info that we need, so the libnl types are
 * forward-declared here and every entry point is resolved with dlsym. */
struct nl_sock;
struct nl_msg;

typedef struct nl_sock* (*NlSocketAllocFn)(void);
typedef void (*NlSocketFreeFn)(struct nl_sock*);
typedef int (*NlSocketModifyCbFn)(struct nl_sock*, int, int, int (*)(struct nl_msg*, void*), void*);
typedef int (*NlConnectFn)(struct nl_sock*, int);
typedef int (*NlCloseFn)(struct nl_sock*);
typedef struct nl_msg* (*NlmsgAllocFn)(void);
typedef struct nlmsghdr* (*NlmsgHdrFn)(struct nl_msg*);
typedef int (*NlSendSyncFn)(struct nl_sock*, struct nl_msg*);

static NlSocketAllocFn sym_nl_socket_alloc;
static NlSocketFreeFn sym_nl_socket_free;
static NlSocketModifyCbFn sym_nl_socket_modify_cb;
static NlConnectFn sym_nl_connect;
static NlCloseFn sym_nl_close;
static NlmsgAllocFn sym_nlmsg_alloc;
static NlmsgHdrFn sym_nlmsg_hdr;
static NlSendSyncFn sym_nl_send_sync;

/* Values from libnl's enum nl_cb_type and enum nl_cb_kind. Only these two
 * are used, so the full enums are not replicated. */
enum {
   NL_CB_VALID = 0,
   NL_CB_CUSTOM = 3,
};

/* Netlink attribute helpers, from libnl's netlink/attr.h. The kernel headers
 * provide struct nlattr but not these accessors. */
#define NLA_ALIGNTO 4
#define NLA_ALIGN(len) (((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define NLA_HDRLEN ((int) NLA_ALIGN(sizeof(struct nlattr)))
#define NLA_DATA(nla) ((void*)((char*)(nla) + NLA_HDRLEN))
#define NLA_LEN(nla) ((nla)->nla_len - NLA_HDRLEN)
#define NLA_OK(nla, len) ((len) >= (int)sizeof(struct nlattr) && (nla)->nla_len >= sizeof(struct nlattr) && (nla)->nla_len <= (len))
#define NLA_NEXT(nla, len) ((len) -= NLA_ALIGN((nla)->nla_len), (struct nlattr*)((char*)(nla) + NLA_ALIGN((nla)->nla_len)))

/* The TCP state constants are not exported to userspace by linux/tcp.h. */
#ifndef TCP_LISTEN
#define TCP_LISTEN 10
#endif

#ifndef LIBNL3_LIBDIR
#define LIBNL3_LIBDIR ""
#endif

/* How often the inode -> process map is rebuilt from /proc */
#define NETLINK_INODE_RESCAN_MS 5000

typedef struct SocketState_ {
   /* Cumulative counter as of the last dump this socket appeared in.
    * For TCP this is tcp_info bytes received / acked, for UDP and ICMP
    * it is the instantaneous queue size. */
   unsigned long long lastRx;
   unsigned long long lastTx;

   /* Dump round in which the socket was last seen, used to prune state
    * for sockets that have gone away. */
   unsigned int round;
} SocketState;

typedef struct PidTotals_ {
   /* Cumulative bytes attributed to the process since tracking began */
   unsigned long long rx;
   unsigned long long tx;
} PidTotals;

static void* dlopenHandle = NULL;
static const char* dlopenName = NULL;
static struct nl_sock* diagSock = NULL;

/* inode -> tgid of the process owning the socket */
static Hashtable* inodePid = NULL;

/* inode -> per-socket counters */
static Hashtable* socketState = NULL;

/* tgid -> cumulative attributed bytes */
static Hashtable* pidTotals = NULL;

/* pids owning at least one socket in the current rescan; only valid
 * while NetLinkNet_rescanInodes runs */
static Hashtable* pidsPresent = NULL;

static unsigned int roundCounter = 0;
static uint64_t lastRescanMs = 0;

/* Protocol of the dump currently being processed */
static int gDumpProto = 0;

/* In release (NDEBUG) builds diagnostic output is compiled out entirely */
static void NetLinkNet_debug(const char* fmt, ...) ATTR_FORMAT(printf, 1, 2);
static void NetLinkNet_debug(const char* fmt, ...) {
#ifdef NDEBUG
   (void) fmt;
#else
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, "htop-netlink: ");
   vfprintf(stderr, fmt, ap);
   va_end(ap);
#endif
}

static inline unsigned long long saturatingAdd(unsigned long long a, unsigned long long b) {
   return a > ULLONG_MAX - b ? ULLONG_MAX : a + b;
}

static bool NetLinkNet_resolveLibnl(void) {
   if (dlopenHandle)
      return true;

   static const char* const libnl_candidates[] = {
      LIBNL3_LIBDIR "libnl-3.so.200",
      LIBNL3_LIBDIR "libnl-3.so.1",
      LIBNL3_LIBDIR "libnl-3.so",
      NULL,
   };
   const char* lastError = NULL;
   for (size_t i = 0; libnl_candidates[i]; i++) {
      dlopenHandle = dlopen(libnl_candidates[i], RTLD_LAZY);
      if (dlopenHandle) {
         dlopenName = libnl_candidates[i];
         break;
      }
      lastError = dlerror();
   }
   if (!dlopenHandle) {
      char* tried = String_join(", ", libnl_candidates);
      NetLinkNet_debug("netlink network monitoring needs the libnl-3 runtime library, none of %s could be loaded%s%s\n",
                       tried, lastError ? ": " : "", lastError ? lastError : "");
      free(tried);
      return false;
   }

   #define resolve(symbolname) do {                                          \
      dlerror();                                                             \
      *(void **)(&sym_##symbolname) = dlsym(dlopenHandle, #symbolname);      \
      if (!sym_##symbolname) {                                               \
         const char* e = dlerror();                                          \
         NetLinkNet_debug("%s is missing symbol %s%s%s\n",                   \
                          dlopenName, #symbolname, e ? ": " : "", e ? e : ""); \
         dlclose(dlopenHandle);                                              \
         dlopenHandle = NULL;                                                \
         return false;                                                       \
      }                                                                      \
   } while (0)

   resolve(nl_socket_alloc);
   resolve(nl_socket_free);
   resolve(nl_socket_modify_cb);
   resolve(nl_connect);
   resolve(nl_close);
   resolve(nlmsg_alloc);
   resolve(nlmsg_hdr);
   resolve(nl_send_sync);

   #undef resolve

   return true;
}

static void NetLinkNet_closeSocket(void) {
   if (!diagSock)
      return;

   sym_nl_close(diagSock);
   sym_nl_socket_free(diagSock);
   diagSock = NULL;
}

static unsigned int NetLinkNet_readTgid(pid_t pid) {
   char path[64];
   xSnprintf(path, sizeof(path), PROCDIR "/%d/status", (int) pid);

   FILE* fp = fopen(path, "re");
   if (!fp)
      return 0;

   unsigned int tgid = 0;
   char* line;
   while ((line = String_readLine(fp)) != NULL) {
      if (String_startsWith(line, "Tgid:")) {
         tgid = (unsigned int) strtoul(line + strlen("Tgid:"), NULL, 10);
         free(line);
         break;
      }
      free(line);
   }
   fclose(fp);
   return tgid;
}

static void NetLinkNet_collectStaleSocket(ht_key_t key, void* value, void* data) {
   const SocketState* state = (const SocketState*) value;
   Hashtable* stale = (Hashtable*) data;
   if (state->round != roundCounter)
      Hashtable_put(stale, key, (void*) 1);
}

static void NetLinkNet_dropStaleSocket(ht_key_t key, ATTR_UNUSED void* value, ATTR_UNUSED void* data) {
   free(Hashtable_remove(socketState, key));
}

static void NetLinkNet_pruneStaleSockets(void) {
   Hashtable* stale = Hashtable_new(64, false);
   Hashtable_foreach(socketState, NetLinkNet_collectStaleSocket, stale);
   Hashtable_foreach(stale, NetLinkNet_dropStaleSocket, NULL);
   Hashtable_delete(stale);
}

static void NetLinkNet_collectStalePid(ht_key_t key, ATTR_UNUSED void* value, void* data) {
   Hashtable* stale = (Hashtable*) data;
   if (!Hashtable_get(pidsPresent, key))
      Hashtable_put(stale, key, (void*) 1);
}

static void NetLinkNet_dropStalePid(ht_key_t key, ATTR_UNUSED void* value, ATTR_UNUSED void* data) {
   free(Hashtable_remove(pidTotals, key));
}

/* Rebuild inode -> tgid by walking /proc/<pid>/fd. Runs at a much lower
 * frequency than the socket dumps because it touches many files. */
static void NetLinkNet_rescanInodes(void) {
   DIR* procDir = opendir(PROCDIR);
   if (!procDir)
      return;

   Hashtable* freshInodePid = Hashtable_new(1024, false);
   pidsPresent = Hashtable_new(256, false);

   struct dirent* de;
   while ((de = readdir(procDir)) != NULL) {
      pid_t pid = (pid_t) strtol(de->d_name, NULL, 10);
      if (pid <= 0)
         continue;

      unsigned int tgid = NetLinkNet_readTgid(pid);
      if (!tgid)
         continue;

      if (!Hashtable_get(pidsPresent, tgid))
         Hashtable_put(pidsPresent, tgid, (void*) (uintptr_t) tgid);

      char fdPath[64];
      xSnprintf(fdPath, sizeof(fdPath), PROCDIR "/%s/fd", de->d_name);
      DIR* fdDir = opendir(fdPath);
      if (!fdDir)
         continue;

      int fdDirFd = dirfd(fdDir);
      struct dirent* fde;
      while ((fde = readdir(fdDir)) != NULL) {
         struct stat st;
         if (fstatat(fdDirFd, fde->d_name, &st, 0) != 0)
            continue;
         if (S_ISSOCK(st.st_mode))
            Hashtable_put(freshInodePid, (ht_key_t) st.st_ino, (void*) (uintptr_t) tgid);
      }
      closedir(fdDir);
   }
   closedir(procDir);

   if (inodePid)
      Hashtable_delete(inodePid);
   inodePid = freshInodePid;

   /* Drop totals of processes without sockets left; a recycled pid must
    * not inherit the counters of its previous holder. */
   if (pidTotals) {
      Hashtable* stale = Hashtable_new(64, false);
      Hashtable_foreach(pidTotals, NetLinkNet_collectStalePid, stale);
      Hashtable_foreach(stale, NetLinkNet_dropStalePid, NULL);
      Hashtable_delete(stale);
   }

   Hashtable_delete(pidsPresent);
   pidsPresent = NULL;
}

static void NetLinkNet_accumulate(unsigned int inode, unsigned long long rx, unsigned long long tx) {
   SocketState* state = Hashtable_get(socketState, inode);
   if (!state) {
      /* First sighting: remember the current counter as the baseline so a
       * long-lived socket does not contribute its whole history. */
      state = xCalloc(1, sizeof(SocketState));
      state->lastRx = rx;
      state->lastTx = tx;
      state->round = roundCounter;
      Hashtable_put(socketState, inode, state);
      return;
   }

   state->round = roundCounter;

   unsigned long long rxDelta = saturatingSub(rx, state->lastRx);
   unsigned long long txDelta = saturatingSub(tx, state->lastTx);
   state->lastRx = rx;
   state->lastTx = tx;

   if (!rxDelta && !txDelta)
      return;

   unsigned int pid = (unsigned int) (uintptr_t) Hashtable_get(inodePid, inode);
   if (!pid)
      return;

   PidTotals* totals = Hashtable_get(pidTotals, pid);
   if (!totals) {
      totals = xCalloc(1, sizeof(PidTotals));
      Hashtable_put(pidTotals, pid, totals);
   }
   totals->rx = saturatingAdd(totals->rx, rxDelta);
   totals->tx = saturatingAdd(totals->tx, txDelta);
}

static int NetLinkNet_validCb(struct nl_msg* msg, ATTR_UNUSED void* arg) {
   struct nlmsghdr* nlh = sym_nlmsg_hdr(msg);
   if (nlh->nlmsg_len < NLMSG_LENGTH((int)sizeof(struct inet_diag_msg)))
      return 0;

   struct inet_diag_msg* dm = NLMSG_DATA(nlh);

   /* No inode means a hash-table placeholder (e.g. TIME_WAIT), which has
    * no owning file descriptor and therefore no process. */
   if (!dm->idiag_inode)
      return 0;

   /* The listen queue is backlog, not traffic. */
   if (gDumpProto == IPPROTO_TCP && dm->idiag_state == TCP_LISTEN)
      return 0;

   unsigned char* attrdata = (unsigned char*) (dm + 1);
   int remaining = (int) (nlh->nlmsg_len - NLMSG_LENGTH((int)sizeof(*dm)));

   unsigned long long rx = 0;
   unsigned long long tx = 0;

   struct nlattr* attr;
   for (attr = (struct nlattr*) attrdata; NLA_OK(attr, remaining); attr = NLA_NEXT(attr, remaining)) {
      int type = attr->nla_type & 0x3fff;
      void* data = NLA_DATA(attr);
      int len = NLA_LEN(attr);

      if (gDumpProto == IPPROTO_TCP && type == INET_DIAG_INFO) {
         int offsetRx = (int) offsetof(struct tcp_info, tcpi_bytes_received);
         int offsetTx = (int) offsetof(struct tcp_info, tcpi_bytes_acked);
         if (len >= offsetRx + (int)sizeof(uint64_t))
            memcpy(&rx, (char*) data + offsetRx, sizeof(uint64_t));
         if (len >= offsetTx + (int)sizeof(uint64_t))
            memcpy(&tx, (char*) data + offsetTx, sizeof(uint64_t));
      } else if (gDumpProto != IPPROTO_TCP && type == INET_DIAG_MEMINFO) {
         uint32_t* mem = (uint32_t*) data;
         if (len >= (int)(5 * sizeof(uint32_t))) {
            /* struct sk_meminfo: [0] rmem_alloc, [3] wmem_queued */
            rx = mem[0];
            tx = mem[3];
         } else if (len >= (int)(2 * sizeof(uint32_t))) {
            /* struct inet_diag_meminfo: [0] idiag_rmem, [1] idiag_wmem */
            rx = mem[0];
            tx = mem[1];
         }
      }
   }

   NetLinkNet_accumulate(dm->idiag_inode, rx, tx);
   return 0;
}

static int NetLinkNet_dumpOnce(int family, int proto) {
   struct nl_msg* msg = sym_nlmsg_alloc();
   if (!msg)
      return -1;

   struct nlmsghdr* nlh = sym_nlmsg_hdr(msg);
   struct inet_diag_req_v2* req = NLMSG_DATA(nlh);
   memset(req, 0, sizeof(*req));
   req->sdiag_family = (unsigned char) family;
   req->sdiag_protocol = (unsigned char) proto;
   req->idiag_ext = (1 << (INET_DIAG_MEMINFO - 1)) | (1 << (INET_DIAG_INFO - 1));
   req->idiag_states = ~0U;
   nlh->nlmsg_type = SOCK_DIAG_BY_FAMILY;
   nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
   nlh->nlmsg_len = NLMSG_LENGTH((int)sizeof(*req));

   gDumpProto = proto;

   /* nl_send_sync() takes ownership of the message and reads the entire dump
    * reply, dispatching every socket to the NL_CB_VALID callback above. */
   return sym_nl_send_sync(diagSock, msg);
}

/* ICMP has no sock_diag handler, so the queue sizes are read straight from
 * /proc/net/icmp{6}. The columns are whitespace separated; the queue field
 * is "tx_queue:rx_queue" and the inode is the tenth column. The values are
 * socket buffer charges rather than exact byte counts. */
static void NetLinkNet_parseProcNetIcmp(const char* path) {
   FILE* fp = fopen(path, "re");
   if (!fp)
      return;

   char line[512];
   while (fgets(line, sizeof(line), fp)) {
      char* tokens[10];
      int n = 0;
      char* save = NULL;
      for (char* token = strtok_r(line, " \t\n", &save); token && n < 10; token = strtok_r(NULL, " \t\n", &save))
         tokens[n++] = token;
      if (n < 10)
         continue;
      if (tokens[0][0] < '0' || tokens[0][0] > '9')
         continue; /* header line */

      char* colon = strchr(tokens[4], ':');
      if (!colon)
         continue;

      *colon = '\0';
      unsigned long long tx = strtoull(tokens[4], NULL, 16);
      unsigned long long rx = strtoull(colon + 1, NULL, 16);
      *colon = ':';

      unsigned int inode = (unsigned int) strtoul(tokens[9], NULL, 10);
      if (!inode)
         continue;

      NetLinkNet_accumulate(inode, rx, tx);
   }
   fclose(fp);
}

static bool NetLinkNet_open(void) {
   if (!NetLinkNet_resolveLibnl())
      return false;

   diagSock = sym_nl_socket_alloc();
   if (!diagSock)
      return false;

   if (sym_nl_socket_modify_cb(diagSock, NL_CB_VALID, NL_CB_CUSTOM, NetLinkNet_validCb, NULL) != 0) {
      NetLinkNet_closeSocket();
      return false;
   }

   if (sym_nl_connect(diagSock, NETLINK_SOCK_DIAG) < 0) {
      int e = errno;
      NetLinkNet_debug("cannot connect to NETLINK_SOCK_DIAG%s%s\n", e ? ": " : "", e ? strerror(e) : "");
      NetLinkNet_closeSocket();
      return false;
   }

   NetLinkNet_debug("netlink network monitoring active via %s\n", dlopenName);
   return true;
}

void NetLinkNet_init(void) {
   diagSock = NULL;
   inodePid = NULL;
   socketState = NULL;
   pidTotals = NULL;
   pidsPresent = NULL;
   roundCounter = 0;
   lastRescanMs = 0;
}

void NetLinkNet_done(void) {
   NetLinkNet_closeSocket();

   if (dlopenHandle) {
      dlclose(dlopenHandle);
      dlopenHandle = NULL;
   }

   if (inodePid) {
      Hashtable_delete(inodePid);
      inodePid = NULL;
   }
   if (socketState) {
      Hashtable_delete(socketState);
      socketState = NULL;
   }
   if (pidTotals) {
      Hashtable_delete(pidTotals);
      pidTotals = NULL;
   }
   if (pidsPresent) {
      Hashtable_delete(pidsPresent);
      pidsPresent = NULL;
   }
}

bool NetLinkNet_isActive(void) {
   return diagSock != NULL;
}

void NetLinkNet_update(void) {
   if (!diagSock && !NetLinkNet_open())
      return;

   if (!socketState)
      socketState = Hashtable_new(512, true);
   if (!pidTotals)
      pidTotals = Hashtable_new(128, true);

   uint64_t nowMs;
   Platform_gettime_monotonic(&nowMs);
   if (!inodePid || nowMs - lastRescanMs >= NETLINK_INODE_RESCAN_MS) {
      NetLinkNet_rescanInodes();
      lastRescanMs = nowMs;
   }

   roundCounter++;

   static const struct {
      int family;
      int proto;
   } combos[] = {
      { AF_INET,  IPPROTO_TCP },
      { AF_INET,  IPPROTO_UDP },
      { AF_INET6, IPPROTO_TCP },
      { AF_INET6, IPPROTO_UDP },
   };

   bool complete = true;
   for (size_t i = 0; i < ARRAYSIZE(combos); i++) {
      if (NetLinkNet_dumpOnce(combos[i].family, combos[i].proto) < 0) {
         complete = false;
         break;
      }
   }

   NetLinkNet_parseProcNetIcmp(PROCDIR "/net/icmp");
   NetLinkNet_parseProcNetIcmp(PROCDIR "/net/icmp6");

   if (complete) {
      NetLinkNet_pruneStaleSockets();
   } else {
      /* A failed dump can leave replies queued; start from a clean socket
       * so the next update is not confused by the previous stream. */
      NetLinkNet_closeSocket();
   }
}

bool NetLinkNet_getNetBytes(pid_t pid, unsigned long long* rx, unsigned long long* tx) {
   if (!pidTotals)
      return false;

   PidTotals* totals = Hashtable_get(pidTotals, (ht_key_t) pid);
   if (!totals)
      return false;

   *rx = totals->rx;
   *tx = totals->tx;
   return true;
}

#endif /* HAVE_LIBNL_NET */
