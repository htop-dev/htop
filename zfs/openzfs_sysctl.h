#ifndef HEADER_openzfs_sysctl
#define HEADER_openzfs_sysctl
/*
htop - zfs/openzfs_sysctl.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "zfs/ZfsArcStats.h"

void openzfs_sysctl_init(ZfsArcStats *stats);

void openzfs_sysctl_updateArcStats(ZfsArcStats *stats);

#endif
