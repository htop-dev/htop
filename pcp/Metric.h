#ifndef HEADER_Metric
#define HEADER_Metric
/*
htop - Metric.h
(C) 2020-2021 htop dev team
(C) 2020-2021 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <ctype.h>
#include <stdbool.h>
#include <pcp/pmapi.h>
#include <sys/time.h>

/* use htop config.h values for these macros, not pcp values */
#undef PACKAGE_URL
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT


typedef enum Metric_ {
   PCP_CONTROL_THREADS,         /* proc.control.perclient.threads */

   PCP_HINV_NCPU,               /* hinv.ncpu */
   PCP_HINV_NDISK,              /* hinv.ndisk */
   PCP_HINV_CPUCLOCK,           /* hinv.cpu.clock */
   PCP_UNAME_SYSNAME,           /* kernel.uname.sysname */
   PCP_UNAME_RELEASE,           /* kernel.uname.release */
   PCP_UNAME_MACHINE,           /* kernel.uname.machine */
   PCP_UNAME_DISTRO,            /* kernel.uname.distro */
   PCP_LOAD_AVERAGE,            /* kernel.all.load */
   PCP_PID_MAX,                 /* kernel.all.pid_max */
   PCP_UPTIME,                  /* kernel.all.uptime */
   PCP_BOOTTIME,                /* kernel.all.boottime */
   PCP_CPU_USER,                /* kernel.all.cpu.user */
   PCP_CPU_NICE,                /* kernel.all.cpu.nice */
   PCP_CPU_SYSTEM,              /* kernel.all.cpu.sys */
   PCP_CPU_IDLE,                /* kernel.all.cpu.idle */
   PCP_CPU_IOWAIT,              /* kernel.all.cpu.wait.total */
   PCP_CPU_IRQ,                 /* kernel.all.cpu.intr */
   PCP_CPU_SOFTIRQ,             /* kernel.all.cpu.irq.soft */
   PCP_CPU_STEAL,               /* kernel.all.cpu.steal */
   PCP_CPU_GUEST,               /* kernel.all.cpu.guest */
   PCP_CPU_GUESTNICE,           /* kernel.all.cpu.guest_nice */
   PCP_PERCPU_USER,             /* kernel.percpu.cpu.user */
   PCP_PERCPU_NICE,             /* kernel.percpu.cpu.nice */
   PCP_PERCPU_SYSTEM,           /* kernel.percpu.cpu.sys */
   PCP_PERCPU_IDLE,             /* kernel.percpu.cpu.idle */
   PCP_PERCPU_IOWAIT,           /* kernel.percpu.cpu.wait.total */
   PCP_PERCPU_IRQ,              /* kernel.percpu.cpu.intr */
   PCP_PERCPU_SOFTIRQ,          /* kernel.percpu.cpu.irq.soft */
   PCP_PERCPU_STEAL,            /* kernel.percpu.cpu.steal */
   PCP_PERCPU_GUEST,            /* kernel.percpu.cpu.guest */
   PCP_PERCPU_GUESTNICE,        /* kernel.percpu.cpu.guest_nice */
   PCP_MEM_TOTAL,               /* mem.physmem */
   PCP_MEM_FREE,                /* mem.util.free */
   PCP_MEM_ACTIVE,              /* mem.util.active */
   PCP_MEM_AVAILABLE,           /* mem.util.available */
   PCP_MEM_BUFFERS,             /* mem.util.bufmem */
   PCP_MEM_CACHED,              /* mem.util.cached */
   PCP_MEM_COMPRESSED,          /* mem.util.compressed */
   PCP_MEM_EXTERNAL,            /* mem.util.external */
   PCP_MEM_INACTIVE,            /* mem.util.inactive */
   PCP_MEM_SHARED,              /* mem.util.shared */
   PCP_MEM_PURGEABLE,           /* mem.util.purgeable */
   PCP_MEM_SPECULATIVE,         /* mem.util.speculative */
   PCP_MEM_SRECLAIM,            /* mem.util.slabReclaimable */
   PCP_MEM_WIRED,               /* mem.util.wired */
   PCP_MEM_SWAPCACHED,          /* mem.util.swapCached */
   PCP_MEM_SWAPTOTAL,           /* mem.util.swapTotal */
   PCP_MEM_SWAPFREE,            /* mem.util.swapFree */
   PCP_DISK_READB,              /* disk.all.read_bytes */
   PCP_DISK_WRITEB,             /* disk.all.write_bytes */
   PCP_DISK_ACTIVE,             /* disk.all.avactive */
   PCP_NET_RECVB,               /* network.all.in.bytes */
   PCP_NET_SENDB,               /* network.all.out.bytes */
   PCP_NET_RECVP,               /* network.all.in.packets */
   PCP_NET_SENDP,               /* network.all.out.packets */
   PCP_PSI_CPUSOME,             /* kernel.all.pressure.cpu.some.avg */
   PCP_PSI_IOSOME,              /* kernel.all.pressure.io.some.avg */
   PCP_PSI_IOFULL,              /* kernel.all.pressure.io.full.avg */
   PCP_PSI_IRQFULL,             /* kernel.all.pressure.irq.full.avg */
   PCP_PSI_MEMSOME,             /* kernel.all.pressure.memory.some.avg */
   PCP_PSI_MEMFULL,             /* kernel.all.pressure.memory.full.avg */
   PCP_ZFS_ARC_ANON_SIZE,       /* zfs.arc.anon_size */
   PCP_ZFS_ARC_BONUS_SIZE,      /* zfs.arc.bonus_size */
   PCP_ZFS_ARC_COMPRESSED_SIZE, /* zfs.arc.compressed_size */
   PCP_ZFS_ARC_UNCOMPRESSED_SIZE, /* zfs.arc.uncompressed_size */
   PCP_ZFS_ARC_C_MIN,           /* zfs.arc.c_min */
   PCP_ZFS_ARC_C_MAX,           /* zfs.arc.c_max */
   PCP_ZFS_ARC_DBUF_SIZE,       /* zfs.arc.dbuf_size */
   PCP_ZFS_ARC_DNODE_SIZE,      /* zfs.arc.dnode_size */
   PCP_ZFS_ARC_HDR_SIZE,        /* zfs.arc.hdr_size */
   PCP_ZFS_ARC_MFU_SIZE,        /* zfs.arc.mfu_size */
   PCP_ZFS_ARC_MRU_SIZE,        /* zfs.arc.mru_size */
   PCP_ZFS_ARC_SIZE,            /* zfs.arc.size */
   PCP_ZRAM_CAPACITY,           /* zram.capacity */
   PCP_ZRAM_ORIGINAL,           /* zram.mm_stat.data_size.original */
   PCP_ZRAM_COMPRESSED,         /* zram.mm_stat.data_size.compressed */
   PCP_MEM_ZSWAP,               /* mem.util.zswap */
   PCP_MEM_ZSWAPPED,            /* mem.util.zswapped */
   PCP_VFS_FILES_COUNT,         /* vfs.files.count */
   PCP_VFS_FILES_MAX,           /* vfs.files.max */

   PCP_PROC_PID,                /* proc.psinfo.pid */
   PCP_PROC_PPID,               /* proc.psinfo.ppid */
   PCP_PROC_TGID,               /* proc.psinfo.tgid */
   PCP_PROC_PGRP,               /* proc.psinfo.pgrp */
   PCP_PROC_SESSION,            /* proc.psinfo.session */
   PCP_PROC_STATE,              /* proc.psinfo.sname */
   PCP_PROC_TTY,                /* proc.psinfo.tty */
   PCP_PROC_TTYPGRP,            /* proc.psinfo.tty_pgrp */
   PCP_PROC_MINFLT,             /* proc.psinfo.minflt */
   PCP_PROC_MAJFLT,             /* proc.psinfo.maj_flt */
   PCP_PROC_CMINFLT,            /* proc.psinfo.cmin_flt */
   PCP_PROC_CMAJFLT,            /* proc.psinfo.cmaj_flt */
   PCP_PROC_UTIME,              /* proc.psinfo.utime */
   PCP_PROC_STIME,              /* proc.psinfo.stime */
   PCP_PROC_CUTIME,             /* proc.psinfo.cutime */
   PCP_PROC_CSTIME,             /* proc.psinfo.cstime */
   PCP_PROC_PRIORITY,           /* proc.psinfo.priority */
   PCP_PROC_NICE,               /* proc.psinfo.nice */
   PCP_PROC_THREADS,            /* proc.psinfo.threads */
   PCP_PROC_STARTTIME,          /* proc.psinfo.start_time */
   PCP_PROC_PROCESSOR,          /* proc.psinfo.processor */
   PCP_PROC_CMD,                /* proc.psinfo.cmd */
   PCP_PROC_PSARGS,             /* proc.psinfo.psargs */
   PCP_PROC_CGROUPS,            /* proc.psinfo.cgroups */
   PCP_PROC_OOMSCORE,           /* proc.psinfo.oom_score */
   PCP_PROC_VCTXSW,             /* proc.psinfo.vctxsw */
   PCP_PROC_NVCTXSW,            /* proc.psinfo.nvctxsw */
   PCP_PROC_LABELS,             /* proc.psinfo.labels */
   PCP_PROC_ENVIRON,            /* proc.psinfo.environ */
   PCP_PROC_TTYNAME,            /* proc.psinfo.ttyname */
   PCP_PROC_EXE,                /* proc.psinfo.exe */
   PCP_PROC_CWD,                /* proc.psinfo.cwd */

   PCP_PROC_AUTOGROUP_ID,       /* proc.autogroup.id */
   PCP_PROC_AUTOGROUP_NICE,     /* proc.autogroup.nice */

   PCP_PROC_ID_UID,             /* proc.id.uid */
   PCP_PROC_ID_USER,            /* proc.id.uid_nm */

   PCP_PROC_IO_RCHAR,           /* proc.io.rchar */
   PCP_PROC_IO_WCHAR,           /* proc.io.wchar */
   PCP_PROC_IO_SYSCR,           /* proc.io.syscr */
   PCP_PROC_IO_SYSCW,           /* proc.io.syscw */
   PCP_PROC_IO_READB,           /* proc.io.read_bytes */
   PCP_PROC_IO_WRITEB,          /* proc.io.write_bytes */
   PCP_PROC_IO_CANCELLED,       /* proc.io.cancelled_write_bytes */

   PCP_PROC_MEM_SIZE,           /* proc.memory.size */
   PCP_PROC_MEM_RSS,            /* proc.memory.rss */
   PCP_PROC_MEM_SHARE,          /* proc.memory.share */
   PCP_PROC_MEM_TEXTRS,         /* proc.memory.textrss */
   PCP_PROC_MEM_LIBRS,          /* proc.memory.librss */
   PCP_PROC_MEM_DATRS,          /* proc.memory.datrss */
   PCP_PROC_MEM_DIRTY,          /* proc.memory.dirty */

   PCP_PROC_SMAPS_PSS,          /* proc.smaps.pss */
   PCP_PROC_SMAPS_SWAP,         /* proc.smaps.swap */
   PCP_PROC_SMAPS_SWAPPSS,      /* proc.smaps.swappss */

   PCP_METRIC_COUNT             /* total metric count */
} Metric;

void Metric_enable(Metric metric, bool enable);

bool Metric_enabled(Metric metric);

void Metric_enableThreads(void);

bool Metric_fetch(struct timeval* timestamp);

bool Metric_iterate(Metric metric, int* instp, int* offsetp);

pmAtomValue* Metric_values(Metric metric, pmAtomValue* atom, int count, int type);

const pmDesc* Metric_desc(Metric metric);

static inline Metric Metric_fromId(size_t id) { return (Metric)id; }

int Metric_type(Metric metric);

int Metric_instanceCount(Metric metric);

int Metric_instanceOffset(Metric metric, int inst);

pmAtomValue* Metric_instance(Metric metric, int inst, int offset, pmAtomValue* atom, int type);

pmAtomValue* Metric_instance_kibibytes(Metric metric, int inst, int offset, pmAtomValue* atom);

pmAtomValue* Metric_instance_milliseconds(Metric metric, int inst, int offset, pmAtomValue* atom);

void Metric_externalName(Metric metric, int inst, char** externalName);

int Metric_lookupText(const char* metric, char** desc);

#endif
