/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2011 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright © 2010 IBM
 * See COPYING in top-level directory.
 */

#include <private/autogen/config.h>
#include <hwloc.h>
#include <hwloc/linux.h>
#include <private/misc.h>
#include <private/private.h>
#include <private/misc.h>
#include <private/debug.h>

#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#if defined HWLOC_HAVE_SET_MEMPOLICY || defined HWLOC_HAVE_MBIND
#define migratepages migrate_pages /* workaround broken migratepages prototype in numaif.h before libnuma 2.0.2 */
#include <numaif.h>
#endif

#if !defined(HWLOC_HAVE_CPU_SET) && !(defined(HWLOC_HAVE_CPU_SET_S) && !defined(HWLOC_HAVE_OLD_SCHED_SETAFFINITY)) && defined(HWLOC_HAVE__SYSCALL3)
/* libc doesn't have support for sched_setaffinity, build system call
 * ourselves: */
#    include <linux/unistd.h>
#    ifndef __NR_sched_setaffinity
#       ifdef __i386__
#         define __NR_sched_setaffinity 241
#       elif defined(__x86_64__)
#         define __NR_sched_setaffinity 203
#       elif defined(__ia64__)
#         define __NR_sched_setaffinity 1231
#       elif defined(__hppa__)
#         define __NR_sched_setaffinity 211
#       elif defined(__alpha__)
#         define __NR_sched_setaffinity 395
#       elif defined(__s390__)
#         define __NR_sched_setaffinity 239
#       elif defined(__sparc__)
#         define __NR_sched_setaffinity 261
#       elif defined(__m68k__)
#         define __NR_sched_setaffinity 311
#       elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(__powerpc64__) || defined(__ppc64__)
#         define __NR_sched_setaffinity 222
#       elif defined(__arm__)
#         define __NR_sched_setaffinity 241
#       elif defined(__cris__)
#         define __NR_sched_setaffinity 241
/*#       elif defined(__mips__)
  #         define __NR_sched_setaffinity TODO (32/64/nabi) */
#       else
#         warning "don't know the syscall number for sched_setaffinity on this architecture, will not support binding"
#         define sched_setaffinity(pid, lg, mask) (errno = ENOSYS, -1)
#       endif
#    endif
#    ifndef sched_setaffinity
       _syscall3(int, sched_setaffinity, pid_t, pid, unsigned int, lg, const void *, mask)
#    endif
#    ifndef __NR_sched_getaffinity
#       ifdef __i386__
#         define __NR_sched_getaffinity 242
#       elif defined(__x86_64__)
#         define __NR_sched_getaffinity 204
#       elif defined(__ia64__)
#         define __NR_sched_getaffinity 1232
#       elif defined(__hppa__)
#         define __NR_sched_getaffinity 212
#       elif defined(__alpha__)
#         define __NR_sched_getaffinity 396
#       elif defined(__s390__)
#         define __NR_sched_getaffinity 240
#       elif defined(__sparc__)
#         define __NR_sched_getaffinity 260
#       elif defined(__m68k__)
#         define __NR_sched_getaffinity 312
#       elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(__powerpc64__) || defined(__ppc64__)
#         define __NR_sched_getaffinity 223
#       elif defined(__arm__)
#         define __NR_sched_getaffinity 242
#       elif defined(__cris__)
#         define __NR_sched_getaffinity 242
/*#       elif defined(__mips__)
  #         define __NR_sched_getaffinity TODO (32/64/nabi) */
#       else
#         warning "don't know the syscall number for sched_getaffinity on this architecture, will not support getting binding"
#         define sched_getaffinity(pid, lg, mask) (errno = ENOSYS, -1)
#       endif
#    endif
#    ifndef sched_getaffinity
       _syscall3(int, sched_getaffinity, pid_t, pid, unsigned int, lg, void *, mask)
#    endif
#endif

/* Added for ntohl() */
#include <arpa/inet.h>

#ifdef HAVE_OPENAT
/* Use our own filesystem functions if we have openat */

static const char *
hwloc_checkat(const char *path, int fsroot_fd)
{
  const char *relative_path;
  if (fsroot_fd < 0) {
    errno = EBADF;
    return NULL;
  }

  /* Skip leading slashes.  */
  for (relative_path = path; *relative_path == '/'; relative_path++);

  return relative_path;
}

static int
hwloc_openat(const char *path, int fsroot_fd)
{
  const char *relative_path;

  relative_path = hwloc_checkat(path, fsroot_fd);
  if (!relative_path)
    return -1;

  return openat (fsroot_fd, relative_path, O_RDONLY);
}

static FILE *
hwloc_fopenat(const char *path, const char *mode, int fsroot_fd)
{
  int fd;

  if (strcmp(mode, "r")) {
    errno = ENOTSUP;
    return NULL;
  }

  fd = hwloc_openat (path, fsroot_fd);
  if (fd == -1)
    return NULL;

  return fdopen(fd, mode);
}

static int
hwloc_accessat(const char *path, int mode, int fsroot_fd)
{
  const char *relative_path;

  relative_path = hwloc_checkat(path, fsroot_fd);
  if (!relative_path)
    return -1;

  return faccessat(fsroot_fd, relative_path, mode, 0);
}

static int
hwloc_fstatat(const char *path, struct stat *st, int flags, int fsroot_fd)
{
  const char *relative_path;

  relative_path = hwloc_checkat(path, fsroot_fd);
  if (!relative_path)
    return -1;

  return fstatat(fsroot_fd, relative_path, st, flags);
}

static DIR*
hwloc_opendirat(const char *path, int fsroot_fd)
{
  int dir_fd;
  const char *relative_path;

  relative_path = hwloc_checkat(path, fsroot_fd);
  if (!relative_path)
    return NULL;

  dir_fd = openat(fsroot_fd, relative_path, O_RDONLY | O_DIRECTORY);
  if (dir_fd < 0)
    return NULL;

  return fdopendir(dir_fd);
}

#endif /* HAVE_OPENAT */

/* Static inline version of fopen so that we can use openat if we have
   it, but still preserve compiler parameter checking */
static __hwloc_inline int
hwloc_open(const char *p, int d __hwloc_attribute_unused)
{ 
#ifdef HAVE_OPENAT
    return hwloc_openat(p, d);
#else
    return open(p, O_RDONLY);
#endif
}

static __hwloc_inline FILE *
hwloc_fopen(const char *p, const char *m, int d __hwloc_attribute_unused)
{ 
#ifdef HAVE_OPENAT
    return hwloc_fopenat(p, m, d);
#else
    return fopen(p, m);
#endif
}

/* Static inline version of access so that we can use openat if we have
   it, but still preserve compiler parameter checking */
static __hwloc_inline int 
hwloc_access(const char *p, int m, int d __hwloc_attribute_unused)
{ 
#ifdef HAVE_OPENAT
    return hwloc_accessat(p, m, d);
#else
    return access(p, m);
#endif
}

static __hwloc_inline int
hwloc_stat(const char *p, struct stat *st, int d __hwloc_attribute_unused)
{
#ifdef HAVE_OPENAT
    return hwloc_fstatat(p, st, 0, d);
#else
    return stat(p, st);
#endif
}

/* Static inline version of opendir so that we can use openat if we have
   it, but still preserve compiler parameter checking */
static __hwloc_inline DIR *
hwloc_opendir(const char *p, int d __hwloc_attribute_unused)
{ 
#ifdef HAVE_OPENAT
    return hwloc_opendirat(p, d);
#else
    return opendir(p);
#endif
}

int
hwloc_linux_set_tid_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, pid_t tid __hwloc_attribute_unused, hwloc_const_bitmap_t hwloc_set __hwloc_attribute_unused)
{
  /* TODO Kerrighed: Use
   * int migrate (pid_t pid, int destination_node);
   * int migrate_self (int destination_node);
   * int thread_migrate (int thread_id, int destination_node);
   */

  /* The resulting binding is always strict */

#if defined(HWLOC_HAVE_CPU_SET_S) && !defined(HWLOC_HAVE_OLD_SCHED_SETAFFINITY)
  cpu_set_t *plinux_set;
  unsigned cpu;
  int last;
  size_t setsize;
  int err;

  last = hwloc_bitmap_last(hwloc_set);
  if (last == -1) {
    errno = EINVAL;
    return -1;
  }

  setsize = CPU_ALLOC_SIZE(last+1);
  plinux_set = CPU_ALLOC(last+1);

  CPU_ZERO_S(setsize, plinux_set);
  hwloc_bitmap_foreach_begin(cpu, hwloc_set)
    CPU_SET_S(cpu, setsize, plinux_set);
  hwloc_bitmap_foreach_end();

  err = sched_setaffinity(tid, setsize, plinux_set);

  CPU_FREE(plinux_set);
  return err;
#elif defined(HWLOC_HAVE_CPU_SET)
  cpu_set_t linux_set;
  unsigned cpu;

  CPU_ZERO(&linux_set);
  hwloc_bitmap_foreach_begin(cpu, hwloc_set)
    CPU_SET(cpu, &linux_set);
  hwloc_bitmap_foreach_end();

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
  return sched_setaffinity(tid, &linux_set);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  return sched_setaffinity(tid, sizeof(linux_set), &linux_set);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
#elif defined(HWLOC_HAVE__SYSCALL3)
  unsigned long mask = hwloc_bitmap_to_ulong(hwloc_set);

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
  return sched_setaffinity(tid, (void*) &mask);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  return sched_setaffinity(tid, sizeof(mask), (void*) &mask);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
#else /* !_SYSCALL3 */
  errno = ENOSYS;
  return -1;
#endif /* !_SYSCALL3 */
}

#if defined(HWLOC_HAVE_CPU_SET_S) && !defined(HWLOC_HAVE_OLD_SCHED_SETAFFINITY)
/*
 * On some kernels, sched_getaffinity requires the output size to be larger
 * than the kernel cpu_set size (defined by CONFIG_NR_CPUS).
 * Try sched_affinity on ourself until we find a nr_cpus value that makes
 * the kernel happy.
 */
static int
hwloc_linux_find_kernel_nr_cpus(hwloc_topology_t topology)
{
  static int nr_cpus = -1;

  if (nr_cpus != -1)
    /* already computed */
    return nr_cpus;

  /* start with a nr_cpus that may contain the whole topology */
  nr_cpus = hwloc_bitmap_last(topology->levels[0][0]->complete_cpuset) + 1;
  while (1) {
    cpu_set_t *set = CPU_ALLOC(nr_cpus);
    size_t setsize = CPU_ALLOC_SIZE(nr_cpus);
    int err = sched_getaffinity(0, setsize, set); /* always works, unless setsize is too small */
    CPU_FREE(set);
    if (!err)
      /* found it */
      return nr_cpus;
    nr_cpus *= 2;
  }
}
#endif

int
hwloc_linux_get_tid_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, pid_t tid __hwloc_attribute_unused, hwloc_bitmap_t hwloc_set __hwloc_attribute_unused)
{
  int err __hwloc_attribute_unused;
  /* TODO Kerrighed */

#if defined(HWLOC_HAVE_CPU_SET_S) && !defined(HWLOC_HAVE_OLD_SCHED_SETAFFINITY)
  cpu_set_t *plinux_set;
  unsigned cpu;
  int last;
  size_t setsize;
  int kernel_nr_cpus;

  /* find the kernel nr_cpus so as to use a large enough cpu_set size */
  kernel_nr_cpus = hwloc_linux_find_kernel_nr_cpus(topology);
  setsize = CPU_ALLOC_SIZE(kernel_nr_cpus);
  plinux_set = CPU_ALLOC(kernel_nr_cpus);

  err = sched_getaffinity(tid, setsize, plinux_set);

  if (err < 0) {
    CPU_FREE(plinux_set);
    return -1;
  }

  last = hwloc_bitmap_last(topology->levels[0][0]->complete_cpuset);
  assert(last != -1);

  hwloc_bitmap_zero(hwloc_set);
  for(cpu=0; cpu<=(unsigned) last; cpu++)
    if (CPU_ISSET_S(cpu, setsize, plinux_set))
      hwloc_bitmap_set(hwloc_set, cpu);

  CPU_FREE(plinux_set);
#elif defined(HWLOC_HAVE_CPU_SET)
  cpu_set_t linux_set;
  unsigned cpu;

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
  err = sched_getaffinity(tid, &linux_set);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  err = sched_getaffinity(tid, sizeof(linux_set), &linux_set);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  if (err < 0)
    return -1;

  hwloc_bitmap_zero(hwloc_set);
  for(cpu=0; cpu<CPU_SETSIZE; cpu++)
    if (CPU_ISSET(cpu, &linux_set))
      hwloc_bitmap_set(hwloc_set, cpu);
#elif defined(HWLOC_HAVE__SYSCALL3)
  unsigned long mask;

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
  err = sched_getaffinity(tid, (void*) &mask);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  err = sched_getaffinity(tid, sizeof(mask), (void*) &mask);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  if (err < 0)
    return -1;

  hwloc_bitmap_from_ulong(hwloc_set, mask);
#else /* !_SYSCALL3 */
  errno = ENOSYS;
  return -1;
#endif /* !_SYSCALL3 */

  return 0;
}

/* Get the array of tids of a process from the task directory in /proc */
static int
hwloc_linux_get_proc_tids(DIR *taskdir, unsigned *nr_tidsp, pid_t ** tidsp)
{
  struct dirent *dirent;
  unsigned nr_tids = 0;
  unsigned max_tids = 32;
  pid_t *tids;
  struct stat sb;

  /* take the number of links as a good estimate for the number of tids */
  if (fstat(dirfd(taskdir), &sb) == 0)
    max_tids = sb.st_nlink;

  tids = malloc(max_tids*sizeof(pid_t));
  if (!tids) {
    errno = ENOMEM;
    return -1;
  }

  rewinddir(taskdir);

  while ((dirent = readdir(taskdir)) != NULL) {
    if (nr_tids == max_tids) {
      pid_t *newtids;
      max_tids += 8;
      newtids = realloc(tids, max_tids*sizeof(pid_t));
      if (!newtids) {
        free(tids);
        errno = ENOMEM;
        return -1;
      }
      tids = newtids;
    }
    if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
      continue;
    tids[nr_tids++] = atoi(dirent->d_name);
  }

  *nr_tidsp = nr_tids;
  *tidsp = tids;
  return 0;
}

/* Callbacks for binding each process sub-tid */
typedef int (*hwloc_linux_foreach_proc_tid_cb_t)(hwloc_topology_t topology, pid_t tid, void *data, int idx, int flags);

static int
hwloc_linux_foreach_proc_tid_set_cpubind_cb(hwloc_topology_t topology, pid_t tid, void *data, int idx __hwloc_attribute_unused, int flags __hwloc_attribute_unused)
{
  hwloc_bitmap_t cpuset = data;
  return hwloc_linux_set_tid_cpubind(topology, tid, cpuset);
}

static int
hwloc_linux_foreach_proc_tid_get_cpubind_cb(hwloc_topology_t topology, pid_t tid, void *data, int idx, int flags)
{
  hwloc_bitmap_t *cpusets = data;
  hwloc_bitmap_t cpuset = cpusets[0];
  hwloc_bitmap_t tidset = cpusets[1];

  if (hwloc_linux_get_tid_cpubind(topology, tid, tidset))
    return -1;

  /* reset the cpuset on first iteration */
  if (!idx)
    hwloc_bitmap_zero(cpuset);

  if (flags & HWLOC_CPUBIND_STRICT) {
    /* if STRICT, we want all threads to have the same binding */
    if (!idx) {
      /* this is the first thread, copy its binding */
      hwloc_bitmap_copy(cpuset, tidset);
    } else if (!hwloc_bitmap_isequal(cpuset, tidset)) {
      /* this is not the first thread, and it's binding is different */
      errno = EXDEV;
      return -1;
    }
  } else {
    /* if not STRICT, just OR all thread bindings */
    hwloc_bitmap_or(cpuset, cpuset, tidset);
  }
  return 0;
}

/* Call the callback for each process tid. */
static int
hwloc_linux_foreach_proc_tid(hwloc_topology_t topology,
			     pid_t pid, hwloc_linux_foreach_proc_tid_cb_t cb,
			     void *data, int flags)
{
  char taskdir_path[128];
  DIR *taskdir;
  pid_t *tids, *newtids;
  unsigned i, nr, newnr;
  int err;

  if (pid)
    snprintf(taskdir_path, sizeof(taskdir_path), "/proc/%u/task", (unsigned) pid);
  else
    snprintf(taskdir_path, sizeof(taskdir_path), "/proc/self/task");

  taskdir = opendir(taskdir_path);
  if (!taskdir) {
    errno = ENOSYS;
    err = -1;
    goto out;
  }

  /* read the current list of threads */
  err = hwloc_linux_get_proc_tids(taskdir, &nr, &tids);
  if (err < 0)
    goto out_with_dir;

 retry:
  /* apply the callback to all threads */
  for(i=0; i<nr; i++) {
    err = cb(topology, tids[i], data, i, flags);
    if (err < 0)
      goto out_with_tids;
  }

  /* re-read the list of thread and retry if it changed in the meantime */
  err = hwloc_linux_get_proc_tids(taskdir, &newnr, &newtids);
  if (err < 0)
    goto out_with_tids;
  if (newnr != nr || memcmp(newtids, tids, nr*sizeof(pid_t))) {
    free(tids);
    tids = newtids;
    nr = newnr;
    goto retry;
  }

  err = 0;
  free(newtids);
 out_with_tids:
  free(tids);
 out_with_dir:
  closedir(taskdir);
 out:
  return err;
}

static int
hwloc_linux_set_pid_cpubind(hwloc_topology_t topology, pid_t pid, hwloc_const_bitmap_t hwloc_set, int flags)
{
  return hwloc_linux_foreach_proc_tid(topology, pid,
				      hwloc_linux_foreach_proc_tid_set_cpubind_cb,
				      (void*) hwloc_set, flags);
}

static int
hwloc_linux_get_pid_cpubind(hwloc_topology_t topology, pid_t pid, hwloc_bitmap_t hwloc_set, int flags)
{
  hwloc_bitmap_t tidset = hwloc_bitmap_alloc();
  hwloc_bitmap_t cpusets[2];
  int ret;

  cpusets[0] = hwloc_set;
  cpusets[1] = tidset;
  ret = hwloc_linux_foreach_proc_tid(topology, pid,
					 hwloc_linux_foreach_proc_tid_get_cpubind_cb,
					 (void*) cpusets, flags);
  hwloc_bitmap_free(tidset);
  return ret;
}

static int
hwloc_linux_set_proc_cpubind(hwloc_topology_t topology, pid_t pid, hwloc_const_bitmap_t hwloc_set, int flags)
{
  if (pid == 0)
    pid = topology->pid;
  if (flags & HWLOC_CPUBIND_THREAD)
    return hwloc_linux_set_tid_cpubind(topology, pid, hwloc_set);
  else
    return hwloc_linux_set_pid_cpubind(topology, pid, hwloc_set, flags);
}

static int
hwloc_linux_get_proc_cpubind(hwloc_topology_t topology, pid_t pid, hwloc_bitmap_t hwloc_set, int flags)
{
  if (pid == 0)
    pid = topology->pid;
  if (flags & HWLOC_CPUBIND_THREAD)
    return hwloc_linux_get_tid_cpubind(topology, pid, hwloc_set);
  else
    return hwloc_linux_get_pid_cpubind(topology, pid, hwloc_set, flags);
}

static int
hwloc_linux_set_thisproc_cpubind(hwloc_topology_t topology, hwloc_const_bitmap_t hwloc_set, int flags)
{
  return hwloc_linux_set_pid_cpubind(topology, topology->pid, hwloc_set, flags);
}

static int
hwloc_linux_get_thisproc_cpubind(hwloc_topology_t topology, hwloc_bitmap_t hwloc_set, int flags)
{
  return hwloc_linux_get_pid_cpubind(topology, topology->pid, hwloc_set, flags);
}

static int
hwloc_linux_set_thisthread_cpubind(hwloc_topology_t topology, hwloc_const_bitmap_t hwloc_set, int flags __hwloc_attribute_unused)
{
  if (topology->pid) {
    errno = ENOSYS;
    return -1;
  }
  return hwloc_linux_set_tid_cpubind(topology, 0, hwloc_set);
}

static int
hwloc_linux_get_thisthread_cpubind(hwloc_topology_t topology, hwloc_bitmap_t hwloc_set, int flags __hwloc_attribute_unused)
{
  if (topology->pid) {
    errno = ENOSYS;
    return -1;
  }
  return hwloc_linux_get_tid_cpubind(topology, 0, hwloc_set);
}

#if HAVE_DECL_PTHREAD_SETAFFINITY_NP
#pragma weak pthread_setaffinity_np
#pragma weak pthread_self

static int
hwloc_linux_set_thread_cpubind(hwloc_topology_t topology, pthread_t tid, hwloc_const_bitmap_t hwloc_set, int flags __hwloc_attribute_unused)
{
  int err;

  if (topology->pid) {
    errno = ENOSYS;
    return -1;
  }

  if (!pthread_self) {
    /* ?! Application uses set_thread_cpubind, but doesn't link against libpthread ?! */
    errno = ENOSYS;
    return -1;
  }
  if (tid == pthread_self())
    return hwloc_linux_set_tid_cpubind(topology, 0, hwloc_set);

  if (!pthread_setaffinity_np) {
    errno = ENOSYS;
    return -1;
  }
  /* TODO Kerrighed: Use
   * int migrate (pid_t pid, int destination_node);
   * int migrate_self (int destination_node);
   * int thread_migrate (int thread_id, int destination_node);
   */

#if defined(HWLOC_HAVE_CPU_SET_S) && !defined(HWLOC_HAVE_OLD_SCHED_SETAFFINITY)
  /* Use a separate block so that we can define specific variable
     types here */
  {
     cpu_set_t *plinux_set;
     unsigned cpu;
     int last;
     size_t setsize;

     last = hwloc_bitmap_last(hwloc_set);
     if (last == -1) {
       errno = EINVAL;
       return -1;
     }

     setsize = CPU_ALLOC_SIZE(last+1);
     plinux_set = CPU_ALLOC(last+1);

     CPU_ZERO_S(setsize, plinux_set);
     hwloc_bitmap_foreach_begin(cpu, hwloc_set)
         CPU_SET_S(cpu, setsize, plinux_set);
     hwloc_bitmap_foreach_end();

     err = pthread_setaffinity_np(tid, setsize, plinux_set);

     CPU_FREE(plinux_set);
  }
#elif defined(HWLOC_HAVE_CPU_SET)
  /* Use a separate block so that we can define specific variable
     types here */
  {
     cpu_set_t linux_set;
     unsigned cpu;

     CPU_ZERO(&linux_set);
     hwloc_bitmap_foreach_begin(cpu, hwloc_set)
         CPU_SET(cpu, &linux_set);
     hwloc_bitmap_foreach_end();

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
     err = pthread_setaffinity_np(tid, &linux_set);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
     err = pthread_setaffinity_np(tid, sizeof(linux_set), &linux_set);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  }
#else /* CPU_SET */
  /* Use a separate block so that we can define specific variable
     types here */
  {
      unsigned long mask = hwloc_bitmap_to_ulong(hwloc_set);

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
      err = pthread_setaffinity_np(tid, (void*) &mask);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
      err = pthread_setaffinity_np(tid, sizeof(mask), (void*) &mask);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
  }
#endif /* CPU_SET */

  if (err) {
    errno = err;
    return -1;
  }
  return 0;
}
#endif /* HAVE_DECL_PTHREAD_SETAFFINITY_NP */

#if HAVE_DECL_PTHREAD_GETAFFINITY_NP
#pragma weak pthread_getaffinity_np
#pragma weak pthread_self

static int
hwloc_linux_get_thread_cpubind(hwloc_topology_t topology, pthread_t tid, hwloc_bitmap_t hwloc_set, int flags __hwloc_attribute_unused)
{
  int err;

  if (topology->pid) {
    errno = ENOSYS;
    return -1;
  }

  if (!pthread_self) {
    /* ?! Application uses set_thread_cpubind, but doesn't link against libpthread ?! */
    errno = ENOSYS;
    return -1;
  }
  if (tid == pthread_self())
    return hwloc_linux_get_tid_cpubind(topology, 0, hwloc_set);

  if (!pthread_getaffinity_np) {
    errno = ENOSYS;
    return -1;
  }
  /* TODO Kerrighed */

#if defined(HWLOC_HAVE_CPU_SET_S) && !defined(HWLOC_HAVE_OLD_SCHED_SETAFFINITY)
  /* Use a separate block so that we can define specific variable
     types here */
  {
     cpu_set_t *plinux_set;
     unsigned cpu;
     int last;
     size_t setsize;

     last = hwloc_bitmap_last(topology->levels[0][0]->complete_cpuset);
     assert (last != -1);

     setsize = CPU_ALLOC_SIZE(last+1);
     plinux_set = CPU_ALLOC(last+1);

     err = pthread_getaffinity_np(tid, setsize, plinux_set);
     if (err) {
        CPU_FREE(plinux_set);
        errno = err;
        return -1;
     }

     hwloc_bitmap_zero(hwloc_set);
     for(cpu=0; cpu<(unsigned) last; cpu++)
       if (CPU_ISSET_S(cpu, setsize, plinux_set))
	 hwloc_bitmap_set(hwloc_set, cpu);

     CPU_FREE(plinux_set);
  }
#elif defined(HWLOC_HAVE_CPU_SET)
  /* Use a separate block so that we can define specific variable
     types here */
  {
     cpu_set_t linux_set;
     unsigned cpu;

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
     err = pthread_getaffinity_np(tid, &linux_set);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
     err = pthread_getaffinity_np(tid, sizeof(linux_set), &linux_set);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
     if (err) {
        errno = err;
        return -1;
     }

     hwloc_bitmap_zero(hwloc_set);
     for(cpu=0; cpu<CPU_SETSIZE; cpu++)
       if (CPU_ISSET(cpu, &linux_set))
	 hwloc_bitmap_set(hwloc_set, cpu);
  }
#else /* CPU_SET */
  /* Use a separate block so that we can define specific variable
     types here */
  {
      unsigned long mask;

#ifdef HWLOC_HAVE_OLD_SCHED_SETAFFINITY
      err = pthread_getaffinity_np(tid, (void*) &mask);
#else /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
      err = pthread_getaffinity_np(tid, sizeof(mask), (void*) &mask);
#endif /* HWLOC_HAVE_OLD_SCHED_SETAFFINITY */
      if (err) {
        errno = err;
        return -1;
      }

     hwloc_bitmap_from_ulong(hwloc_set, mask);
  }
#endif /* CPU_SET */

  return 0;
}
#endif /* HAVE_DECL_PTHREAD_GETAFFINITY_NP */

static int
hwloc_linux_get_tid_last_cpu_location(hwloc_topology_t topology __hwloc_attribute_unused, pid_t tid, hwloc_bitmap_t set)
{
  /* read /proc/pid/stat.
   * its second field contains the command name between parentheses,
   * and the command itself may contain parentheses,
   * so read the whole line and find the last closing parenthesis to find the third field.
   */
  char buf[1024] = "";
  char name[64];
  char *tmp;
  FILE *file;
  int i;

  if (!tid) {
#ifdef SYS_gettid
    tid = syscall(SYS_gettid);
#else
    errno = ENOSYS;
    return -1;
#endif
  }

  snprintf(name, sizeof(name), "/proc/%lu/stat", (unsigned long) tid);
  file = fopen(name, "r");
  if (!file) {
    errno = ENOSYS;
    return -1;
  }
  tmp = fgets(buf, sizeof(buf), file);
  fclose(file);
  if (!tmp) {
    errno = ENOSYS;
    return -1;
  }

  tmp = strrchr(buf, ')');
  if (!tmp) {
    errno = ENOSYS;
    return -1;
  }
  /* skip ') ' to find the actual third argument */
  tmp += 2;

  /* skip 35 fields */
  for(i=0; i<36; i++) {
    tmp = strchr(tmp, ' ');
    if (!tmp) {
      errno = ENOSYS;
      return -1;
    }
    /* skip the ' ' itself */
    tmp++;
  }

  /* read the last cpu in the 38th field now */
  if (sscanf(tmp, "%d ", &i) != 1) {
    errno = ENOSYS;
    return -1;
  }

  hwloc_bitmap_only(set, i);
  return 0;
}

static int
hwloc_linux_foreach_proc_tid_get_last_cpu_location_cb(hwloc_topology_t topology, pid_t tid, void *data, int idx, int flags __hwloc_attribute_unused)
{
  hwloc_bitmap_t *cpusets = data;
  hwloc_bitmap_t cpuset = cpusets[0];
  hwloc_bitmap_t tidset = cpusets[1];

  if (hwloc_linux_get_tid_last_cpu_location(topology, tid, tidset))
    return -1;

  /* reset the cpuset on first iteration */
  if (!idx)
    hwloc_bitmap_zero(cpuset);

  hwloc_bitmap_or(cpuset, cpuset, tidset);
  return 0;
}

static int
hwloc_linux_get_pid_last_cpu_location(hwloc_topology_t topology, pid_t pid, hwloc_bitmap_t hwloc_set, int flags)
{
  hwloc_bitmap_t tidset = hwloc_bitmap_alloc();
  hwloc_bitmap_t cpusets[2];
  int ret;

  cpusets[0] = hwloc_set;
  cpusets[1] = tidset;
  ret = hwloc_linux_foreach_proc_tid(topology, pid,
				     hwloc_linux_foreach_proc_tid_get_last_cpu_location_cb,
				     (void*) cpusets, flags);
  hwloc_bitmap_free(tidset);
  return ret;
}

static int
hwloc_linux_get_proc_last_cpu_location(hwloc_topology_t topology, pid_t pid, hwloc_bitmap_t hwloc_set, int flags)
{
  if (pid == 0)
    pid = topology->pid;
  if (flags & HWLOC_CPUBIND_THREAD)
    return hwloc_linux_get_tid_last_cpu_location(topology, pid, hwloc_set);
  else
    return hwloc_linux_get_pid_last_cpu_location(topology, pid, hwloc_set, flags);
}

static int
hwloc_linux_get_thisproc_last_cpu_location(hwloc_topology_t topology, hwloc_bitmap_t hwloc_set, int flags)
{
  return hwloc_linux_get_pid_last_cpu_location(topology, topology->pid, hwloc_set, flags);
}

static int
hwloc_linux_get_thisthread_last_cpu_location(hwloc_topology_t topology, hwloc_bitmap_t hwloc_set, int flags __hwloc_attribute_unused)
{
  if (topology->pid) {
    errno = ENOSYS;
    return -1;
  }
  return hwloc_linux_get_tid_last_cpu_location(topology, 0, hwloc_set);
}


#if defined HWLOC_HAVE_SET_MEMPOLICY || defined HWLOC_HAVE_MBIND
static int
hwloc_linux_membind_policy_from_hwloc(int *linuxpolicy, hwloc_membind_policy_t policy, int flags)
{
  switch (policy) {
  case HWLOC_MEMBIND_DEFAULT:
  case HWLOC_MEMBIND_FIRSTTOUCH:
    *linuxpolicy = MPOL_DEFAULT;
    break;
  case HWLOC_MEMBIND_BIND:
    if (flags & HWLOC_MEMBIND_STRICT)
      *linuxpolicy = MPOL_BIND;
    else
      *linuxpolicy = MPOL_PREFERRED;
    break;
  case HWLOC_MEMBIND_INTERLEAVE:
    *linuxpolicy = MPOL_INTERLEAVE;
    break;
  /* TODO: next-touch when (if?) patch applied upstream */
  default:
    errno = ENOSYS;
    return -1;
  }
  return 0;
}

static int
hwloc_linux_membind_mask_from_nodeset(hwloc_topology_t topology __hwloc_attribute_unused,
				      hwloc_const_nodeset_t nodeset,
				      unsigned *max_os_index_p, unsigned long **linuxmaskp)
{
  unsigned max_os_index = 0; /* highest os_index + 1 */
  unsigned long *linuxmask;
  unsigned i;
  hwloc_nodeset_t linux_nodeset = NULL;

  if (hwloc_bitmap_isfull(nodeset)) {
    linux_nodeset = hwloc_bitmap_alloc();
    hwloc_bitmap_only(linux_nodeset, 0);
    nodeset = linux_nodeset;
  }

  max_os_index = hwloc_bitmap_last(nodeset);
  if (max_os_index == (unsigned) -1)
    max_os_index = 0;
  /* add 1 to convert the last os_index into a max_os_index,
   * and round up to the nearest multiple of BITS_PER_LONG */
  max_os_index = (max_os_index + 1 + HWLOC_BITS_PER_LONG - 1) & ~(HWLOC_BITS_PER_LONG - 1);

  linuxmask = calloc(max_os_index/HWLOC_BITS_PER_LONG, sizeof(long));
  if (!linuxmask) {
    errno = ENOMEM;
    return -1;
  }

  for(i=0; i<max_os_index/HWLOC_BITS_PER_LONG; i++)
    linuxmask[i] = hwloc_bitmap_to_ith_ulong(nodeset, i);

  if (linux_nodeset)
    hwloc_bitmap_free(linux_nodeset);

  *max_os_index_p = max_os_index;
  *linuxmaskp = linuxmask;
  return 0;
}

static void
hwloc_linux_membind_mask_to_nodeset(hwloc_topology_t topology __hwloc_attribute_unused,
				    hwloc_nodeset_t nodeset,
				    unsigned max_os_index, const unsigned long *linuxmask)
{
  unsigned i;

#ifdef HWLOC_DEBUG
  /* max_os_index comes from hwloc_linux_find_kernel_max_numnodes() so it's a multiple of HWLOC_BITS_PER_LONG */
  assert(!(max_os_index%HWLOC_BITS_PER_LONG));
#endif

  hwloc_bitmap_zero(nodeset);
  for(i=0; i<max_os_index/HWLOC_BITS_PER_LONG; i++)
    hwloc_bitmap_set_ith_ulong(nodeset, i, linuxmask[i]);
}
#endif /* HWLOC_HAVE_SET_MEMPOLICY || HWLOC_HAVE_MBIND */

#ifdef HWLOC_HAVE_MBIND
static int
hwloc_linux_set_area_membind(hwloc_topology_t topology, const void *addr, size_t len, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags)
{
  unsigned max_os_index; /* highest os_index + 1 */
  unsigned long *linuxmask;
  size_t remainder;
  int linuxpolicy;
  unsigned linuxflags = 0;
  int err;

  remainder = (uintptr_t) addr & (sysconf(_SC_PAGESIZE)-1);
  addr = (char*) addr - remainder;
  len += remainder;

  err = hwloc_linux_membind_policy_from_hwloc(&linuxpolicy, policy, flags);
  if (err < 0)
    return err;

  if (linuxpolicy == MPOL_DEFAULT)
    /* Some Linux kernels don't like being passed a set */
    return mbind((void *) addr, len, linuxpolicy, NULL, 0, 0);

  err = hwloc_linux_membind_mask_from_nodeset(topology, nodeset, &max_os_index, &linuxmask);
  if (err < 0)
    goto out;

  if (flags & HWLOC_MEMBIND_MIGRATE) {
#ifdef MPOL_MF_MOVE
    linuxflags = MPOL_MF_MOVE;
    if (flags & HWLOC_MEMBIND_STRICT)
      linuxflags |= MPOL_MF_STRICT;
#else
    if (flags & HWLOC_MEMBIND_STRICT) {
      errno = ENOSYS;
      goto out_with_mask;
    }
#endif
  }

  err = mbind((void *) addr, len, linuxpolicy, linuxmask, max_os_index+1, linuxflags);
  if (err < 0)
    goto out_with_mask;

  free(linuxmask);
  return 0;

 out_with_mask:
  free(linuxmask);
 out:
  return -1;
}

static void *
hwloc_linux_alloc_membind(hwloc_topology_t topology, size_t len, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags)
{
  void *buffer;
  int err;

  buffer = hwloc_alloc_mmap(topology, len);
  if (buffer == MAP_FAILED)
    return NULL;

  err = hwloc_linux_set_area_membind(topology, buffer, len, nodeset, policy, flags);
  if (err < 0 && policy & HWLOC_MEMBIND_STRICT) {
    munmap(buffer, len);
    return NULL;
  }

  return buffer;
}
#endif /* HWLOC_HAVE_MBIND */

#ifdef HWLOC_HAVE_SET_MEMPOLICY
static int
hwloc_linux_set_thisthread_membind(hwloc_topology_t topology, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags)
{
  unsigned max_os_index; /* highest os_index + 1 */
  unsigned long *linuxmask;
  int linuxpolicy;
  int err;

  err = hwloc_linux_membind_policy_from_hwloc(&linuxpolicy, policy, flags);
  if (err < 0)
    return err;

  if (linuxpolicy == MPOL_DEFAULT)
    /* Some Linux kernels don't like being passed a set */
    return set_mempolicy(linuxpolicy, NULL, 0);

  err = hwloc_linux_membind_mask_from_nodeset(topology, nodeset, &max_os_index, &linuxmask);
  if (err < 0)
    goto out;

  if (flags & HWLOC_MEMBIND_MIGRATE) {
#ifdef HWLOC_HAVE_MIGRATE_PAGES
    unsigned long *fullmask = malloc(max_os_index/HWLOC_BITS_PER_LONG * sizeof(long));
    if (fullmask) {
      memset(fullmask, 0xf, max_os_index/HWLOC_BITS_PER_LONG * sizeof(long));
      err = migrate_pages(0, max_os_index+1, fullmask, linuxmask);
      free(fullmask);
    } else
      err = -1;
    if (err < 0 && (flags & HWLOC_MEMBIND_STRICT))
      goto out_with_mask;
#else
    errno = ENOSYS;
    goto out_with_mask;
#endif
  }

  err = set_mempolicy(linuxpolicy, linuxmask, max_os_index+1);
  if (err < 0)
    goto out_with_mask;

  free(linuxmask);
  return 0;

 out_with_mask:
  free(linuxmask);
 out:
  return -1;
}

/*
 * On some kernels, get_mempolicy requires the output size to be larger
 * than the kernel MAX_NUMNODES (defined by CONFIG_NODES_SHIFT).
 * Try get_mempolicy on ourself until we find a max_os_index value that
 * makes the kernel happy.
 */
static int
hwloc_linux_find_kernel_max_numnodes(hwloc_topology_t topology __hwloc_attribute_unused)
{
  static int max_numnodes = -1;
  int linuxpolicy;

  if (max_numnodes != -1)
    /* already computed */
    return max_numnodes;

  /* start with a single ulong, it's the minimal and it's enough for most machines */
  max_numnodes = HWLOC_BITS_PER_LONG;
  while (1) {
    unsigned long *mask = malloc(max_numnodes / HWLOC_BITS_PER_LONG * sizeof(long));
    int err = get_mempolicy(&linuxpolicy, mask, max_numnodes, 0, 0);
    free(mask);
    if (!err || errno != EINVAL)
      /* found it */
      return max_numnodes;
    max_numnodes *= 2;
  }
}

static int
hwloc_linux_get_thisthread_membind(hwloc_topology_t topology, hwloc_nodeset_t nodeset, hwloc_membind_policy_t *policy, int flags __hwloc_attribute_unused)
{
  unsigned max_os_index;
  unsigned long *linuxmask;
  int linuxpolicy;
  int err;

  max_os_index = hwloc_linux_find_kernel_max_numnodes(topology);

  linuxmask = malloc(max_os_index/HWLOC_BITS_PER_LONG * sizeof(long));
  if (!linuxmask) {
    errno = ENOMEM;
    goto out;
  }

  err = get_mempolicy(&linuxpolicy, linuxmask, max_os_index, 0, 0);
  if (err < 0)
    goto out_with_mask;

  if (linuxpolicy == MPOL_DEFAULT) {
    hwloc_bitmap_copy(nodeset, hwloc_topology_get_topology_nodeset(topology));
  } else {
    hwloc_linux_membind_mask_to_nodeset(topology, nodeset, max_os_index, linuxmask);
  }

  switch (linuxpolicy) {
  case MPOL_DEFAULT:
    *policy = HWLOC_MEMBIND_FIRSTTOUCH;
    break;
  case MPOL_PREFERRED:
  case MPOL_BIND:
    *policy = HWLOC_MEMBIND_BIND;
    break;
  case MPOL_INTERLEAVE:
    *policy = HWLOC_MEMBIND_INTERLEAVE;
    break;
  default:
    errno = EINVAL;
    goto out_with_mask;
  }

  free(linuxmask);
  return 0;

 out_with_mask:
  free(linuxmask);
 out:
  return -1;
}

#endif /* HWLOC_HAVE_SET_MEMPOLICY */

int
hwloc_backend_sysfs_init(struct hwloc_topology *topology, const char *fsroot_path __hwloc_attribute_unused)
{
#ifdef HAVE_OPENAT
  int root;

  assert(topology->backend_type == HWLOC_BACKEND_NONE);

  if (!fsroot_path)
    fsroot_path = "/";

  root = open(fsroot_path, O_RDONLY | O_DIRECTORY);
  if (root < 0)
    return -1;

  if (strcmp(fsroot_path, "/"))
    topology->is_thissystem = 0;

  topology->backend_params.sysfs.root_path = strdup(fsroot_path);
  topology->backend_params.sysfs.root_fd = root;
#else
  topology->backend_params.sysfs.root_path = NULL;
  topology->backend_params.sysfs.root_fd = -1;
#endif
  topology->backend_type = HWLOC_BACKEND_SYSFS;
  return 0;
}

void
hwloc_backend_sysfs_exit(struct hwloc_topology *topology)
{
  assert(topology->backend_type == HWLOC_BACKEND_SYSFS);
#ifdef HAVE_OPENAT
  close(topology->backend_params.sysfs.root_fd);
  free(topology->backend_params.sysfs.root_path);
  topology->backend_params.sysfs.root_path = NULL;
#endif
  topology->backend_type = HWLOC_BACKEND_NONE;
}

static int
hwloc_parse_sysfs_unsigned(const char *mappath, unsigned *value, int fsroot_fd)
{
  char string[11];
  FILE * fd;

  fd = hwloc_fopen(mappath, "r", fsroot_fd);
  if (!fd) {
    *value = -1;
    return -1;
  }

  if (!fgets(string, 11, fd)) {
    *value = -1;
    fclose(fd);
    return -1;
  }
  *value = strtoul(string, NULL, 10);

  fclose(fd);

  return 0;
}


/* kernel cpumaps are composed of an array of 32bits cpumasks */
#define KERNEL_CPU_MASK_BITS 32
#define KERNEL_CPU_MAP_LEN (KERNEL_CPU_MASK_BITS/4+2)

int
hwloc_linux_parse_cpumap_file(FILE *file, hwloc_bitmap_t set)
{
  unsigned long *maps;
  unsigned long map;
  int nr_maps = 0;
  static int nr_maps_allocated = 8; /* only compute the power-of-two above the kernel cpumask size once */
  int i;

  maps = malloc(nr_maps_allocated * sizeof(*maps));

  /* reset to zero first */
  hwloc_bitmap_zero(set);

  /* parse the whole mask */
  while (fscanf(file, "%lx,", &map) == 1) /* read one kernel cpu mask and the ending comma */
    {
      if (nr_maps == nr_maps_allocated) {
	nr_maps_allocated *= 2;
	maps = realloc(maps, nr_maps_allocated * sizeof(*maps));
      }

      if (!map && !nr_maps)
	/* ignore the first map if it's empty */
	continue;

      memmove(&maps[1], &maps[0], nr_maps*sizeof(*maps));
      maps[0] = map;
      nr_maps++;
    }

  /* convert into a set */
#if KERNEL_CPU_MASK_BITS == HWLOC_BITS_PER_LONG
  for(i=0; i<nr_maps; i++)
    hwloc_bitmap_set_ith_ulong(set, i, maps[i]);
#else
  for(i=0; i<(nr_maps+1)/2; i++) {
    unsigned long mask;
    mask = maps[2*i];
    if (2*i+1<nr_maps)
      mask |= maps[2*i+1] << KERNEL_CPU_MASK_BITS;
    hwloc_bitmap_set_ith_ulong(set, i, mask);
  }
#endif

  free(maps);

  return 0;
}

static hwloc_bitmap_t
hwloc_parse_cpumap(const char *mappath, int fsroot_fd)
{
  hwloc_bitmap_t set;
  FILE * file;

  file = hwloc_fopen(mappath, "r", fsroot_fd);
  if (!file)
    return NULL;

  set = hwloc_bitmap_alloc();
  hwloc_linux_parse_cpumap_file(file, set);

  fclose(file);
  return set;
}

static char *
hwloc_strdup_mntpath(const char *escapedpath, size_t length)
{
  char *path = malloc(length+1);
  const char *src = escapedpath, *tmp = src;
  char *dst = path;

  while ((tmp = strchr(src, '\\')) != NULL) {
    strncpy(dst, src, tmp-src);
    dst += tmp-src;
    if (!strncmp(tmp+1, "040", 3))
      *dst = ' ';
    else if (!strncmp(tmp+1, "011", 3))
      *dst = '	';
    else if (!strncmp(tmp+1, "012", 3))
      *dst = '\n';
    else
      *dst = '\\';
    dst++;
    src = tmp+4;
  }

  strcpy(dst, src);

  return path;
}

static void
hwloc_find_linux_cpuset_mntpnt(char **cgroup_mntpnt, char **cpuset_mntpnt, int fsroot_fd)
{
#define PROC_MOUNT_LINE_LEN 512
  char line[PROC_MOUNT_LINE_LEN];
  FILE *fd;

  *cgroup_mntpnt = NULL;
  *cpuset_mntpnt = NULL;

  /* ideally we should use setmntent, getmntent, hasmntopt and endmntent,
   * but they do not support fsroot_fd.
   */

  fd = hwloc_fopen("/proc/mounts", "r", fsroot_fd);
  if (!fd)
    return;

  while (fgets(line, sizeof(line), fd)) {
    char *path;
    char *type;
    char *tmp;

    /* remove the ending " 0 0\n" that the kernel always adds */
    tmp = line + strlen(line) - 5;
    if (tmp < line || strcmp(tmp, " 0 0\n"))
      fprintf(stderr, "Unexpected end of /proc/mounts line `%s'\n", line);
    else
      *tmp = '\0';

    /* path is after first field and a space */
    tmp = strchr(line, ' ');
    if (!tmp)
      continue;
    path = tmp+1;

    /* type is after path, which may not contain spaces since the kernel escaped them to \040
     * (see the manpage of getmntent) */
    tmp = strchr(path, ' ');
    if (!tmp)
      continue;
    type = tmp+1;
    /* mark the end of path to ease upcoming strdup */
    *tmp = '\0';

    if (!strncmp(type, "cpuset ", 7)) {
      /* found a cpuset mntpnt */
      hwloc_debug("Found cpuset mount point on %s\n", path);
      *cpuset_mntpnt = hwloc_strdup_mntpath(path, type-path);
      break;

    } else if (!strncmp(type, "cgroup ", 7)) {
      /* found a cgroup mntpnt */
      char *opt, *opts;
      int cpuset_opt = 0;
      int noprefix_opt = 0;

      /* find options */
      tmp = strchr(type, ' ');
      if (!tmp)
	continue;
      opts = tmp+1;

      /* look at options */
      while ((opt = strsep(&opts, ",")) != NULL) {
	if (!strcmp(opt, "cpuset"))
	  cpuset_opt = 1;
	else if (!strcmp(opt, "noprefix"))
	  noprefix_opt = 1;
      }
      if (!cpuset_opt)
	continue;

      if (noprefix_opt) {
	hwloc_debug("Found cgroup emulating a cpuset mount point on %s\n", path);
	*cpuset_mntpnt = hwloc_strdup_mntpath(path, type-path);
      } else {
	hwloc_debug("Found cgroup/cpuset mount point on %s\n", path);
	*cgroup_mntpnt = hwloc_strdup_mntpath(path, type-path);
      }
      break;
    }
  }

  fclose(fd);
}

/*
 * Linux cpusets may be managed directly or through cgroup.
 * If cgroup is used, tasks get a /proc/pid/cgroup which may contain a
 * single line %d:cpuset:<name>. If cpuset are used they get /proc/pid/cpuset
 * containing <name>.
 */
static char *
hwloc_read_linux_cpuset_name(int fsroot_fd, hwloc_pid_t pid)
{
#define CPUSET_NAME_LEN 128
  char cpuset_name[CPUSET_NAME_LEN];
  FILE *fd;
  char *tmp;

  /* check whether a cgroup-cpuset is enabled */
  if (!pid)
    fd = hwloc_fopen("/proc/self/cgroup", "r", fsroot_fd);
  else {
    char path[] = "/proc/XXXXXXXXXX/cgroup";
    snprintf(path, sizeof(path), "/proc/%d/cgroup", pid);
    fd = hwloc_fopen(path, "r", fsroot_fd);
  }
  if (fd) {
    /* find a cpuset line */
#define CGROUP_LINE_LEN 256
    char line[CGROUP_LINE_LEN];
    while (fgets(line, sizeof(line), fd)) {
      char *end, *colon = strchr(line, ':');
      if (!colon)
	continue;
      if (strncmp(colon, ":cpuset:", 8))
	continue;

      /* found a cgroup-cpuset line, return the name */
      fclose(fd);
      end = strchr(colon, '\n');
      if (end)
	*end = '\0';
      hwloc_debug("Found cgroup-cpuset %s\n", colon+8);
      return strdup(colon+8);
    }
    fclose(fd);
  }

  /* check whether a cpuset is enabled */
  if (!pid)
    fd = hwloc_fopen("/proc/self/cpuset", "r", fsroot_fd);
  else {
    char path[] = "/proc/XXXXXXXXXX/cpuset";
    snprintf(path, sizeof(path), "/proc/%d/cpuset", pid);
    fd = hwloc_fopen(path, "r", fsroot_fd);
  }
  if (!fd) {
    /* found nothing */
    hwloc_debug("%s", "No cgroup or cpuset found\n");
    return NULL;
  }

  /* found a cpuset, return the name */
  tmp = fgets(cpuset_name, sizeof(cpuset_name), fd);
  fclose(fd);
  if (!tmp)
    return NULL;
  tmp = strchr(cpuset_name, '\n');
  if (tmp)
    *tmp = '\0';
  hwloc_debug("Found cpuset %s\n", cpuset_name);
  return strdup(cpuset_name);
}

/*
 * Then, the cpuset description is available from either the cgroup or
 * the cpuset filesystem (usually mounted in / or /dev) where there
 * are cgroup<name>/cpuset.{cpus,mems} or cpuset<name>/{cpus,mems} files.
 */
static char *
hwloc_read_linux_cpuset_mask(const char *cgroup_mntpnt, const char *cpuset_mntpnt, const char *cpuset_name, const char *attr_name, int fsroot_fd)
{
#define CPUSET_FILENAME_LEN 256
  char cpuset_filename[CPUSET_FILENAME_LEN];
  FILE *fd;
  char *info = NULL, *tmp;
  ssize_t ssize;
  size_t size;

  if (cgroup_mntpnt) {
    /* try to read the cpuset from cgroup */
    snprintf(cpuset_filename, CPUSET_FILENAME_LEN, "%s%s/cpuset.%s", cgroup_mntpnt, cpuset_name, attr_name);
    hwloc_debug("Trying to read cgroup file <%s>\n", cpuset_filename);
    fd = hwloc_fopen(cpuset_filename, "r", fsroot_fd);
    if (fd)
      goto gotfile;
  } else if (cpuset_mntpnt) {
    /* try to read the cpuset directly */
    snprintf(cpuset_filename, CPUSET_FILENAME_LEN, "%s%s/%s", cpuset_mntpnt, cpuset_name, attr_name);
    hwloc_debug("Trying to read cpuset file <%s>\n", cpuset_filename);
    fd = hwloc_fopen(cpuset_filename, "r", fsroot_fd);
    if (fd)
      goto gotfile;
  }

  /* found no cpuset description, ignore it */
  hwloc_debug("Couldn't find cpuset <%s> description, ignoring\n", cpuset_name);
  goto out;

gotfile:
  ssize = getline(&info, &size, fd);
  fclose(fd);
  if (ssize < 0)
    goto out;
  if (!info)
    goto out;

  tmp = strchr(info, '\n');
  if (tmp)
    *tmp = '\0';

out:
  return info;
}

static void
hwloc_admin_disable_set_from_cpuset(struct hwloc_topology *topology,
				    const char *cgroup_mntpnt, const char *cpuset_mntpnt, const char *cpuset_name,
				    const char *attr_name,
				    hwloc_bitmap_t admin_enabled_cpus_set)
{
  char *cpuset_mask;
  char *current, *comma, *tmp;
  int prevlast, nextfirst, nextlast; /* beginning/end of enabled-segments */
  hwloc_bitmap_t tmpset;

  cpuset_mask = hwloc_read_linux_cpuset_mask(cgroup_mntpnt, cpuset_mntpnt, cpuset_name,
					     attr_name, topology->backend_params.sysfs.root_fd);
  if (!cpuset_mask)
    return;

  hwloc_debug("found cpuset %s: %s\n", attr_name, cpuset_mask);

  current = cpuset_mask;
  prevlast = -1;

  while (1) {
    /* save a pointer to the next comma and erase it to simplify things */
    comma = strchr(current, ',');
    if (comma)
      *comma = '\0';

    /* find current enabled-segment bounds */
    nextfirst = strtoul(current, &tmp, 0);
    if (*tmp == '-')
      nextlast = strtoul(tmp+1, NULL, 0);
    else
      nextlast = nextfirst;
    if (prevlast+1 <= nextfirst-1) {
      hwloc_debug("%s [%d:%d] excluded by cpuset\n", attr_name, prevlast+1, nextfirst-1);
      hwloc_bitmap_clr_range(admin_enabled_cpus_set, prevlast+1, nextfirst-1);
    }

    /* switch to next enabled-segment */
    prevlast = nextlast;
    if (!comma)
      break;
    current = comma+1;
  }

  hwloc_debug("%s [%d:%d] excluded by cpuset\n", attr_name, prevlast+1, nextfirst-1);
  /* no easy way to clear until the infinity */
  tmpset = hwloc_bitmap_alloc();
  hwloc_bitmap_set_range(tmpset, 0, prevlast);
  hwloc_bitmap_and(admin_enabled_cpus_set, admin_enabled_cpus_set, tmpset);
  hwloc_bitmap_free(tmpset);

  free(cpuset_mask);
}

static void
hwloc_parse_meminfo_info(struct hwloc_topology *topology,
			 const char *path,
			 int prefixlength,
			 uint64_t *local_memory,
			 uint64_t *meminfo_hugepages_count,
			 uint64_t *meminfo_hugepages_size,
			 int onlytotal)
{
  char string[64];
  FILE *fd;

  fd = hwloc_fopen(path, "r", topology->backend_params.sysfs.root_fd);
  if (!fd)
    return;

  while (fgets(string, sizeof(string), fd) && *string != '\0')
    {
      unsigned long long number;
      if (strlen(string) < (size_t) prefixlength)
        continue;
      if (sscanf(string+prefixlength, "MemTotal: %llu kB", (unsigned long long *) &number) == 1) {
	*local_memory = number << 10;
	if (onlytotal)
	  break;
      }
      else if (!onlytotal) {
	if (sscanf(string+prefixlength, "Hugepagesize: %llu", (unsigned long long *) &number) == 1)
	  *meminfo_hugepages_size = number << 10;
	else if (sscanf(string+prefixlength, "HugePages_Free: %llu", (unsigned long long *) &number) == 1)
          /* these are free hugepages, not the total amount of huge pages */
	  *meminfo_hugepages_count = number;
      }
    }

  fclose(fd);
}

#define SYSFS_NUMA_NODE_PATH_LEN 128

static void
hwloc_parse_hugepages_info(struct hwloc_topology *topology,
			   const char *dirpath,
			   struct hwloc_obj_memory_s *memory,
			   uint64_t *remaining_local_memory)
{
  DIR *dir;
  struct dirent *dirent;
  unsigned long index_ = 1;
  FILE *hpfd;
  char line[64];
  char path[SYSFS_NUMA_NODE_PATH_LEN];

  dir = hwloc_opendir(dirpath, topology->backend_params.sysfs.root_fd);
  if (dir) {
    while ((dirent = readdir(dir)) != NULL) {
      if (strncmp(dirent->d_name, "hugepages-", 10))
        continue;
      memory->page_types[index_].size = strtoul(dirent->d_name+10, NULL, 0) * 1024ULL;
      sprintf(path, "%s/%s/nr_hugepages", dirpath, dirent->d_name);
      hpfd = hwloc_fopen(path, "r", topology->backend_params.sysfs.root_fd);
      if (hpfd) {
        if (fgets(line, sizeof(line), hpfd)) {
          fclose(hpfd);
          /* these are the actual total amount of huge pages */
          memory->page_types[index_].count = strtoull(line, NULL, 0);
          *remaining_local_memory -= memory->page_types[index_].count * memory->page_types[index_].size;
          index_++;
        }
      }
    }
    closedir(dir);
    memory->page_types_len = index_;
  }
}

static void
hwloc_get_kerrighed_node_meminfo_info(struct hwloc_topology *topology, unsigned long node, struct hwloc_obj_memory_s *memory)
{
  char path[128];
  uint64_t meminfo_hugepages_count, meminfo_hugepages_size = 0;

  if (topology->is_thissystem) {
    memory->page_types_len = 2;
    memory->page_types = malloc(2*sizeof(*memory->page_types));
    memset(memory->page_types, 0, 2*sizeof(*memory->page_types));
    /* Try to get the hugepage size from sysconf in case we fail to get it from /proc/meminfo later */
#ifdef HAVE__SC_LARGE_PAGESIZE
    memory->page_types[1].size = sysconf(_SC_LARGE_PAGESIZE);
#endif
    memory->page_types[0].size = getpagesize();
  }

  snprintf(path, sizeof(path), "/proc/nodes/node%lu/meminfo", node);
  hwloc_parse_meminfo_info(topology, path, 0 /* no prefix */,
			   &memory->local_memory,
			   &meminfo_hugepages_count, &meminfo_hugepages_size,
			   memory->page_types == NULL);

  if (memory->page_types) {
    uint64_t remaining_local_memory = memory->local_memory;
    if (meminfo_hugepages_size) {
      memory->page_types[1].size = meminfo_hugepages_size;
      memory->page_types[1].count = meminfo_hugepages_count;
      remaining_local_memory -= meminfo_hugepages_count * meminfo_hugepages_size;
    } else {
      memory->page_types_len = 1;
    }
    memory->page_types[0].count = remaining_local_memory / memory->page_types[0].size;
  }
}

static void
hwloc_get_procfs_meminfo_info(struct hwloc_topology *topology, struct hwloc_obj_memory_s *memory)
{
  uint64_t meminfo_hugepages_count, meminfo_hugepages_size = 0;
  struct stat st;
  int has_sysfs_hugepages = 0;
  int types = 2;
  int err;

  err = hwloc_stat("/sys/kernel/mm/hugepages", &st, topology->backend_params.sysfs.root_fd);
  if (!err) {
    types = 1 + st.st_nlink-2;
    has_sysfs_hugepages = 1;
  }

  if (topology->is_thissystem) {
    memory->page_types_len = types;
    memory->page_types = malloc(types*sizeof(*memory->page_types));
    memset(memory->page_types, 0, types*sizeof(*memory->page_types));
    /* Try to get the hugepage size from sysconf in case we fail to get it from /proc/meminfo later */
#ifdef HAVE__SC_LARGE_PAGESIZE
    memory->page_types[1].size = sysconf(_SC_LARGE_PAGESIZE);
#endif
    memory->page_types[0].size = getpagesize();
  }

  hwloc_parse_meminfo_info(topology, "/proc/meminfo", 0 /* no prefix */,
			   &memory->local_memory,
			   &meminfo_hugepages_count, &meminfo_hugepages_size,
			   memory->page_types == NULL);

  if (memory->page_types) {
    uint64_t remaining_local_memory = memory->local_memory;
    if (has_sysfs_hugepages) {
      /* read from node%d/hugepages/hugepages-%skB/nr_hugepages */
      hwloc_parse_hugepages_info(topology, "/sys/kernel/mm/hugepages", memory, &remaining_local_memory);
    } else {
      /* use what we found in meminfo */
      if (meminfo_hugepages_size) {
        memory->page_types[1].size = meminfo_hugepages_size;
        memory->page_types[1].count = meminfo_hugepages_count;
        remaining_local_memory -= meminfo_hugepages_count * meminfo_hugepages_size;
      } else {
        memory->page_types_len = 1;
      }
    }
    memory->page_types[0].count = remaining_local_memory / memory->page_types[0].size;
  }
}

static void
hwloc_sysfs_node_meminfo_info(struct hwloc_topology *topology,
			     const char *syspath, int node,
			     struct hwloc_obj_memory_s *memory)
{
  char path[SYSFS_NUMA_NODE_PATH_LEN];
  char meminfopath[SYSFS_NUMA_NODE_PATH_LEN];
  uint64_t meminfo_hugepages_count = 0;
  uint64_t meminfo_hugepages_size = 0;
  struct stat st;
  int has_sysfs_hugepages = 0;
  int types = 2;
  int err;

  sprintf(path, "%s/node%d/hugepages", syspath, node);
  err = hwloc_stat(path, &st, topology->backend_params.sysfs.root_fd);
  if (!err) {
    types = 1 + st.st_nlink-2;
    has_sysfs_hugepages = 1;
  }

  if (topology->is_thissystem) {
    memory->page_types_len = types;
    memory->page_types = malloc(types*sizeof(*memory->page_types));
    memset(memory->page_types, 0, types*sizeof(*memory->page_types));
  }

  sprintf(meminfopath, "%s/node%d/meminfo", syspath, node);
  hwloc_parse_meminfo_info(topology, meminfopath,
			   hwloc_snprintf(NULL, 0, "Node %d ", node),
			   &memory->local_memory,
			   &meminfo_hugepages_count, NULL /* no hugepage size in node-specific meminfo */,
			   memory->page_types == NULL);

  if (memory->page_types) {
    uint64_t remaining_local_memory = memory->local_memory;
    if (has_sysfs_hugepages) {
      /* read from node%d/hugepages/hugepages-%skB/nr_hugepages */
      hwloc_parse_hugepages_info(topology, path, memory, &remaining_local_memory);
    } else {
      /* get hugepage size from machine-specific meminfo since there is no size in node-specific meminfo,
       * hwloc_get_procfs_meminfo_info must have been called earlier */
      meminfo_hugepages_size = topology->levels[0][0]->memory.page_types[1].size;
      /* use what we found in meminfo */
      if (meminfo_hugepages_size) {
        memory->page_types[1].count = meminfo_hugepages_count;
        memory->page_types[1].size = meminfo_hugepages_size;
        remaining_local_memory -= meminfo_hugepages_count * meminfo_hugepages_size;
      } else {
        memory->page_types_len = 1;
      }
    }
    /* update what's remaining as normal pages */
    memory->page_types[0].size = getpagesize();
    memory->page_types[0].count = remaining_local_memory / memory->page_types[0].size;
  }
}

static void
hwloc_parse_node_distance(const char *distancepath, unsigned nbnodes, float *distances, int fsroot_fd)
{
  char string[4096]; /* enough for hundreds of nodes */
  char *tmp, *next;
  FILE * fd;

  fd = hwloc_fopen(distancepath, "r", fsroot_fd);
  if (!fd)
    return;

  if (!fgets(string, sizeof(string), fd)) {
    fclose(fd);
    return;
  }

  tmp = string;
  while (tmp) {
    unsigned distance = strtoul(tmp, &next, 0);
    if (next == tmp)
      break;
    *distances = (float) distance;
    distances++;
    nbnodes--;
    if (!nbnodes)
      break;
    tmp = next+1;
  }

  fclose(fd);
}

static void
look_sysfsnode(struct hwloc_topology *topology, const char *path, unsigned *found)
{
  unsigned osnode;
  unsigned nbnodes = 0;
  DIR *dir;
  struct dirent *dirent;
  hwloc_obj_t node;
  hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();

  *found = 0;

  /* Get the list of nodes first */
  dir = hwloc_opendir(path, topology->backend_params.sysfs.root_fd);
  if (dir)
    {
      while ((dirent = readdir(dir)) != NULL)
	{
	  if (strncmp(dirent->d_name, "node", 4))
	    continue;
	  osnode = strtoul(dirent->d_name+4, NULL, 0);
	  hwloc_bitmap_set(nodeset, osnode);
	  nbnodes++;
	}
      closedir(dir);
    }

  if (nbnodes <= 1)
    {
      hwloc_bitmap_free(nodeset);
      return;
    }

  /* For convenience, put these declarations inside a block. */

  {
      hwloc_obj_t * nodes = calloc(nbnodes, sizeof(hwloc_obj_t));
      float * distances = calloc(nbnodes*nbnodes, sizeof(float));
      unsigned *indexes = calloc(nbnodes, sizeof(unsigned));
      unsigned index_;

      if (NULL == indexes || NULL == distances || NULL == nodes) {
          free(nodes);
          free(indexes);
          free(distances);
          goto out;
      }

      /* Get node indexes now. We need them in order since Linux groups
       * sparse distances but keep them in order in the sysfs distance files.
       */
      index_ = 0;
      hwloc_bitmap_foreach_begin (osnode, nodeset) {
	indexes[index_] = osnode;
	index_++;
      } hwloc_bitmap_foreach_end();
      hwloc_bitmap_free(nodeset);

#ifdef HWLOC_DEBUG
      hwloc_debug("%s", "numa distance indexes: ");
      for (index_ = 0; index_ < nbnodes; index_++) {
	hwloc_debug(" %u", indexes[index_]);
      }
      hwloc_debug("%s", "\n");
#endif

      /* Get actual distances now */
      for (index_ = 0; index_ < nbnodes; index_++) {
          char nodepath[SYSFS_NUMA_NODE_PATH_LEN];
          hwloc_bitmap_t cpuset;
	  osnode = indexes[index_];

          sprintf(nodepath, "%s/node%u/cpumap", path, osnode);
          cpuset = hwloc_parse_cpumap(nodepath, topology->backend_params.sysfs.root_fd);
          if (!cpuset)
              continue;

          node = hwloc_alloc_setup_object(HWLOC_OBJ_NODE, osnode);
          node->cpuset = cpuset;
          node->nodeset = hwloc_bitmap_alloc();
          hwloc_bitmap_set(node->nodeset, osnode);

          hwloc_sysfs_node_meminfo_info(topology, path, osnode, &node->memory);

          hwloc_debug_1arg_bitmap("os node %u has cpuset %s\n",
                                  osnode, node->cpuset);
          hwloc_insert_object_by_cpuset(topology, node);
          nodes[index_] = node;

	  /* Linux nodeX/distance file contains distance from X to other localities (from ACPI SLIT table or so),
	   * store them in slots X*N...X*N+N-1 */
          sprintf(nodepath, "%s/node%u/distance", path, osnode);
          hwloc_parse_node_distance(nodepath, nbnodes, distances+index_*nbnodes, topology->backend_params.sysfs.root_fd);
      }

      hwloc_topology__set_distance_matrix(topology, HWLOC_OBJ_NODE, nbnodes, indexes, nodes, distances);
  }

 out:
  *found = nbnodes;
}

/* Reads the entire file and returns bytes read if bytes_read != NULL
 * Returned pointer can be freed by using free().  */
static void * 
hwloc_read_raw(const char *p, const char *p1, size_t *bytes_read, int root_fd)
{
  char *fname = NULL;
  char *ret = NULL;
  struct stat fs;
  int file = -1;
  unsigned len;

  len = strlen(p) + 1 + strlen(p1) + 1;
  fname = malloc(len);
  if (NULL == fname) {
      return NULL;
  }
  snprintf(fname, len, "%s/%s", p, p1);

  file = hwloc_open(fname, root_fd);
  if (-1 == file) {
      goto out;
  }
  if (fstat(file, &fs)) {
    goto out;
  }

  ret = (char *) malloc(fs.st_size);
  if (NULL != ret) {
    ssize_t cb = read(file, ret, fs.st_size);
    if (cb == -1) {
      free(ret);
      ret = NULL;
    } else {
      if (NULL != bytes_read)
        *bytes_read = cb;
    }
  }

 out:
  close(file);
  if (NULL != fname) {
      free(fname);
  }
  return ret;
}

/* Reads the entire file and returns it as a 0-terminated string
 * Returned pointer can be freed by using free().  */
static char *
hwloc_read_str(const char *p, const char *p1, int root_fd)
{
  size_t cb = 0;
  char *ret = hwloc_read_raw(p, p1, &cb, root_fd);
  if ((NULL != ret) && (0 < cb) && (0 != ret[cb-1])) {
    ret = realloc(ret, cb + 1);
    ret[cb] = 0;
  }
  return ret;
}

/* Reads first 32bit bigendian value */
static ssize_t 
hwloc_read_unit32be(const char *p, const char *p1, uint32_t *buf, int root_fd)
{
  size_t cb = 0;
  uint32_t *tmp = hwloc_read_raw(p, p1, &cb, root_fd);
  if (sizeof(*buf) != cb) {
    errno = EINVAL;
    return -1;
  }
  *buf = htonl(*tmp);
  free(tmp);
  return sizeof(*buf);
}

typedef struct {
  unsigned int n, allocated;
  struct {
    hwloc_bitmap_t cpuset;
    uint32_t phandle;
    uint32_t l2_cache;
    char *name;
  } *p;
} device_tree_cpus_t;

static void
add_device_tree_cpus_node(device_tree_cpus_t *cpus, hwloc_bitmap_t cpuset,
    uint32_t l2_cache, uint32_t phandle, const char *name)
{
  if (cpus->n == cpus->allocated) {
    if (!cpus->allocated)
      cpus->allocated = 64;
    else
      cpus->allocated *= 2;
    cpus->p = realloc(cpus->p, cpus->allocated * sizeof(cpus->p[0]));
  }
  cpus->p[cpus->n].phandle = phandle;
  cpus->p[cpus->n].cpuset = (NULL == cpuset)?NULL:hwloc_bitmap_dup(cpuset);
  cpus->p[cpus->n].l2_cache = l2_cache;
  cpus->p[cpus->n].name = strdup(name);
  ++cpus->n;
}

/* Walks over the cache list in order to detect nested caches and CPU mask for each */
static int
look_powerpc_device_tree_discover_cache(device_tree_cpus_t *cpus,
    uint32_t phandle, unsigned int *level, hwloc_bitmap_t cpuset)
{
  unsigned int i;
  int ret = -1;
  if ((NULL == level) || (NULL == cpuset) || phandle == (uint32_t) -1)
    return ret;
  for (i = 0; i < cpus->n; ++i) {
    if (phandle != cpus->p[i].l2_cache)
      continue;
    if (NULL != cpus->p[i].cpuset) {
      hwloc_bitmap_or(cpuset, cpuset, cpus->p[i].cpuset);
      ret = 0;
    } else {
      ++(*level);
      if (0 == look_powerpc_device_tree_discover_cache(cpus,
            cpus->p[i].phandle, level, cpuset))
        ret = 0;
    }
  }
  return ret;
}

static void
try_add_cache_from_device_tree_cpu(struct hwloc_topology *topology,
  const char *cpu, unsigned int level, hwloc_bitmap_t cpuset)
{
  /* Ignore Instruction caches */
  /* d-cache-block-size - ignore */
  /* d-cache-line-size - to read, in bytes */
  /* d-cache-sets - ignore */
  /* d-cache-size - to read, in bytes */ 
  /* d-tlb-sets - ignore */
  /* d-tlb-size - ignore, always 0 on power6 */
  /* i-cache-* and i-tlb-* represent instruction cache, ignore */
  uint32_t d_cache_line_size = 0, d_cache_size = 0;
  struct hwloc_obj *c = NULL;

  hwloc_read_unit32be(cpu, "d-cache-line-size", &d_cache_line_size,
      topology->backend_params.sysfs.root_fd);
  hwloc_read_unit32be(cpu, "d-cache-size", &d_cache_size,
      topology->backend_params.sysfs.root_fd);

  if ( (0 == d_cache_line_size) && (0 == d_cache_size) )
    return;

  c = hwloc_alloc_setup_object(HWLOC_OBJ_CACHE, -1);
  c->attr->cache.depth = level;
  c->attr->cache.linesize = d_cache_line_size;
  c->attr->cache.size = d_cache_size;
  c->cpuset = hwloc_bitmap_dup(cpuset);
  hwloc_debug_1arg_bitmap("cache depth %d has cpuset %s\n", level, c->cpuset);
  hwloc_insert_object_by_cpuset(topology, c);
}

/* 
 * Discovers L1/L2/L3 cache information on IBM PowerPC systems for old kernels (RHEL5.*)
 * which provide NUMA nodes information without any details
 */
static void
look_powerpc_device_tree(struct hwloc_topology *topology)
{
  device_tree_cpus_t cpus;
  const char ofroot[] = "/proc/device-tree/cpus";
  unsigned int i;
  int root_fd = topology->backend_params.sysfs.root_fd;
  DIR *dt = hwloc_opendir(ofroot, root_fd);
  struct dirent *dirent;

  cpus.n = 0;
  cpus.p = NULL;
  cpus.allocated = 0;

  if (NULL == dt)
    return;

  while (NULL != (dirent = readdir(dt))) {
    struct stat statbuf;
    int err;
    char *cpu;
    char *device_type;
    uint32_t reg = -1, l2_cache = -1, phandle = -1;
    unsigned len;

    if ('.' == dirent->d_name[0])
      continue;

    len = sizeof(ofroot) + 1 + strlen(dirent->d_name) + 1;
    cpu = malloc(len);
    if (NULL == cpu) {
      continue;
    }
    snprintf(cpu, len, "%s/%s", ofroot, dirent->d_name);

    err = hwloc_stat(cpu, &statbuf, root_fd);
    if (err < 0 || !S_ISDIR(statbuf.st_mode))
      goto cont;

    device_type = hwloc_read_str(cpu, "device_type", root_fd);
    if (NULL == device_type)
      goto cont;

    hwloc_read_unit32be(cpu, "reg", &reg, root_fd);
    if (hwloc_read_unit32be(cpu, "next-level-cache", &l2_cache, root_fd) == -1)
      hwloc_read_unit32be(cpu, "l2-cache", &l2_cache, root_fd);
    if (hwloc_read_unit32be(cpu, "phandle", &phandle, root_fd) == -1)
      if (hwloc_read_unit32be(cpu, "ibm,phandle", &phandle, root_fd) == -1)
        hwloc_read_unit32be(cpu, "linux,phandle", &phandle, root_fd);

    if (0 == strcmp(device_type, "cache")) {
      add_device_tree_cpus_node(&cpus, NULL, l2_cache, phandle, dirent->d_name); 
    }
    else if (0 == strcmp(device_type, "cpu")) {
      /* Found CPU */
      hwloc_bitmap_t cpuset = NULL;
      size_t cb = 0;
      uint32_t *threads = hwloc_read_raw(cpu, "ibm,ppc-interrupt-server#s", &cb, root_fd);
      uint32_t nthreads = cb / sizeof(threads[0]);

      if (NULL != threads) {
        cpuset = hwloc_bitmap_alloc();
        for (i = 0; i < nthreads; ++i) {
          if (hwloc_bitmap_isset(topology->levels[0][0]->complete_cpuset, ntohl(threads[i])))
            hwloc_bitmap_set(cpuset, ntohl(threads[i]));
        }
        free(threads);
      } else if ((unsigned int)-1 != reg) {
        cpuset = hwloc_bitmap_alloc();
        hwloc_bitmap_set(cpuset, reg);
      }

      if (NULL == cpuset) {
        hwloc_debug("%s has no \"reg\" property, skipping\n", cpu);
      } else {
        struct hwloc_obj *core = NULL;
        add_device_tree_cpus_node(&cpus, cpuset, l2_cache, phandle, dirent->d_name); 

        /* Add core */
        core = hwloc_alloc_setup_object(HWLOC_OBJ_CORE, reg);
        core->cpuset = hwloc_bitmap_dup(cpuset);
        hwloc_insert_object_by_cpuset(topology, core);

        /* Add L1 cache */
        try_add_cache_from_device_tree_cpu(topology, cpu, 1, cpuset);

        hwloc_bitmap_free(cpuset);
      }
      free(device_type);
    }
cont:
    free(cpu);
  }
  closedir(dt);

  /* No cores and L2 cache were found, exiting */
  if (0 == cpus.n) {
    hwloc_debug("No cores and L2 cache were found in %s, exiting\n", ofroot);
    return;
  }

#ifdef HWLOC_DEBUG
  for (i = 0; i < cpus.n; ++i) {
    hwloc_debug("%i: %s  ibm,phandle=%08X l2_cache=%08X ",
      i, cpus.p[i].name, cpus.p[i].phandle, cpus.p[i].l2_cache);
    if (NULL == cpus.p[i].cpuset) {
      hwloc_debug("%s\n", "no cpuset");
    } else {
      hwloc_debug_bitmap("cpuset %s\n", cpus.p[i].cpuset);
    }
  }
#endif

  /* Scan L2/L3/... caches */
  for (i = 0; i < cpus.n; ++i) {
    unsigned int level = 2;
    hwloc_bitmap_t cpuset;
    /* Skip real CPUs */
    if (NULL != cpus.p[i].cpuset)
      continue;

    /* Calculate cache level and CPU mask */
    cpuset = hwloc_bitmap_alloc();
    if (0 == look_powerpc_device_tree_discover_cache(&cpus,
          cpus.p[i].phandle, &level, cpuset)) {
      char *cpu;
      unsigned len;

      len = sizeof(ofroot) + 1 + strlen(cpus.p[i].name) + 1;
      cpu = malloc(len);
      if (NULL == cpu) {
          return;
      }
      snprintf(cpu, len, "%s/%s", ofroot, cpus.p[i].name);

      try_add_cache_from_device_tree_cpu(topology, cpu, level, cpuset);
      free(cpu);
    }
    hwloc_bitmap_free(cpuset);
  }

  /* Do cleanup */
  for (i = 0; i < cpus.n; ++i) {
    hwloc_bitmap_free(cpus.p[i].cpuset);
    free(cpus.p[i].name);
  }
  free(cpus.p);
}

/* Look at Linux' /sys/devices/system/cpu/cpu%d/topology/ */
static void
look_sysfscpu(struct hwloc_topology *topology, const char *path)
{
  hwloc_bitmap_t cpuset; /* Set of cpus for which we have topology information */
#define CPU_TOPOLOGY_STR_LEN 128
  char str[CPU_TOPOLOGY_STR_LEN];
  DIR *dir;
  int i,j;
  FILE *fd;
  unsigned caches_added;

  cpuset = hwloc_bitmap_alloc();

  /* fill the cpuset of interesting cpus */
  dir = hwloc_opendir(path, topology->backend_params.sysfs.root_fd);
  if (dir) {
    struct dirent *dirent;
    while ((dirent = readdir(dir)) != NULL) {
      unsigned long cpu;
      char online[2];

      if (strncmp(dirent->d_name, "cpu", 3))
	continue;
      cpu = strtoul(dirent->d_name+3, NULL, 0);

      /* Maybe we don't have topology information but at least it exists */
      hwloc_bitmap_set(topology->levels[0][0]->complete_cpuset, cpu);

      /* check whether this processor is online */
      sprintf(str, "%s/cpu%lu/online", path, cpu);
      fd = hwloc_fopen(str, "r", topology->backend_params.sysfs.root_fd);
      if (fd) {
	if (fgets(online, sizeof(online), fd)) {
	  fclose(fd);
	  if (atoi(online)) {
	    hwloc_debug("os proc %lu is online\n", cpu);
	  } else {
	    hwloc_debug("os proc %lu is offline\n", cpu);
            hwloc_bitmap_clr(topology->levels[0][0]->online_cpuset, cpu);
	  }
	} else {
	  fclose(fd);
	}
      }

      /* check whether the kernel exports topology information for this cpu */
      sprintf(str, "%s/cpu%lu/topology", path, cpu);
      if (hwloc_access(str, X_OK, topology->backend_params.sysfs.root_fd) < 0 && errno == ENOENT) {
	hwloc_debug("os proc %lu has no accessible %s/cpu%lu/topology\n",
		   cpu, path, cpu);
	continue;
      }

      hwloc_bitmap_set(cpuset, cpu);
    }
    closedir(dir);
  }

  topology->support.discovery->pu = 1;
  hwloc_debug_1arg_bitmap("found %d cpu topologies, cpuset %s\n",
	     hwloc_bitmap_weight(cpuset), cpuset);

  caches_added = 0;
  hwloc_bitmap_foreach_begin(i, cpuset)
    {
      struct hwloc_obj *sock, *core, *thread;
      hwloc_bitmap_t socketset, coreset, threadset, savedcoreset;
      unsigned mysocketid, mycoreid;
      int threadwithcoreid = 0;

      /* look at the socket */
      mysocketid = 0; /* shut-up the compiler */
      sprintf(str, "%s/cpu%d/topology/physical_package_id", path, i);
      hwloc_parse_sysfs_unsigned(str, &mysocketid, topology->backend_params.sysfs.root_fd);

      sprintf(str, "%s/cpu%d/topology/core_siblings", path, i);
      socketset = hwloc_parse_cpumap(str, topology->backend_params.sysfs.root_fd);
      if (socketset && hwloc_bitmap_first(socketset) == i) {
        /* first cpu in this socket, add the socket */
        sock = hwloc_alloc_setup_object(HWLOC_OBJ_SOCKET, mysocketid);
        sock->cpuset = socketset;
        hwloc_debug_1arg_bitmap("os socket %u has cpuset %s\n",
                     mysocketid, socketset);
        hwloc_insert_object_by_cpuset(topology, sock);
        socketset = NULL; /* don't free it */
      }
      hwloc_bitmap_free(socketset);

      /* look at the core */
      mycoreid = 0; /* shut-up the compiler */
      sprintf(str, "%s/cpu%d/topology/core_id", path, i);
      hwloc_parse_sysfs_unsigned(str, &mycoreid, topology->backend_params.sysfs.root_fd);

      sprintf(str, "%s/cpu%d/topology/thread_siblings", path, i);
      coreset = hwloc_parse_cpumap(str, topology->backend_params.sysfs.root_fd);
      savedcoreset = coreset; /* store it for later work-arounds */

      if (coreset && hwloc_bitmap_weight(coreset) > 1) {
	/* check if this is hyperthreading or different coreids */
	unsigned siblingid, siblingcoreid;
	hwloc_bitmap_t set = hwloc_bitmap_dup(coreset);
	hwloc_bitmap_clr(set, i);
	siblingid = hwloc_bitmap_first(set);
	siblingcoreid = mycoreid;
	sprintf(str, "%s/cpu%d/topology/core_id", path, siblingid);
	hwloc_parse_sysfs_unsigned(str, &siblingcoreid, topology->backend_params.sysfs.root_fd);
	threadwithcoreid = (siblingcoreid != mycoreid);
	hwloc_bitmap_free(set);
      }


      if (coreset && (hwloc_bitmap_first(coreset) == i || threadwithcoreid)) {
	/* regular core */
        core = hwloc_alloc_setup_object(HWLOC_OBJ_CORE, mycoreid);
	if (threadwithcoreid) {
	  /* amd multicore compute-unit, create one core per thread */
	  core->cpuset = hwloc_bitmap_alloc();
	  hwloc_bitmap_set(core->cpuset, i);
	} else {
	  core->cpuset = coreset;
	}
        hwloc_debug_1arg_bitmap("os core %u has cpuset %s\n",
                     mycoreid, coreset);
        hwloc_insert_object_by_cpuset(topology, core);
        coreset = NULL; /* don't free it */
      }

      /* look at the thread */
      threadset = hwloc_bitmap_alloc();
      hwloc_bitmap_only(threadset, i);

      /* add the thread */
      thread = hwloc_alloc_setup_object(HWLOC_OBJ_PU, i);
      thread->cpuset = threadset;
      hwloc_debug_1arg_bitmap("thread %d has cpuset %s\n",
		 i, threadset);
      hwloc_insert_object_by_cpuset(topology, thread);

      /* look at the caches */
      for(j=0; j<10; j++) {
#define SHARED_CPU_MAP_STRLEN 128
	char mappath[SHARED_CPU_MAP_STRLEN];
	char str2[20]; /* enough for a level number (one digit) or a type (Data/Instruction/Unified) */
	struct hwloc_obj *cache;
	hwloc_bitmap_t cacheset;
	unsigned long kB = 0;
	unsigned linesize = 0;
	int depth; /* 0 for L1, .... */

	/* get the cache level depth */
	sprintf(mappath, "%s/cpu%d/cache/index%d/level", path, i, j);
	fd = hwloc_fopen(mappath, "r", topology->backend_params.sysfs.root_fd);
	if (fd) {
	  if (fgets(str2,sizeof(str2), fd))
	    depth = strtoul(str2, NULL, 10)-1;
	  else
	    continue;
	  fclose(fd);
	} else
	  continue;

	/* ignore Instruction caches */
	sprintf(mappath, "%s/cpu%d/cache/index%d/type", path, i, j);
	fd = hwloc_fopen(mappath, "r", topology->backend_params.sysfs.root_fd);
	if (fd) {
	  if (fgets(str2, sizeof(str2), fd)) {
	    fclose(fd);
	    if (!strncmp(str2, "Instruction", 11))
	      continue;
	  } else {
	    fclose(fd);
	    continue;
	  }
	} else
	  continue;

	/* get the cache size */
	sprintf(mappath, "%s/cpu%d/cache/index%d/size", path, i, j);
	fd = hwloc_fopen(mappath, "r", topology->backend_params.sysfs.root_fd);
	if (fd) {
	  if (fgets(str2,sizeof(str2), fd))
	    kB = atol(str2); /* in kB */
	  fclose(fd);
	}

	/* get the line size */
	sprintf(mappath, "%s/cpu%d/cache/index%d/coherency_line_size", path, i, j);
	fd = hwloc_fopen(mappath, "r", topology->backend_params.sysfs.root_fd);
	if (fd) {
	  if (fgets(str2,sizeof(str2), fd))
	    linesize = atol(str2); /* in bytes */
	  fclose(fd);
	}

	sprintf(mappath, "%s/cpu%d/cache/index%d/shared_cpu_map", path, i, j);
	cacheset = hwloc_parse_cpumap(mappath, topology->backend_params.sysfs.root_fd);
        if (cacheset) {
          if (hwloc_bitmap_weight(cacheset) < 1) {
            /* mask is wrong (useful for many itaniums) */
            if (savedcoreset)
              /* assume it's a core-specific cache */
              hwloc_bitmap_copy(cacheset, savedcoreset);
            else
              /* assumes it's not shared */
              hwloc_bitmap_only(cacheset, i);
          }

          if (hwloc_bitmap_first(cacheset) == i) {
            /* first cpu in this cache, add the cache */
            cache = hwloc_alloc_setup_object(HWLOC_OBJ_CACHE, -1);
            cache->attr->cache.size = kB << 10;
            cache->attr->cache.depth = depth+1;
            cache->attr->cache.linesize = linesize;
            cache->cpuset = cacheset;
            hwloc_debug_1arg_bitmap("cache depth %d has cpuset %s\n",
                       depth, cacheset);
            hwloc_insert_object_by_cpuset(topology, cache);
            cacheset = NULL; /* don't free it */
            ++caches_added;
          }
        }
        hwloc_bitmap_free(cacheset);
      }
      hwloc_bitmap_free(coreset);
    }
  hwloc_bitmap_foreach_end();

  if (0 == caches_added)
    look_powerpc_device_tree(topology);

  hwloc_bitmap_free(cpuset);
}


/* Look at Linux' /proc/cpuinfo */
#      define PROCESSOR	"processor"
#      define PHYSID "physical id"
#      define COREID "core id"
#define HWLOC_NBMAXCPUS 1024 /* FIXME: drop */
static int
look_cpuinfo(struct hwloc_topology *topology, const char *path,
	     hwloc_bitmap_t online_cpuset)
{
  FILE *fd;
  char *str = NULL;
  char *endptr;
  unsigned len;
  unsigned proc_physids[HWLOC_NBMAXCPUS];
  unsigned osphysids[HWLOC_NBMAXCPUS];
  unsigned proc_coreids[HWLOC_NBMAXCPUS];
  unsigned oscoreids[HWLOC_NBMAXCPUS];
  unsigned proc_osphysids[HWLOC_NBMAXCPUS];
  unsigned core_osphysids[HWLOC_NBMAXCPUS];
  unsigned procid_max=0;
  unsigned numprocs=0;
  unsigned numsockets=0;
  unsigned numcores=0;
  unsigned long physid;
  unsigned long coreid;
  unsigned missingsocket;
  unsigned missingcore;
  unsigned long processor = (unsigned long) -1;
  unsigned i;
  hwloc_bitmap_t cpuset;
  hwloc_obj_t obj;

  for (i = 0; i < HWLOC_NBMAXCPUS; i++) {
    proc_physids[i] = -1;
    osphysids[i] = -1;
    proc_coreids[i] = -1;
    oscoreids[i] = -1;
    proc_osphysids[i] = -1;
    core_osphysids[i] = -1;
  }

  if (!(fd=hwloc_fopen(path,"r", topology->backend_params.sysfs.root_fd)))
    {
      hwloc_debug("%s", "could not open /proc/cpuinfo\n");
      return -1;
    }

  cpuset = hwloc_bitmap_alloc();
  /* Just record information and count number of sockets and cores */

  len = strlen(PHYSID) + 1 + 9 + 1 + 1;
  str = malloc(len);
  hwloc_debug("%s", "\n\n * Topology extraction from /proc/cpuinfo *\n\n");
  while (fgets(str,len,fd)!=NULL)
    {
#      define getprocnb_begin(field, var)		     \
      if ( !strncmp(field,str,strlen(field)))	     \
	{						     \
	char *c = strchr(str, ':')+1;		     \
	var = strtoul(c,&endptr,0);			     \
	if (endptr==c)							\
	  {								\
            hwloc_debug("%s", "no number in "field" field of /proc/cpuinfo\n"); \
            hwloc_bitmap_free(cpuset);					\
            free(str);							\
            return -1;							\
	  }								\
	else if (var==ULONG_MAX)						\
	  {								\
            hwloc_debug("%s", "too big "field" number in /proc/cpuinfo\n"); \
            hwloc_bitmap_free(cpuset);					\
            free(str);							\
            return -1;							\
	  }								\
	hwloc_debug(field " %lu\n", var)
#      define getprocnb_end()			\
      }
      getprocnb_begin(PROCESSOR,processor);
      hwloc_bitmap_set(cpuset, processor);

      obj = hwloc_alloc_setup_object(HWLOC_OBJ_PU, processor);
      obj->cpuset = hwloc_bitmap_alloc();
      hwloc_bitmap_only(obj->cpuset, processor);

      hwloc_debug_2args_bitmap("cpu %u (os %lu) has cpuset %s\n",
		 numprocs, processor, obj->cpuset);
      numprocs++;
      hwloc_insert_object_by_cpuset(topology, obj);

      getprocnb_end() else
      getprocnb_begin(PHYSID,physid);
      proc_osphysids[processor]=physid;
      for (i=0; i<numsockets; i++)
	if (physid == osphysids[i])
	  break;
      proc_physids[processor]=i;
      hwloc_debug("%lu on socket %u (%lx)\n", processor, i, physid);
      if (i==numsockets)
	osphysids[(numsockets)++] = physid;
      getprocnb_end() else
      getprocnb_begin(COREID,coreid);
      for (i=0; i<numcores; i++)
	if (coreid == oscoreids[i] && proc_osphysids[processor] == core_osphysids[i])
	  break;
      proc_coreids[processor]=i;
      if (i==numcores)
	{
	  core_osphysids[numcores] = proc_osphysids[processor];
	  oscoreids[numcores] = coreid;
	  (numcores)++;
	}
      getprocnb_end()
	if (str[strlen(str)-1]!='\n')
	  {
            /* ignore end of line */
	    if (fscanf(fd,"%*[^\n]") == EOF)
	      break;
	    getc(fd);
	  }
    }
  fclose(fd);
  free(str);

  if (processor == (unsigned long) -1) {
    hwloc_bitmap_free(cpuset);
    return -1;
  }

  topology->support.discovery->pu = 1;
  /* setup the final number of procs */
  procid_max = processor + 1;
  hwloc_bitmap_copy(online_cpuset, cpuset);
  hwloc_bitmap_free(cpuset);

  hwloc_debug("%u online processors found, with id max %u\n", numprocs, procid_max);
  hwloc_debug_bitmap("online processor cpuset: %s\n", online_cpuset);

  hwloc_debug("%s", "\n * Topology summary *\n");
  hwloc_debug("%u processors (%u max id)\n", numprocs, procid_max);

  /* Some buggy Linuxes don't provide numbers for processor 0, which makes us
   * provide bogus information. We should rather drop it. */
  missingsocket=0;
  missingcore=0;
  hwloc_bitmap_foreach_begin(processor, online_cpuset)
    if (proc_physids[processor] == (unsigned) -1)
      missingsocket=1;
    if (proc_coreids[processor] == (unsigned) -1)
      missingcore=1;
    if (missingcore && missingsocket)
      /* No usable information, no need to continue */
      break;
  hwloc_bitmap_foreach_end();

  hwloc_debug("%u sockets%s\n", numsockets, missingsocket ? ", but some missing socket" : "");
  if (!missingsocket && numsockets>0)
    hwloc_setup_level(procid_max, numsockets, osphysids, proc_physids, topology, HWLOC_OBJ_SOCKET);

  look_powerpc_device_tree(topology);

  hwloc_debug("%u cores%s\n", numcores, missingcore ? ", but some missing core" : "");
  if (!missingcore && numcores>0)
    hwloc_setup_level(procid_max, numcores, oscoreids, proc_coreids, topology, HWLOC_OBJ_CORE);

  return 0;
}

static void
hwloc__get_dmi_one_info(struct hwloc_topology *topology, hwloc_obj_t obj, const char *sysfs_name, const char *hwloc_name)
{
  char sysfs_path[128];
  char dmi_line[64];
  char *tmp;
  FILE *fd;

  snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/dmi/id/%s", sysfs_name);

  dmi_line[0] = '\0';
  fd = hwloc_fopen(sysfs_path, "r", topology->backend_params.sysfs.root_fd);
  if (fd) {
    tmp = fgets(dmi_line, sizeof(dmi_line), fd);
    fclose (fd);
    if (tmp && dmi_line[0] != '\0') {
      tmp = strchr(dmi_line, '\n');
      if (tmp)
	*tmp = '\0';
      hwloc_debug("found %s '%s'\n", hwloc_name, dmi_line);
      hwloc_add_object_info(obj, hwloc_name, dmi_line);
    }
  }
}

static void
hwloc__get_dmi_info(struct hwloc_topology *topology, hwloc_obj_t obj)
{
  hwloc__get_dmi_one_info(topology, obj, "product_name", "DMIProductName");
  hwloc__get_dmi_one_info(topology, obj, "product_version", "DMIProductVersion");
  hwloc__get_dmi_one_info(topology, obj, "product_serial", "DMIProductSerial");
  hwloc__get_dmi_one_info(topology, obj, "product_uuid", "DMIProductUUID");
  hwloc__get_dmi_one_info(topology, obj, "board_vendor", "DMIBoardVendor");
  hwloc__get_dmi_one_info(topology, obj, "board_name", "DMIBoardName");
  hwloc__get_dmi_one_info(topology, obj, "board_version", "DMIBoardVersion");
  hwloc__get_dmi_one_info(topology, obj, "board_serial", "DMIBoardSerial");
  hwloc__get_dmi_one_info(topology, obj, "board_asset_tag", "DMIBoardAssetTag");
  hwloc__get_dmi_one_info(topology, obj, "chassis_vendor", "DMIChassisVendor");
  hwloc__get_dmi_one_info(topology, obj, "chassis_type", "DMIChassisType");
  hwloc__get_dmi_one_info(topology, obj, "chassis_version", "DMIChassisVersion");
  hwloc__get_dmi_one_info(topology, obj, "chassis_serial", "DMIChassisSerial");
  hwloc__get_dmi_one_info(topology, obj, "chassis_asset_tag", "DMIChassisAssetTag");
  hwloc__get_dmi_one_info(topology, obj, "bios_vendor", "DMIBIOSVendor");
  hwloc__get_dmi_one_info(topology, obj, "bios_version", "DMIBIOSVersion");
  hwloc__get_dmi_one_info(topology, obj, "bios_date", "DMIBIOSDate");
  hwloc__get_dmi_one_info(topology, obj, "sys_vendor", "DMISysVendor");
}

void
hwloc_look_linux(struct hwloc_topology *topology)
{
  DIR *nodes_dir;
  unsigned nbnodes;
  char *cpuset_mntpnt, *cgroup_mntpnt, *cpuset_name = NULL;
  int err;

  /* Gather the list of admin-disabled cpus and mems */
  hwloc_find_linux_cpuset_mntpnt(&cgroup_mntpnt, &cpuset_mntpnt, topology->backend_params.sysfs.root_fd);
  if (cgroup_mntpnt || cpuset_mntpnt) {
    cpuset_name = hwloc_read_linux_cpuset_name(topology->backend_params.sysfs.root_fd, topology->pid);
    if (cpuset_name) {
      hwloc_admin_disable_set_from_cpuset(topology, cgroup_mntpnt, cpuset_mntpnt, cpuset_name, "cpus", topology->levels[0][0]->allowed_cpuset);
      hwloc_admin_disable_set_from_cpuset(topology, cgroup_mntpnt, cpuset_mntpnt, cpuset_name, "mems", topology->levels[0][0]->allowed_nodeset);
    }
    free(cgroup_mntpnt);
    free(cpuset_mntpnt);
  }

  nodes_dir = hwloc_opendir("/proc/nodes", topology->backend_params.sysfs.root_fd);
  if (nodes_dir) {
    /* Kerrighed */
    struct dirent *dirent;
    char path[128];
    hwloc_obj_t machine;
    hwloc_bitmap_t machine_online_set;

    /* replace top-level object type with SYSTEM and add some MACHINE underneath */

    topology->levels[0][0]->type = HWLOC_OBJ_SYSTEM;
    topology->levels[0][0]->name = strdup("Kerrighed");

    /* No cpuset support for now.  */
    /* No sys support for now.  */
    while ((dirent = readdir(nodes_dir)) != NULL) {
      unsigned long node;
      if (strncmp(dirent->d_name, "node", 4))
	continue;
      machine_online_set = hwloc_bitmap_alloc();
      node = strtoul(dirent->d_name+4, NULL, 0);
      snprintf(path, sizeof(path), "/proc/nodes/node%lu/cpuinfo", node);
      err = look_cpuinfo(topology, path, machine_online_set);
      if (err < 0)
        continue;
      hwloc_bitmap_or(topology->levels[0][0]->online_cpuset, topology->levels[0][0]->online_cpuset, machine_online_set);
      machine = hwloc_alloc_setup_object(HWLOC_OBJ_MACHINE, node);
      machine->cpuset = machine_online_set;
      hwloc_debug_1arg_bitmap("machine number %lu has cpuset %s\n",
		 node, machine_online_set);
      hwloc_insert_object_by_cpuset(topology, machine);

      /* Get the machine memory attributes */
      hwloc_get_kerrighed_node_meminfo_info(topology, node, &machine->memory);

      /* Gather DMI info */
      /* FIXME: get the right DMI info of each machine */
      hwloc__get_dmi_info(topology, machine);
    }
    closedir(nodes_dir);
  } else {
    /* Get the machine memory attributes */
    hwloc_get_procfs_meminfo_info(topology, &topology->levels[0][0]->memory);

    /* Gather NUMA information. Must be after hwloc_get_procfs_meminfo_info so that the hugepage size is known */
    look_sysfsnode(topology, "/sys/devices/system/node", &nbnodes);

    /* if we found some numa nodes, the machine object has no local memory */
    if (nbnodes) {
      unsigned i;
      topology->levels[0][0]->memory.local_memory = 0;
      if (topology->levels[0][0]->memory.page_types)
        for(i=0; i<topology->levels[0][0]->memory.page_types_len; i++)
          topology->levels[0][0]->memory.page_types[i].count = 0;
    }

    /* Gather the list of cpus now */
    if (getenv("HWLOC_LINUX_USE_CPUINFO")
	|| (hwloc_access("/sys/devices/system/cpu/cpu0/topology/core_siblings", R_OK, topology->backend_params.sysfs.root_fd) < 0
	    && hwloc_access("/sys/devices/system/cpu/cpu0/topology/thread_siblings", R_OK, topology->backend_params.sysfs.root_fd) < 0)) {
	/* revert to reading cpuinfo only if /sys/.../topology unavailable (before 2.6.16)
	 * or not containing anything interesting */
      err = look_cpuinfo(topology, "/proc/cpuinfo", topology->levels[0][0]->online_cpuset);
      if (err < 0) {
        if (topology->is_thissystem)
          hwloc_setup_pu_level(topology, hwloc_fallback_nbprocessors(topology));
        else
          /* fsys-root but not this system, no way, assume there's just 1
           * processor :/ */
          hwloc_setup_pu_level(topology, 1);
      }
    } else {
      look_sysfscpu(topology, "/sys/devices/system/cpu");
    }

    /* Gather DMI info */
    hwloc__get_dmi_info(topology, topology->levels[0][0]);
  }

  hwloc_add_object_info(topology->levels[0][0], "Backend", "Linux");
  if (cpuset_name) {
    hwloc_add_object_info(topology->levels[0][0], "LinuxCgroup", cpuset_name);
    free(cpuset_name);
  }

  /* gather uname info if fsroot wasn't changed */
  if (topology->is_thissystem)
     hwloc_add_uname_info(topology);
}

void
hwloc_set_linux_hooks(struct hwloc_topology *topology)
{
  topology->set_thisthread_cpubind = hwloc_linux_set_thisthread_cpubind;
  topology->get_thisthread_cpubind = hwloc_linux_get_thisthread_cpubind;
  topology->set_thisproc_cpubind = hwloc_linux_set_thisproc_cpubind;
  topology->get_thisproc_cpubind = hwloc_linux_get_thisproc_cpubind;
  topology->set_proc_cpubind = hwloc_linux_set_proc_cpubind;
  topology->get_proc_cpubind = hwloc_linux_get_proc_cpubind;
#if HAVE_DECL_PTHREAD_SETAFFINITY_NP
  topology->set_thread_cpubind = hwloc_linux_set_thread_cpubind;
#endif /* HAVE_DECL_PTHREAD_SETAFFINITY_NP */
#if HAVE_DECL_PTHREAD_GETAFFINITY_NP
  topology->get_thread_cpubind = hwloc_linux_get_thread_cpubind;
#endif /* HAVE_DECL_PTHREAD_GETAFFINITY_NP */
  topology->get_thisthread_last_cpu_location = hwloc_linux_get_thisthread_last_cpu_location;
  topology->get_thisproc_last_cpu_location = hwloc_linux_get_thisproc_last_cpu_location;
  topology->get_proc_last_cpu_location = hwloc_linux_get_proc_last_cpu_location;
#ifdef HWLOC_HAVE_SET_MEMPOLICY
  topology->set_thisthread_membind = hwloc_linux_set_thisthread_membind;
  topology->get_thisthread_membind = hwloc_linux_get_thisthread_membind;
#endif /* HWLOC_HAVE_SET_MEMPOLICY */
#ifdef HWLOC_HAVE_MBIND
  topology->set_area_membind = hwloc_linux_set_area_membind;
  topology->alloc_membind = hwloc_linux_alloc_membind;
  topology->alloc = hwloc_alloc_mmap;
  topology->free_membind = hwloc_free_mmap;
  topology->support.membind->firsttouch_membind = 1;
  topology->support.membind->bind_membind = 1;
  topology->support.membind->interleave_membind = 1;
#endif /* HWLOC_HAVE_MBIND */
#if (defined HWLOC_HAVE_MIGRATE_PAGES) || ((defined HWLOC_HAVE_MBIND) && (defined MPOL_MF_MOVE))
  topology->support.membind->migrate_membind = 1;
#endif
}
