/*
htop - SELinuxMeter.c
(C) 2020 Christian Goettsche
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "SELinuxMeter.h"

#include "CRT.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/magic.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>

#include "Macros.h"
#include "Object.h"
#include "XUtils.h"


static const int SELinuxMeter_attributes[] = {
   METER_TEXT,
};

static bool enabled = false;
static bool enforcing = false;

static bool hasSELinuxMount(void) {
   struct statfs sfbuf;
   int r = statfs("/sys/fs/selinux", &sfbuf);
   if (r != 0)
      return false;

   if (sfbuf.f_type != SELINUX_MAGIC)
      return false;

   struct statvfs vfsbuf;
   r = statvfs("/sys/fs/selinux", &vfsbuf);
   if (r != 0 || (vfsbuf.f_flag & ST_RDONLY))
      return false;

   return true;
}

static bool isSelinuxEnabled(void) {
   return hasSELinuxMount() && (0 == access("/etc/selinux/config", F_OK));
}

static bool isSelinuxEnforcing(void) {
   if (!enabled)
      return false;

   int fd = open("/sys/fs/selinux/enforce", O_RDONLY);
   if (fd < 0)
      return false;

   char buf[20] = {0};
   int r = read(fd, buf, sizeof(buf) - 1);
   close(fd);
   if (r < 0)
      return false;

   int enforce = 0;
   if (sscanf(buf, "%d", &enforce) != 1)
      return false;

   return !!enforce;
}

static void SELinuxMeter_updateValues(ATTR_UNUSED Meter* this, char* buffer, int len) {
   enabled = isSelinuxEnabled();
   enforcing = isSelinuxEnforcing();

   xSnprintf(buffer, len, "%s%s", enabled ? "enabled" : "disabled", enabled ? (enforcing ? "; mode: enforcing" : "; mode: permissive") : "");
}

const MeterClass SELinuxMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
   },
   .updateValues = SELinuxMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = SELinuxMeter_attributes,
   .name = "SELinux",
   .uiName = "SELinux",
   .description = "SELinux state overview",
   .caption = "SELinux: "
};
