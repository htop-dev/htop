/*
htop - linux/LibNl.c
(C) 2024 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#ifndef HAVE_DELAYACCT
#error Compiling this file requires HAVE_DELAYACCT
#endif

#include "linux/LibNl.h"

#include <dlfcn.h>

#include <linux/netlink.h>
#include <linux/taskstats.h>

#include <netlink/attr.h>
#include <netlink/handlers.h>
#include <netlink/msg.h>


static void* libnlHandle;
static void* libnlGenlHandle;

static void (*sym_nl_close)(struct nl_sock*);
static int (*sym_nl_connect)(struct nl_sock*, int);
static int (*sym_nl_recvmsgs_default)(struct nl_sock*);
static int (*sym_nl_send_sync)(struct nl_sock*, struct nl_msg*);
static struct nl_sock* (*sym_nl_socket_alloc)(void);
static void (*sym_nl_socket_free)(struct nl_sock*);
static int (*sym_nl_socket_modify_cb)(struct nl_sock*, enum nl_cb_type, enum nl_cb_kind, nl_recvmsg_msg_cb_t, void*);
static void* (*sym_nla_data)(const struct nlattr*);
static struct nlattr* (*sym_nla_next)(const struct nlattr*, int*);
static int (*sym_nla_put_u32)(struct nl_msg*, int, uint32_t);
static struct nl_msg* (*sym_nlmsg_alloc)(void);
static struct nlmsghdr* (*sym_nlmsg_hdr)(struct nl_msg*);
static void (*sym_nlmsg_free)(struct nl_msg*);

static int (*sym_genl_ctrl_resolve)(struct nl_sock*, const char*);
static int (*sym_genlmsg_parse)(struct nlmsghdr*, int, struct nlattr**, int, const struct nla_policy*);
static void* (*sym_genlmsg_put)(struct nl_msg*, uint32_t, uint32_t, int, int, int, uint8_t, uint8_t);


static void unload_libnl(void) {
   sym_nl_close = NULL;
   sym_nl_connect = NULL;
   sym_nl_recvmsgs_default = NULL;
   sym_nl_send_sync = NULL;
   sym_nl_socket_alloc = NULL;
   sym_nl_socket_free = NULL;
   sym_nl_socket_modify_cb = NULL;
   sym_nla_data = NULL;
   sym_nla_next = NULL;
   sym_nla_put_u32 = NULL;
   sym_nlmsg_alloc = NULL;
   sym_nlmsg_free = NULL;
   sym_nlmsg_hdr = NULL;

   sym_genl_ctrl_resolve = NULL;
   sym_genlmsg_parse = NULL;
   sym_genlmsg_put = NULL;

   if (libnlGenlHandle) {
      dlclose(libnlGenlHandle);
      libnlGenlHandle = NULL;
   }
   if (libnlHandle) {
      dlclose(libnlHandle);
      libnlHandle = NULL;
   }
}

static int load_libnl(void) {
   if (libnlHandle && libnlGenlHandle)
      return 0;

   libnlHandle = dlopen("libnl-3.so", RTLD_LAZY);
   if (!libnlHandle) {
      libnlHandle = dlopen("libnl-3.so.200", RTLD_LAZY);
      if (!libnlHandle) {
         goto dlfailure;
      }
   }

   libnlGenlHandle = dlopen("libnl-genl-3.so", RTLD_LAZY);
   if (!libnlGenlHandle) {
      libnlGenlHandle = dlopen("libnl-genl-3.so.200", RTLD_LAZY);
      if (!libnlGenlHandle) {
         goto dlfailure;
      }
   }

   /* Clear any errors */
   dlerror();

   #define resolve(handle, symbolname) do {                              \
      *(void **)(&sym_##symbolname) = dlsym(handle, #symbolname);        \
      if (!sym_##symbolname || dlerror() != NULL) {                      \
         goto dlfailure;                                                 \
      }                                                                  \
   } while(0)

   resolve(libnlHandle, nl_close);
   resolve(libnlHandle, nl_connect);
   resolve(libnlHandle, nl_recvmsgs_default);
   resolve(libnlHandle, nl_send_sync);
   resolve(libnlHandle, nl_socket_alloc);
   resolve(libnlHandle, nl_socket_free);
   resolve(libnlHandle, nl_socket_modify_cb);
   resolve(libnlHandle, nla_data);
   resolve(libnlHandle, nla_next);
   resolve(libnlHandle, nla_put_u32);
   resolve(libnlHandle, nlmsg_alloc);
   resolve(libnlHandle, nlmsg_free);
   resolve(libnlHandle, nlmsg_hdr);

   resolve(libnlGenlHandle, genl_ctrl_resolve);
   resolve(libnlGenlHandle, genlmsg_parse);
   resolve(libnlGenlHandle, genlmsg_put);

   #undef resolve

   return 0;

dlfailure:
   unload_libnl();
   return -1;
}

static void initNetlinkSocket(LinuxProcessTable* this) {
   if (load_libnl() < 0) {
      return;
   }

   this->netlink_socket = sym_nl_socket_alloc();
   if (this->netlink_socket == NULL) {
      return;
   }
   if (sym_nl_connect(this->netlink_socket, NETLINK_GENERIC) < 0) {
      return;
   }
   this->netlink_family = sym_genl_ctrl_resolve(this->netlink_socket, TASKSTATS_GENL_NAME);
}

void LibNl_destroyNetlinkSocket(LinuxProcessTable* this) {
   if (this->netlink_socket) {
      assert(libnlHandle);

      sym_nl_close(this->netlink_socket);
      sym_nl_socket_free(this->netlink_socket);
      this->netlink_socket = NULL;
   }

   unload_libnl();
}

static int handleNetlinkMsg(struct nl_msg* nlmsg, void* linuxProcess) {
   struct nlmsghdr* nlhdr;
   struct nlattr* nlattrs[TASKSTATS_TYPE_MAX + 1];
   const struct nlattr* nlattr;
   struct taskstats stats;
   int rem;
   LinuxProcess* lp = (LinuxProcess*) linuxProcess;

   nlhdr = sym_nlmsg_hdr(nlmsg);

   if (sym_genlmsg_parse(nlhdr, 0, nlattrs, TASKSTATS_TYPE_MAX, NULL) < 0) {
      return NL_SKIP;
   }

   if ((nlattr = nlattrs[TASKSTATS_TYPE_AGGR_PID]) || (nlattr = nlattrs[TASKSTATS_TYPE_NULL])) {
      memcpy(&stats, sym_nla_data(sym_nla_next(sym_nla_data(nlattr), &rem)), sizeof(stats));
      assert(Process_getPid(&lp->super) == (pid_t)stats.ac_pid);

      // The xxx_delay_total values wrap around on overflow.
      // (Linux Kernel "Documentation/accounting/taskstats-struct.rst")
      unsigned long long int timeDelta = stats.ac_etime * 1000 - lp->delay_read_time;
      #define DELTAPERC(x, y) (timeDelta ? MINIMUM((float)((x) - (y)) / timeDelta * 100.0F, 100.0F) : NAN)
      lp->cpu_delay_percent = DELTAPERC(stats.cpu_delay_total, lp->cpu_delay_total);
      lp->blkio_delay_percent = DELTAPERC(stats.blkio_delay_total, lp->blkio_delay_total);
      lp->swapin_delay_percent = DELTAPERC(stats.swapin_delay_total, lp->swapin_delay_total);
      #undef DELTAPERC

      lp->swapin_delay_total = stats.swapin_delay_total;
      lp->blkio_delay_total = stats.blkio_delay_total;
      lp->cpu_delay_total = stats.cpu_delay_total;
      lp->delay_read_time = stats.ac_etime * 1000;
   }
   return NL_OK;
}

/*
 * Gather delay-accounting information (thread-specific data)
 */
void LibNl_readDelayAcctData(LinuxProcessTable* this, LinuxProcess* process) {
   struct nl_msg* msg;

   if (!this->netlink_socket) {
      initNetlinkSocket(this);
      if (!this->netlink_socket) {
         goto delayacct_failure;
      }
   }

   if (sym_nl_socket_modify_cb(this->netlink_socket, NL_CB_VALID, NL_CB_CUSTOM, handleNetlinkMsg, process) < 0) {
      goto delayacct_failure;
   }

   if (! (msg = sym_nlmsg_alloc())) {
      goto delayacct_failure;
   }

   if (! sym_genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, this->netlink_family, 0, NLM_F_REQUEST, TASKSTATS_CMD_GET, TASKSTATS_VERSION)) {
      sym_nlmsg_free(msg);
   }

   if (sym_nla_put_u32(msg, TASKSTATS_CMD_ATTR_PID, Process_getPid(&process->super)) < 0) {
      sym_nlmsg_free(msg);
   }

   if (sym_nl_send_sync(this->netlink_socket, msg) < 0) {
      goto delayacct_failure;
   }

   if (sym_nl_recvmsgs_default(this->netlink_socket) < 0) {
      goto delayacct_failure;
   }

   return;

delayacct_failure:
   process->swapin_delay_percent = NAN;
   process->blkio_delay_percent = NAN;
   process->cpu_delay_percent = NAN;
}
