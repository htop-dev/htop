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

/*{
#include <mach/mach_host.h>
#include <sys/sysctl.h>

typedef struct DarwinProcessList_ {
   ProcessList super;

   host_basic_info_data_t prev_hinfo;
   processor_info_data_t prev_cpus;
} DarwinProcessList;

}*/

void ProcessList_getHostInfo(host_basic_info_data_t *p) {
   mach_msg_type_number_t info_size = HOST_BASIC_INFO_COUNT;

   if(0 != host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)p, &info_size)) {
       fprintf(stderr, "Unable to retrieve host info\n");
       exit(2);
   }
}

unsigned ProcessList_getCPUInfo(processor_info_data_t *p) {
   mach_msg_type_number_t info_size = PROCESSOR_CPU_LOAD_INFO_COUNT;
   unsigned cpu_count;

   if(0 != host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, (processor_info_array_t *)p, &info_size)) {
       fprintf(stderr, "Unable to retrieve CPU info\n");
       exit(4);
   }

   return cpu_count;
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
   
   /* Initialize the previous information */
   this->super.cpuCount = ProcessList_getCPUInfo(&this->prev_cpus);
   ProcessList_getHostInfo(&this->prev_hinfo);

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
    Process *proc;
    struct timeval tv;

    gettimeofday(&tv, NULL); /* Start processing time */

    /* We use kinfo_procs for initial data since :
     *
     * 1) They always succeed.
     * 2) The contain the basic information.
     *
     * We attempt to fill-in additional information with libproc.
     */
    ps = ProcessList_getKInfoProcs(&count);

    for(size_t i = 0; i < count; ++i) {
       proc = ProcessList_getProcess(super, ps[i].kp_proc.p_pid, &preExisting, DarwinProcess_new);

       DarwinProcess_setFromKInfoProc(proc, ps + i, tv.tv_sec, preExisting);
       DarwinProcess_setFromLibprocPidinfo(proc, dpl->prev_hinfo.max_mem, preExisting);

       if(!preExisting) {
           proc->user = UsersTable_getRef(super->usersTable, proc->st_uid);

           ProcessList_add(super, proc);
       }
    }

    free(ps);
}
