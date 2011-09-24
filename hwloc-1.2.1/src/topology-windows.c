/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2010 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

/* To try to get all declarations duplicated below.  */
#define _WIN32_WINNT 0x0601

#include <private/autogen/config.h>
#include <hwloc.h>
#include <private/private.h>
#include <private/debug.h>

#include <windows.h>

#ifndef HAVE_KAFFINITY
typedef ULONG_PTR KAFFINITY, *PKAFFINITY;
#endif

#ifndef HAVE_PROCESSOR_CACHE_TYPE
typedef enum _PROCESSOR_CACHE_TYPE {
  CacheUnified,
  CacheInstruction,
  CacheData,
  CacheTrace
} PROCESSOR_CACHE_TYPE;
#endif

#ifndef CACHE_FULLY_ASSOCIATIVE
#define CACHE_FULLY_ASSOCIATIVE 0xFF
#endif

#ifndef HAVE_CACHE_DESCRIPTOR
typedef struct _CACHE_DESCRIPTOR {
  BYTE Level;
  BYTE Associativity;
  WORD LineSize;
  DWORD Size; /* in bytes */
  PROCESSOR_CACHE_TYPE Type;
} CACHE_DESCRIPTOR, *PCACHE_DESCRIPTOR;
#endif

#ifndef HAVE_LOGICAL_PROCESSOR_RELATIONSHIP
typedef enum _LOGICAL_PROCESSOR_RELATIONSHIP {
  RelationProcessorCore,
  RelationNumaNode,
  RelationCache,
  RelationProcessorPackage,
  RelationGroup,
  RelationAll = 0xffff
} LOGICAL_PROCESSOR_RELATIONSHIP;
#else /* HAVE_LOGICAL_PROCESSOR_RELATIONSHIP */
#  ifndef HAVE_RELATIONPROCESSORPACKAGE
#    define RelationProcessorPackage 3
#    define RelationGroup 4
#    define RelationAll 0xffff
#  endif /* HAVE_RELATIONPROCESSORPACKAGE */
#endif /* HAVE_LOGICAL_PROCESSOR_RELATIONSHIP */

#ifndef HAVE_SYSTEM_LOGICAL_PROCESSOR_INFORMATION
typedef struct _SYSTEM_LOGICAL_PROCESSOR_INFORMATION {
  ULONG_PTR ProcessorMask;
  LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
  _ANONYMOUS_UNION
  union {
    struct {
      BYTE flags;
    } ProcessorCore;
    struct {
      DWORD NodeNumber;
    } NumaNode;
    CACHE_DESCRIPTOR Cache;
    ULONGLONG Reserved[2];
  } DUMMYUNIONNAME;
} SYSTEM_LOGICAL_PROCESSOR_INFORMATION, *PSYSTEM_LOGICAL_PROCESSOR_INFORMATION;
#endif

/* Extended interface, for group support */

#ifndef HAVE_GROUP_AFFINITY
typedef struct _GROUP_AFFINITY {
  KAFFINITY Mask;
  WORD Group;
  WORD Reserved[3];
} GROUP_AFFINITY, *PGROUP_AFFINITY;
#endif

#ifndef HAVE_PROCESSOR_RELATIONSHIP
typedef struct _PROCESSOR_RELATIONSHIP {
  BYTE Flags;
  BYTE Reserved[21];
  WORD GroupCount;
  GROUP_AFFINITY GroupMask[ANYSIZE_ARRAY];
} PROCESSOR_RELATIONSHIP, *PPROCESSOR_RELATIONSHIP;
#endif

#ifndef HAVE_NUMA_NODE_RELATIONSHIP
typedef struct _NUMA_NODE_RELATIONSHIP {
  DWORD NodeNumber;
  BYTE Reserved[20];
  GROUP_AFFINITY GroupMask;
} NUMA_NODE_RELATIONSHIP, *PNUMA_NODE_RELATIONSHIP;
#endif

#ifndef HAVE_CACHE_RELATIONSHIP
typedef struct _CACHE_RELATIONSHIP {
  BYTE Level;
  BYTE Associativity;
  WORD LineSize;
  DWORD CacheSize;
  PROCESSOR_CACHE_TYPE Type;
  BYTE Reserved[20];
  GROUP_AFFINITY GroupMask;
} CACHE_RELATIONSHIP, *PCACHE_RELATIONSHIP;
#endif

#ifndef HAVE_PROCESSOR_GROUP_INFO
typedef struct _PROCESSOR_GROUP_INFO {
  BYTE MaximumProcessorCount;
  BYTE ActiveProcessorCount;
  BYTE Reserved[38];
  KAFFINITY ActiveProcessorMask;
} PROCESSOR_GROUP_INFO, *PPROCESSOR_GROUP_INFO;
#endif

#ifndef HAVE_GROUP_RELATIONSHIP
typedef struct _GROUP_RELATIONSHIP {
  WORD MaximumGroupCount;
  WORD ActiveGroupCount;
  ULONGLONG Reserved[2];
  PROCESSOR_GROUP_INFO GroupInfo[ANYSIZE_ARRAY];
} GROUP_RELATIONSHIP, *PGROUP_RELATIONSHIP;
#endif

#ifndef HAVE_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
typedef struct _SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX {
  LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
  DWORD Size;
  _ANONYMOUS_UNION
  union {
    PROCESSOR_RELATIONSHIP Processor;
    NUMA_NODE_RELATIONSHIP NumaNode;
    CACHE_RELATIONSHIP Cache;
    GROUP_RELATIONSHIP Group;
    /* Odd: no member to tell the cpu mask of the package... */
  } DUMMYUNIONNAME;
} SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, *PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;
#endif

#ifndef HAVE_PSAPI_WORKING_SET_EX_BLOCK
typedef union _PSAPI_WORKING_SET_EX_BLOCK {
  ULONG_PTR Flags;
  struct {
    unsigned Valid  :1;
    unsigned ShareCount  :3;
    unsigned Win32Protection  :11;
    unsigned Shared  :1;
    unsigned Node  :6;
    unsigned Locked  :1;
    unsigned LargePage  :1;
  };
} PSAPI_WORKING_SET_EX_BLOCK;
#endif

#ifndef HAVE_PSAPI_WORKING_SET_EX_INFORMATION
typedef struct _PSAPI_WORKING_SET_EX_INFORMATION {
  PVOID VirtualAddress;
  PSAPI_WORKING_SET_EX_BLOCK VirtualAttributes;
} PSAPI_WORKING_SET_EX_INFORMATION;
#endif

/* TODO: SetThreadIdealProcessor */

static int
hwloc_win_set_thread_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_thread_t thread, hwloc_const_bitmap_t hwloc_set, int flags)
{
  if (flags & HWLOC_CPUBIND_NOMEMBIND) {
    errno = ENOSYS;
    return -1;
  }
  /* TODO: groups SetThreadGroupAffinity */
  /* The resulting binding is always strict */
  DWORD mask = hwloc_bitmap_to_ulong(hwloc_set);
  if (!SetThreadAffinityMask(thread, mask))
    return -1;
  return 0;
}

/* TODO: SetThreadGroupAffinity to get affinity */

static int
hwloc_win_set_thisthread_cpubind(hwloc_topology_t topology, hwloc_const_bitmap_t hwloc_set, int flags)
{
  return hwloc_win_set_thread_cpubind(topology, GetCurrentThread(), hwloc_set, flags);
}

static int
hwloc_win_set_thisthread_membind(hwloc_topology_t topology, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags)
{
  int ret;
  hwloc_cpuset_t cpuset;

  if ((policy != HWLOC_MEMBIND_DEFAULT && policy != HWLOC_MEMBIND_BIND)
      || flags & HWLOC_MEMBIND_NOCPUBIND) {
    errno = ENOSYS;
    return -1;
  }

  cpuset = hwloc_bitmap_alloc();
  hwloc_cpuset_from_nodeset(topology, cpuset, nodeset);
  ret = hwloc_win_set_thisthread_cpubind(topology, cpuset, flags & HWLOC_MEMBIND_STRICT?HWLOC_CPUBIND_STRICT:0);
  hwloc_bitmap_free(cpuset);
  return ret;
}

static int
hwloc_win_set_proc_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_pid_t proc, hwloc_const_bitmap_t hwloc_set, int flags)
{
  if (flags & HWLOC_CPUBIND_NOMEMBIND) {
    errno = ENOSYS;
    return -1;
  }
  /* TODO: groups, hard: has to manually bind all threads into the other group,
   * and the bind the process inside the group */
  /* The resulting binding is always strict */
  DWORD mask = hwloc_bitmap_to_ulong(hwloc_set);
  if (!SetProcessAffinityMask(proc, mask))
    return -1;
  return 0;
}

static int
hwloc_win_set_proc_membind(hwloc_topology_t topology, hwloc_pid_t pid, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags)
{
  int ret;
  hwloc_cpuset_t cpuset;

  if ((policy != HWLOC_MEMBIND_DEFAULT && policy != HWLOC_MEMBIND_BIND)
      || flags & HWLOC_MEMBIND_NOCPUBIND) {
    errno = ENOSYS;
    return -1;
  }

  cpuset = hwloc_bitmap_alloc();
  hwloc_cpuset_from_nodeset(topology, cpuset, nodeset);
  ret = hwloc_win_set_proc_cpubind(topology, pid, cpuset, flags & HWLOC_MEMBIND_STRICT?HWLOC_CPUBIND_STRICT:0);
  hwloc_bitmap_free(cpuset);
  return ret;
}

static int
hwloc_win_get_proc_cpubind(hwloc_topology_t topology __hwloc_attribute_unused, hwloc_pid_t proc, hwloc_bitmap_t hwloc_set, int flags)
{
  DWORD_PTR proc_mask, sys_mask;
  if (flags & HWLOC_CPUBIND_NOMEMBIND) {
    errno = ENOSYS;
    return -1;
  }
  /* TODO: groups, GetProcessGroupAffinity, or merge SetThreadGroupAffinity for all threads */
  if (!GetProcessAffinityMask(proc, &proc_mask, &sys_mask))
    return -1;
  hwloc_bitmap_from_ulong(hwloc_set, proc_mask);
  return 0;
}

static int
hwloc_win_get_proc_membind(hwloc_topology_t topology, hwloc_pid_t pid, hwloc_nodeset_t nodeset, hwloc_membind_policy_t * policy, int flags)
{
  int ret;
  hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
  ret = hwloc_win_get_proc_cpubind(topology, pid, cpuset, flags & HWLOC_MEMBIND_STRICT?HWLOC_CPUBIND_STRICT:0);
  if (!ret) {
    *policy = HWLOC_MEMBIND_BIND;
    hwloc_cpuset_to_nodeset(topology, cpuset, nodeset);
  }
  hwloc_bitmap_free(cpuset);
  return ret;
}

static int
hwloc_win_set_thisproc_cpubind(hwloc_topology_t topology, hwloc_const_bitmap_t hwloc_set, int flags)
{
  return hwloc_win_set_proc_cpubind(topology, GetCurrentProcess(), hwloc_set, flags);
}

static int
hwloc_win_set_thisproc_membind(hwloc_topology_t topology, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags)
{
  return hwloc_win_set_proc_membind(topology, GetCurrentProcess(), nodeset, policy, flags);
}

static int
hwloc_win_get_thisproc_cpubind(hwloc_topology_t topology, hwloc_bitmap_t hwloc_cpuset, int flags)
{
  return hwloc_win_get_proc_cpubind(topology, GetCurrentProcess(), hwloc_cpuset, flags);
}

static int
hwloc_win_get_thisproc_membind(hwloc_topology_t topology, hwloc_nodeset_t nodeset, hwloc_membind_policy_t * policy, int flags)
{
  return hwloc_win_get_proc_membind(topology, GetCurrentProcess(), nodeset, policy, flags);
}

static LPVOID WINAPI (*VirtualAllocExNumaProc)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect, DWORD nndPreferred);
static BOOL WINAPI (*VirtualFreeExProc)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
static BOOL WINAPI (*QueryWorkingSetExProc)(HANDLE hProcess, PVOID pv, DWORD cb);

static int hwloc_win_get_VirtualAllocExNumaProc(void) {
  if (VirtualAllocExNumaProc == NULL) {
    FARPROC alloc_fun = NULL, free_fun = NULL;
    HMODULE kernel32;

    kernel32 = LoadLibrary("kernel32.dll");
    if (kernel32) {
      alloc_fun = GetProcAddress(kernel32, "VirtualAllocExNuma");
      free_fun = GetProcAddress(kernel32, "VirtualFreeEx");
    }

    if (!alloc_fun || !free_fun) {
      VirtualAllocExNumaProc = (FARPROC) -1;
      errno = ENOSYS;
      return -1;
    }

    VirtualAllocExNumaProc = alloc_fun;
    VirtualFreeExProc = free_fun;
  } else if ((FARPROC) VirtualAllocExNumaProc == (FARPROC)-1) {
    errno = ENOSYS;
    return -1;
  }

  return 0;
}

static void *
hwloc_win_alloc(hwloc_topology_t topology __hwloc_attribute_unused, size_t len) {
  return VirtualAlloc(NULL, len, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

static void *
hwloc_win_alloc_membind(hwloc_topology_t topology __hwloc_attribute_unused, size_t len, hwloc_const_nodeset_t nodeset, hwloc_membind_policy_t policy, int flags) {
  int node;

  switch (policy) {
    case HWLOC_MEMBIND_DEFAULT:
    case HWLOC_MEMBIND_BIND:
      break;
    default:
      errno = ENOSYS;
      return hwloc_alloc_or_fail(topology, len, flags);
  }

  if (flags & HWLOC_MEMBIND_STRICT) {
    errno = ENOSYS;
    return NULL;
  }

  if (hwloc_bitmap_weight(nodeset) != 1) {
    /* Not a single node, can't do this */
    errno = EXDEV;
    return hwloc_alloc_or_fail(topology, len, flags);
  }

  node = hwloc_bitmap_first(nodeset);
  return VirtualAllocExNumaProc(GetCurrentProcess(), NULL, len, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE, node);
}

static int
hwloc_win_free_membind(hwloc_topology_t topology __hwloc_attribute_unused, void *addr, size_t len __hwloc_attribute_unused) {
  if (!addr)
    return 0;
  if (!VirtualFreeExProc(GetCurrentProcess(), addr, 0, MEM_RELEASE))
    return -1;
  return 0;
}

static int hwloc_win_get_QueryWorkingSetExProc(void) {
  if (QueryWorkingSetExProc == NULL) {
    FARPROC fun = NULL;
    HMODULE kernel32, psapi;

    kernel32 = LoadLibrary("kernel32.dll");
    if (kernel32)
      fun = GetProcAddress(kernel32, "K32QueryWorkingSetEx");
    if (!fun) {
      psapi = LoadLibrary("psapi.dll");
      if (psapi)
        fun = GetProcAddress(psapi, "QueryWorkingSetEx");
    }

    if (!fun) {
      QueryWorkingSetExProc = (FARPROC) -1;
      errno = ENOSYS;
      return -1;
    }

    QueryWorkingSetExProc = fun;
  } else if ((FARPROC) QueryWorkingSetExProc == (FARPROC)-1) {
    errno = ENOSYS;
    return -1;
  }

  return 0;
}

static int
hwloc_win_get_area_membind(hwloc_topology_t topology __hwloc_attribute_unused, const void *addr, size_t len, hwloc_nodeset_t nodeset, hwloc_membind_policy_t * policy, int flags)
{
  SYSTEM_INFO SystemInfo;
  DWORD page_size;

  GetSystemInfo(&SystemInfo);
  page_size = SystemInfo.dwPageSize;

  uintptr_t start = (((uintptr_t) addr) / page_size) * page_size;
  unsigned nb = (((uintptr_t) addr + len - start) + page_size - 1) / page_size;

  if (!nb)
    nb = 1;

  {
    PSAPI_WORKING_SET_EX_INFORMATION pv[nb];
    unsigned i;

    for (i = 0; i < nb; i++)
      pv[i].VirtualAddress = (void*) (start + i * page_size);
    if (!QueryWorkingSetExProc(GetCurrentProcess(), &pv, sizeof(pv)))
      return -1;
    *policy = HWLOC_MEMBIND_BIND;
    if (flags & HWLOC_MEMBIND_STRICT) {
      unsigned node = pv[0].VirtualAttributes.Node;
      for (i = 1; i < nb; i++) {
	if (pv[i].VirtualAttributes.Node != node) {
	  errno = EXDEV;
	  return -1;
	}
      }
      hwloc_bitmap_only(nodeset, node);
      return 0;
    }
    hwloc_bitmap_zero(nodeset);
    for (i = 0; i < nb; i++)
      hwloc_bitmap_set(nodeset, pv[i].VirtualAttributes.Node);
    return 0;
  }
}

void
hwloc_look_windows(struct hwloc_topology *topology)
{
  BOOL WINAPI (*GetLogicalProcessorInformationProc)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer, PDWORD ReturnLength);
  BOOL WINAPI (*GetLogicalProcessorInformationExProc)(LOGICAL_PROCESSOR_RELATIONSHIP relationship, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Buffer, PDWORD ReturnLength);
  BOOL WINAPI (*GetNumaAvailableMemoryNodeProc)(UCHAR Node, PULONGLONG AvailableBytes);
  BOOL WINAPI (*GetNumaAvailableMemoryNodeExProc)(USHORT Node, PULONGLONG AvailableBytes);
  SYSTEM_INFO SystemInfo;

  DWORD length;

  HMODULE kernel32;

  GetSystemInfo(&SystemInfo);

  kernel32 = LoadLibrary("kernel32.dll");
  if (kernel32) {
    GetLogicalProcessorInformationProc = GetProcAddress(kernel32, "GetLogicalProcessorInformation");
    GetNumaAvailableMemoryNodeProc = GetProcAddress(kernel32, "GetNumaAvailableMemoryNode");
    GetNumaAvailableMemoryNodeExProc = GetProcAddress(kernel32, "GetNumaAvailableMemoryNodeEx");
    GetLogicalProcessorInformationExProc = GetProcAddress(kernel32, "GetLogicalProcessorInformationEx");

    if (!GetLogicalProcessorInformationExProc && GetLogicalProcessorInformationProc) {
      PSYSTEM_LOGICAL_PROCESSOR_INFORMATION procInfo;
      unsigned id;
      unsigned i;
      struct hwloc_obj *obj;
      hwloc_obj_type_t type;

      length = 0;
      procInfo = NULL;

      while (1) {
	if (GetLogicalProcessorInformationProc(procInfo, &length))
	  break;
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	  return;
	procInfo = realloc(procInfo, length);
      }

      for (i = 0; i < length / sizeof(*procInfo); i++) {

        /* Ignore non-data caches */
	if (procInfo[i].Relationship == RelationCache
		&& procInfo[i].Cache.Type != CacheUnified
		&& procInfo[i].Cache.Type != CacheData)
	  continue;

	id = -1;
	switch (procInfo[i].Relationship) {
	  case RelationNumaNode:
	    type = HWLOC_OBJ_NODE;
	    id = procInfo[i].NumaNode.NodeNumber;
	    break;
	  case RelationProcessorPackage:
	    type = HWLOC_OBJ_SOCKET;
	    break;
	  case RelationCache:
	    type = HWLOC_OBJ_CACHE;
	    break;
	  case RelationProcessorCore:
	    type = HWLOC_OBJ_CORE;
	    break;
	  case RelationGroup:
	  default:
	    type = HWLOC_OBJ_GROUP;
	    break;
	}

	obj = hwloc_alloc_setup_object(type, id);
        obj->cpuset = hwloc_bitmap_alloc();
	hwloc_debug("%s#%u mask %lx\n", hwloc_obj_type_string(type), id, procInfo[i].ProcessorMask);
	hwloc_bitmap_from_ulong(obj->cpuset, procInfo[i].ProcessorMask);

	switch (type) {
	  case HWLOC_OBJ_NODE:
	    {
	      ULONGLONG avail;
	      obj->nodeset = hwloc_bitmap_alloc();
	      hwloc_bitmap_set(obj->nodeset, id);
	      if ((GetNumaAvailableMemoryNodeExProc && GetNumaAvailableMemoryNodeExProc(id, &avail))
	       || (GetNumaAvailableMemoryNodeProc && GetNumaAvailableMemoryNodeProc(id, &avail)))
		obj->memory.local_memory = avail;
	      obj->memory.page_types_len = 2;
	      obj->memory.page_types = malloc(2 * sizeof(*obj->memory.page_types));
	      memset(obj->memory.page_types, 0, 2 * sizeof(*obj->memory.page_types));
	      obj->memory.page_types_len = 1;
	      obj->memory.page_types[0].size = SystemInfo.dwPageSize;
#ifdef HAVE__SC_LARGE_PAGESIZE
	      obj->memory.page_types_len++;
	      obj->memory.page_types[1].size = sysconf(_SC_LARGE_PAGESIZE);
#endif
	      break;
	    }
	  case HWLOC_OBJ_CACHE:
	    obj->attr->cache.size = procInfo[i].Cache.Size;
	    obj->attr->cache.linesize = procInfo[i].Cache.LineSize;
	    obj->attr->cache.depth = procInfo[i].Cache.Level;
	    break;
	  case HWLOC_OBJ_GROUP:
	    obj->attr->group.depth = procInfo[i].Relationship == RelationGroup;
	    break;
	  default:
	    break;
	}
	hwloc_insert_object_by_cpuset(topology, obj);
      }

      free(procInfo);
    }

    if (GetLogicalProcessorInformationExProc) {
      PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX procInfoTotal, procInfo;

      unsigned id;
      struct hwloc_obj *obj;
      hwloc_obj_type_t type;

      length = 0;
      procInfoTotal = NULL;

      while (1) {
	if (GetLogicalProcessorInformationExProc(RelationAll, procInfoTotal, &length))
	  break;
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	  return;
        procInfoTotal = realloc(procInfoTotal, length);
      }

      for (procInfo = procInfoTotal;
	   (void*) procInfo < (void*) ((unsigned long) procInfoTotal + length);
	   procInfo = (void*) ((unsigned long) procInfo + procInfo->Size)) {
        unsigned num, i;
        GROUP_AFFINITY *GroupMask;

        /* Ignore non-data caches */
	if (procInfo->Relationship == RelationCache
		&& procInfo->Cache.Type != CacheUnified
		&& procInfo->Cache.Type != CacheData)
	  continue;

	id = -1;
	switch (procInfo->Relationship) {
	  case RelationNumaNode:
	    type = HWLOC_OBJ_NODE;
            num = 1;
            GroupMask = &procInfo->NumaNode.GroupMask;
	    id = procInfo->NumaNode.NodeNumber;
	    break;
	  case RelationProcessorPackage:
	    type = HWLOC_OBJ_SOCKET;
            num = procInfo->Processor.GroupCount;
            GroupMask = procInfo->Processor.GroupMask;
	    break;
	  case RelationCache:
	    type = HWLOC_OBJ_CACHE;
            num = 1;
            GroupMask = &procInfo->Cache.GroupMask;
	    break;
	  case RelationProcessorCore:
	    type = HWLOC_OBJ_CORE;
            num = procInfo->Processor.GroupCount;
            GroupMask = procInfo->Processor.GroupMask;
	    break;
	  case RelationGroup:
	    /* So strange an interface... */
	    for (id = 0; id < procInfo->Group.ActiveGroupCount; id++) {
              KAFFINITY mask;
	      obj = hwloc_alloc_setup_object(HWLOC_OBJ_GROUP, id);
	      obj->cpuset = hwloc_bitmap_alloc();
	      mask = procInfo->Group.GroupInfo[id].ActiveProcessorMask;
	      hwloc_debug("group %u %d cpus mask %lx\n", id,
                  procInfo->Group.GroupInfo[id].ActiveProcessorCount, mask);
	      hwloc_bitmap_from_ith_ulong(obj->cpuset, id, mask);
	      hwloc_insert_object_by_cpuset(topology, obj);
	    }
	    continue;
	  default:
	    /* Don't know how to get the mask.  */
            hwloc_debug("unknown relation %d\n", procInfo->Relationship);
	    continue;
	}

	obj = hwloc_alloc_setup_object(type, id);
        obj->cpuset = hwloc_bitmap_alloc();
        for (i = 0; i < num; i++) {
          hwloc_debug("%s#%u %d: mask %d:%lx\n", hwloc_obj_type_string(type), id, i, GroupMask[i].Group, GroupMask[i].Mask);
          hwloc_bitmap_from_ith_ulong(obj->cpuset, GroupMask[i].Group, GroupMask[i].Mask);
        }

	switch (type) {
	  case HWLOC_OBJ_NODE:
	    {
	      ULONGLONG avail;
	      obj->nodeset = hwloc_bitmap_alloc();
	      hwloc_bitmap_set(obj->nodeset, id);
	      if ((GetNumaAvailableMemoryNodeExProc && GetNumaAvailableMemoryNodeExProc(id, &avail))
	       || (GetNumaAvailableMemoryNodeProc && GetNumaAvailableMemoryNodeProc(id, &avail)))
	        obj->memory.local_memory = avail;
	      obj->memory.page_types = malloc(2 * sizeof(*obj->memory.page_types));
	      memset(obj->memory.page_types, 0, 2 * sizeof(*obj->memory.page_types));
	      obj->memory.page_types_len = 1;
	      obj->memory.page_types[0].size = SystemInfo.dwPageSize;
#ifdef HAVE__SC_LARGE_PAGESIZE
	      obj->memory.page_types_len++;
	      obj->memory.page_types[1].size = sysconf(_SC_LARGE_PAGESIZE);
#endif
	      break;
	    }
	  case HWLOC_OBJ_CACHE:
	    obj->attr->cache.size = procInfo->Cache.CacheSize;
	    obj->attr->cache.linesize = procInfo->Cache.LineSize;
	    obj->attr->cache.depth = procInfo->Cache.Level;
	    break;
	  default:
	    break;
	}
	hwloc_insert_object_by_cpuset(topology, obj);
      }
      free(procInfoTotal);
    }
  }

  /* add PU objects */
  hwloc_setup_pu_level(topology, hwloc_fallback_nbprocessors(topology));

  hwloc_add_object_info(topology->levels[0][0], "Backend", "Windows");
}

void
hwloc_set_windows_hooks(struct hwloc_topology *topology)
{
  topology->set_proc_cpubind = hwloc_win_set_proc_cpubind;
  topology->get_proc_cpubind = hwloc_win_get_proc_cpubind;
  topology->set_thread_cpubind = hwloc_win_set_thread_cpubind;
  topology->set_thisproc_cpubind = hwloc_win_set_thisproc_cpubind;
  topology->get_thisproc_cpubind = hwloc_win_get_thisproc_cpubind;
  topology->set_thisthread_cpubind = hwloc_win_set_thisthread_cpubind;
  /* TODO: get_last_cpu_location: use GetCurrentProcessorNumber */

  topology->set_proc_membind = hwloc_win_set_proc_membind;
  topology->get_proc_membind = hwloc_win_get_proc_membind;
  topology->set_thisproc_membind = hwloc_win_set_thisproc_membind;
  topology->get_thisproc_membind = hwloc_win_get_thisproc_membind;
  topology->set_thisthread_membind = hwloc_win_set_thisthread_membind;

  if (!hwloc_win_get_VirtualAllocExNumaProc()) {
    topology->alloc_membind = hwloc_win_alloc_membind;
    topology->alloc = hwloc_win_alloc;
    topology->free_membind = hwloc_win_free_membind;
    topology->support.membind->bind_membind = 1;
  }

  if (!hwloc_win_get_QueryWorkingSetExProc())
    topology->get_area_membind = hwloc_win_get_area_membind;
}
