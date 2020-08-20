/*
htop - zfs/openzfs_sysctl.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "zfs/openzfs_sysctl.h"
#include "zfs/ZfsArcStats.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>

static int MIB_kstat_zfs_misc_arcstats_size[5];
static int MIB_kstat_zfs_misc_arcstats_c_max[5];
static int MIB_kstat_zfs_misc_arcstats_mfu_size[5];
static int MIB_kstat_zfs_misc_arcstats_mru_size[5];
static int MIB_kstat_zfs_misc_arcstats_anon_size[5];
static int MIB_kstat_zfs_misc_arcstats_hdr_size[5];
static int MIB_kstat_zfs_misc_arcstats_other_size[5];
static int MIB_kstat_zfs_misc_arcstats_compressed_size[5];
static int MIB_kstat_zfs_misc_arcstats_uncompressed_size[5];

/*{
#include "zfs/ZfsArcStats.h"
}*/

void openzfs_sysctl_init(ZfsArcStats *stats) {
   size_t len;
   unsigned long long int arcSize;

   len = sizeof(arcSize);
   if (sysctlbyname("kstat.zfs.misc.arcstats.size", &arcSize, &len,
	    NULL, 0) == 0 && arcSize != 0) {
                  stats->enabled = 1;
                  len = 5; sysctlnametomib("kstat.zfs.misc.arcstats.size", MIB_kstat_zfs_misc_arcstats_size, &len);

                  sysctlnametomib("kstat.zfs.misc.arcstats.c_max", MIB_kstat_zfs_misc_arcstats_c_max, &len);
                  sysctlnametomib("kstat.zfs.misc.arcstats.mfu_size", MIB_kstat_zfs_misc_arcstats_mfu_size, &len);
                  sysctlnametomib("kstat.zfs.misc.arcstats.mru_size", MIB_kstat_zfs_misc_arcstats_mru_size, &len);
                  sysctlnametomib("kstat.zfs.misc.arcstats.anon_size", MIB_kstat_zfs_misc_arcstats_anon_size, &len);
                  sysctlnametomib("kstat.zfs.misc.arcstats.hdr_size", MIB_kstat_zfs_misc_arcstats_hdr_size, &len);
                  sysctlnametomib("kstat.zfs.misc.arcstats.other_size", MIB_kstat_zfs_misc_arcstats_other_size, &len);
                  if (sysctlnametomib("kstat.zfs.misc.arcstats.compressed_size", MIB_kstat_zfs_misc_arcstats_compressed_size, &len) == 0) {
                     stats->isCompressed = 1;
                     sysctlnametomib("kstat.zfs.misc.arcstats.uncompressed_size", MIB_kstat_zfs_misc_arcstats_uncompressed_size, &len);
                  } else {
                     stats->isCompressed = 0;
                  }
   } else {
      stats->enabled = 0;
   }
}

void openzfs_sysctl_updateArcStats(ZfsArcStats *stats) {
   size_t len;

   if (stats->enabled) {
      len = sizeof(stats->size);
      sysctl(MIB_kstat_zfs_misc_arcstats_size, 5, &(stats->size), &len , NULL, 0);
      stats->size /= 1024;

      len = sizeof(stats->max);
      sysctl(MIB_kstat_zfs_misc_arcstats_c_max, 5, &(stats->max), &len , NULL, 0);
      stats->max /= 1024;

      len = sizeof(stats->MFU);
      sysctl(MIB_kstat_zfs_misc_arcstats_mfu_size, 5, &(stats->MFU), &len , NULL, 0);
      stats->MFU /= 1024;

      len = sizeof(stats->MRU);
      sysctl(MIB_kstat_zfs_misc_arcstats_mru_size, 5, &(stats->MRU), &len , NULL, 0);
      stats->MRU /= 1024;

      len = sizeof(stats->anon);
      sysctl(MIB_kstat_zfs_misc_arcstats_anon_size, 5, &(stats->anon), &len , NULL, 0);
      stats->anon /= 1024;

      len = sizeof(stats->header);
      sysctl(MIB_kstat_zfs_misc_arcstats_hdr_size, 5, &(stats->header), &len , NULL, 0);
      stats->header /= 1024;

      len = sizeof(stats->other);
      sysctl(MIB_kstat_zfs_misc_arcstats_other_size, 5, &(stats->other), &len , NULL, 0);
      stats->other /= 1024;

      if (stats->isCompressed) {
         len = sizeof(stats->compressed);
         sysctl(MIB_kstat_zfs_misc_arcstats_compressed_size, 5, &(stats->compressed), &len , NULL, 0);
         stats->compressed /= 1024;

         len = sizeof(stats->uncompressed);
         sysctl(MIB_kstat_zfs_misc_arcstats_uncompressed_size, 5, &(stats->uncompressed), &len , NULL, 0);
         stats->uncompressed /= 1024;
      }
   }
}
