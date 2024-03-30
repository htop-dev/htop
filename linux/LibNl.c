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

#include <linux/netlink.h>
#include <linux/taskstats.h>

#include <netlink/attr.h>
#include <netlink/handlers.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>


static void initNetlinkSocket(LinuxProcessTable* this) {
   this->netlink_socket = nl_socket_alloc();
   if (this->netlink_socket == NULL) {
      return;
   }
   if (nl_connect(this->netlink_socket, NETLINK_GENERIC) < 0) {
      return;
   }
   this->netlink_family = genl_ctrl_resolve(this->netlink_socket, TASKSTATS_GENL_NAME);
}

void LibNl_destroyNetlinkSocket(LinuxProcessTable* this) {
   if (!this->netlink_socket)
      return;

   nl_close(this->netlink_socket);
   nl_socket_free(this->netlink_socket);
   this->netlink_socket = NULL;
}

static int handleNetlinkMsg(struct nl_msg* nlmsg, void* linuxProcess) {
   struct nlmsghdr* nlhdr;
   struct nlattr* nlattrs[TASKSTATS_TYPE_MAX + 1];
   const struct nlattr* nlattr;
   struct taskstats stats;
   int rem;
   LinuxProcess* lp = (LinuxProcess*) linuxProcess;

   nlhdr = nlmsg_hdr(nlmsg);

   if (genlmsg_parse(nlhdr, 0, nlattrs, TASKSTATS_TYPE_MAX, NULL) < 0) {
      return NL_SKIP;
   }

   if ((nlattr = nlattrs[TASKSTATS_TYPE_AGGR_PID]) || (nlattr = nlattrs[TASKSTATS_TYPE_NULL])) {
      memcpy(&stats, nla_data(nla_next(nla_data(nlattr), &rem)), sizeof(stats));
      assert(Process_getPid(&lp->super) == (pid_t)stats.ac_pid);

      // The xxx_delay_total values wrap around on overflow.
      // (Linux Kernel "Documentation/accounting/taskstats-struct.rst")
      unsigned long long int timeDelta = stats.ac_etime * 1000 - lp->delay_read_time;
      #define DELTAPERC(x, y) (timeDelta ? MINIMUM((float)((x) - (y)) / timeDelta * 100.0f, 100.0f) : NAN)
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

void LibNl_readDelayAcctData(LinuxProcessTable* this, LinuxProcess* process) {
   struct nl_msg* msg;

   if (!this->netlink_socket) {
      initNetlinkSocket(this);
      if (!this->netlink_socket) {
         goto delayacct_failure;
      }
   }

   if (nl_socket_modify_cb(this->netlink_socket, NL_CB_VALID, NL_CB_CUSTOM, handleNetlinkMsg, process) < 0) {
      goto delayacct_failure;
   }

   if (! (msg = nlmsg_alloc())) {
      goto delayacct_failure;
   }

   if (! genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, this->netlink_family, 0, NLM_F_REQUEST, TASKSTATS_CMD_GET, TASKSTATS_VERSION)) {
      nlmsg_free(msg);
   }

   if (nla_put_u32(msg, TASKSTATS_CMD_ATTR_PID, Process_getPid(&process->super)) < 0) {
      nlmsg_free(msg);
   }

   if (nl_send_sync(this->netlink_socket, msg) < 0) {
      goto delayacct_failure;
   }

   if (nl_recvmsgs_default(this->netlink_socket) < 0) {
      goto delayacct_failure;
   }

   return;

delayacct_failure:
   process->swapin_delay_percent = NAN;
   process->blkio_delay_percent = NAN;
   process->cpu_delay_percent = NAN;
}
