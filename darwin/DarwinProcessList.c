/*
htop - DarwinProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "DarwinProcess.h"
#include "DarwinProcessList.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <libproc.h>
#include <sys/mman.h>
#include <utmpx.h>

/*{
#include "ProcessList.h"
#include <mach/mach_host.h>
#include <sys/sysctl.h>

typedef struct DarwinProcessList_ {
   ProcessList super;

   host_basic_info_data_t host_info;
   vm_statistics64_data_t vm_stats;
   processor_cpu_load_info_t prev_load;
   processor_cpu_load_info_t curr_load;
   uint64_t kernel_threads;
   uint64_t user_threads;
   uint64_t global_diff;
} DarwinProcessList;

}*/

void ProcessList_getHostInfo(host_basic_info_data_t *p) {
   mach_msg_type_number_t info_size = HOST_BASIC_INFO_COUNT;

   if(0 != host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)p, &info_size)) {
       fprintf(stderr, "Unable to retrieve host info\n");
       exit(2);
   }
}

void ProcessList_freeCPULoadInfo(processor_cpu_load_info_t *p) {
   if(NULL != p && NULL != *p) {
       if(0 != munmap(*p, vm_page_size)) {
           fprintf(stderr, "Unable to free old CPU load information\n");
           exit(8);
       }
   }

   *p = NULL;
}

unsigned ProcessList_allocateCPULoadInfo(processor_cpu_load_info_t *p) {
   mach_msg_type_number_t info_size = sizeof(processor_cpu_load_info_t);
   unsigned cpu_count;

   // TODO Improving the accuracy of the load counts woule help a lot.
   if(0 != host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, (processor_info_array_t *)p, &info_size)) {
       fprintf(stderr, "Unable to retrieve CPU info\n");
       exit(4);
   }

   return cpu_count;
}

void ProcessList_getVMStats(vm_statistics64_t p) {
    mach_msg_type_number_t info_size = HOST_VM_INFO64_COUNT;

    if(0 != host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info_t)p, &info_size)) {
       fprintf(stderr, "Unable to retrieve VM statistics\n");
       exit(9);
    }
}

struct kinfo_proc *ProcessList_getKInfoProcs(size_t *count) {
   int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
   struct kinfo_proc *processes = NULL;

   /* Note the two calls to sysctl(). One to get length and one to get the
    * data. This -does- mean that the second call could end up with a missing
    * process entry or two.
    */
   *count = 0;
   if(0 > sysctl(mib, 4, NULL, count, NULL, 0)) {
      fprintf(stderr, "Unable to get size of kproc_infos");
      exit(5);
   }

   processes = (struct kinfo_proc *)malloc(*count);
   if(NULL == processes) {
      fprintf(stderr, "Out of memory for kproc_infos\n");
      exit(6);
   }

   if(0 > sysctl(mib, 4, processes, count, NULL, 0)) {
      fprintf(stderr, "Unable to get kinfo_procs\n");
      exit(7);
   }

   *count = *count / sizeof(struct kinfo_proc);

   return processes;
}


ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   DarwinProcessList* this = calloc(1, sizeof(DarwinProcessList));

   ProcessList_init(&this->super, Class(Process), usersTable, pidWhiteList, userId);

   /* Initialize the CPU information */
   this->super.cpuCount = ProcessList_allocateCPULoadInfo(&this->prev_load);
   ProcessList_getHostInfo(&this->host_info);
   ProcessList_allocateCPULoadInfo(&this->curr_load);

   /* Initialize the VM statistics */
   ProcessList_getVMStats(&this->vm_stats);

   this->super.kernelThreads = 0;
   this->super.userlandThreads = 0;
   this->super.totalTasks = 0;
   this->super.runningTasks = 0;

   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* super) {
    DarwinProcessList *dpl = (DarwinProcessList *)super;
	bool preExisting = true;
	struct kinfo_proc *ps;
	size_t count;
    DarwinProcess *proc;
    struct timeval tv;

    gettimeofday(&tv, NULL); /* Start processing time */

    /* Update the global data (CPU times and VM stats) */
    ProcessList_freeCPULoadInfo(&dpl->prev_load);
    dpl->prev_load = dpl->curr_load;
    ProcessList_allocateCPULoadInfo(&dpl->curr_load);
    ProcessList_getVMStats(&dpl->vm_stats);

    /* Get the time difference */
    dpl->global_diff = 0;
    for(int i = 0; i < dpl->super.cpuCount; ++i) {
        for(size_t j = 0; j < CPU_STATE_MAX; ++j) {
            dpl->global_diff += dpl->curr_load[i].cpu_ticks[j] - dpl->prev_load[i].cpu_ticks[j];
        }
    }

    /* Clear the thread counts */
    super->kernelThreads = 0;
    super->userlandThreads = 0;
    super->totalTasks = 0;
    super->runningTasks = 0;

    /* We use kinfo_procs for initial data since :
     *
     * 1) They always succeed.
     * 2) The contain the basic information.
     *
     * We attempt to fill-in additional information with libproc.
     */
    ps = ProcessList_getKInfoProcs(&count);

    for(size_t i = 0; i < count; ++i) {
       proc = (DarwinProcess *)ProcessList_getProcess(super, ps[i].kp_proc.p_pid, &preExisting, (Process_New)DarwinProcess_new);

       DarwinProcess_setFromKInfoProc(&proc->super, ps + i, tv.tv_sec, preExisting);
       DarwinProcess_setFromLibprocPidinfo(proc, dpl);

       super->totalTasks += 1;

       if(!preExisting) {
           proc->super.user = UsersTable_getRef(super->usersTable, proc->super.st_uid);

           ProcessList_add(super, &proc->super);
       }
    }

    free(ps);
}
