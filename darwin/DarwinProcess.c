/*
htop - DarwinProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "DarwinProcess.h"

#include <stdlib.h>
#include <libproc.h>
#include <string.h>
#include <stdio.h>

/*{
#include "Settings.h"
#include "DarwinProcessList.h"

#include <sys/sysctl.h>

typedef struct DarwinProcess_ {
   Process super;

   uint64_t utime;
   uint64_t stime;
} DarwinProcess;

}*/

ProcessClass DarwinProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = Process_writeField,
};

DarwinProcess* DarwinProcess_new(Settings* settings) {
   DarwinProcess* this = xCalloc(1, sizeof(DarwinProcess));
   Object_setClass(this, Class(DarwinProcess));
   Process_init(&this->super, settings);

   this->utime = 0;
   this->stime = 0;

   return this;
}

void Process_delete(Object* cast) {
   DarwinProcess* this = (DarwinProcess*) cast;
   Process_done(&this->super);
   // free platform-specific fields here
   free(this);
}

bool Process_isThread(Process* this) {
   (void) this;
   return false;
}

void DarwinProcess_setStartTime(Process *proc, struct extern_proc *ep, time_t now) {
   struct tm date;

   proc->starttime_ctime = ep->p_starttime.tv_sec;
   (void) localtime_r(&proc->starttime_ctime, &date);
   strftime(proc->starttime_show, 7, ((proc->starttime_ctime > now - 86400) ? "%R " : "%b%d "), &date);
}

char *DarwinProcess_getCmdLine(struct kinfo_proc* k, int show_args ) {
   /* This function is from the old Mac version of htop. Originally from ps? */
   int mib[3], argmax, nargs, c = 0;
   size_t size;
   char *procargs, *sp, *np, *cp, *retval;

   /* Get the maximum process arguments size. */
   mib[0] = CTL_KERN;
   mib[1] = KERN_ARGMAX;

   size = sizeof( argmax );
   if ( sysctl( mib, 2, &argmax, &size, NULL, 0 ) == -1 ) {
      goto ERROR_A;
   }

   /* Allocate space for the arguments. */
   procargs = ( char * ) xMalloc( argmax );
   if ( procargs == NULL ) {
      goto ERROR_A;
   }

   /*
    * Make a sysctl() call to get the raw argument space of the process.
    * The layout is documented in start.s, which is part of the Csu
    * project.  In summary, it looks like:
    *
    * /---------------\ 0x00000000
    * :               :
    * :               :
    * |---------------|
    * | argc          |
    * |---------------|
    * | arg[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | arg[argc - 1] |
    * |---------------|
    * | 0             |
    * |---------------|
    * | env[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | env[n]        |
    * |---------------|
    * | 0             |
    * |---------------| <-- Beginning of data returned by sysctl() is here.
    * | argc          |
    * |---------------|
    * | exec_path     |
    * |:::::::::::::::|
    * |               |
    * | String area.  |
    * |               |
    * |---------------| <-- Top of stack.
    * :               :
    * :               :
    * \---------------/ 0xffffffff
    */
   mib[0] = CTL_KERN;
   mib[1] = KERN_PROCARGS2;
   mib[2] = k->kp_proc.p_pid;

   size = ( size_t ) argmax;
   if ( sysctl( mib, 3, procargs, &size, NULL, 0 ) == -1 ) {
      goto ERROR_B;
   }

   memcpy( &nargs, procargs, sizeof( nargs ) );
   cp = procargs + sizeof( nargs );

   /* Skip the saved exec_path. */
   for ( ; cp < &procargs[size]; cp++ ) {
      if ( *cp == '\0' ) {
         /* End of exec_path reached. */
         break;
      }
   }
   if ( cp == &procargs[size] ) {
      goto ERROR_B;
   }

   /* Skip trailing '\0' characters. */
   for ( ; cp < &procargs[size]; cp++ ) {
      if ( *cp != '\0' ) {
         /* Beginning of first argument reached. */
         break;
      }
   }
   if ( cp == &procargs[size] ) {
      goto ERROR_B;
   }
   /* Save where the argv[0] string starts. */
   sp = cp;

   /*
    * Iterate through the '\0'-terminated strings and convert '\0' to ' '
    * until a string is found that has a '=' character in it (or there are
    * no more strings in procargs).  There is no way to deterministically
    * know where the command arguments end and the environment strings
    * start, which is why the '=' character is searched for as a heuristic.
    */
   for ( np = NULL; c < nargs && cp < &procargs[size]; cp++ ) {
      if ( *cp == '\0' ) {
         c++;
         if ( np != NULL ) {
            /* Convert previous '\0'. */
            *np = ' ';
         }
        /* Note location of current '\0'. */
        np = cp;

        if ( !show_args ) {
           /*
            * Don't convert '\0' characters to ' '.
            * However, we needed to know that the
            * command name was terminated, which we
            * now know.
            */
           break;
        }
     }
  }
#if 0
   /*
    * If eflg is non-zero, continue converting '\0' characters to ' '
    * characters until no more strings that look like environment settings
    * follow.
    */
   if ( ( eflg != 0 )
         && ( ( getuid(  ) == 0 )
              || ( k->kp_eproc.e_pcred.p_ruid == getuid(  ) ) ) ) {
      for ( ; cp < &procargs[size]; cp++ ) {
         if ( *cp == '\0' ) {
            if ( np != NULL ) {
               if ( &np[1] == cp ) {
                  /*
                   * Two '\0' characters in a row.
                   * This should normally only
                   * happen after all the strings
                   * have been seen, but in any
                   * case, stop parsing.
                   */
                  break;
               }
               /* Convert previous '\0'. */
               *np = ' ';
            }
            /* Note location of current '\0'. */
            np = cp;
         }
      }
   }
#endif

   /*
    * sp points to the beginning of the arguments/environment string, and
    * np should point to the '\0' terminator for the string.
    */
   if ( np == NULL || np == sp ) {
      /* Empty or unterminated string. */
      goto ERROR_B;
   }

   /* Make a copy of the string. */
   retval = xStrdup(sp);

   /* Clean up. */
   free( procargs );

   return retval;

ERROR_B:
   free( procargs );
ERROR_A:
   retval = xStrdup(k->kp_proc.p_comm);

   return retval;
}

void DarwinProcess_setFromKInfoProc(Process *proc, struct kinfo_proc *ps, time_t now, bool exists) {
   struct extern_proc *ep = &ps->kp_proc;

   /* UNSET HERE :
    *
    * processor
    * user (set at ProcessList level)
    * nlwp
    * percent_cpu
    * percent_mem
    * m_size
    * m_resident
    * minflt
    * majflt
    */

   /* First, the "immutable" parts */
   if(!exists) {
      /* Set the PID/PGID/etc. */
      proc->pid = ep->p_pid;
      proc->ppid = ps->kp_eproc.e_ppid;
      proc->pgrp = ps->kp_eproc.e_pgid;
      proc->session = 0; /* TODO Get the session id */
      proc->tgid = ps->kp_eproc.e_tpgid;
      proc->st_uid = ps->kp_eproc.e_ucred.cr_uid;
      /* e_tdev = (major << 24) | (minor & 0xffffff) */
      /* e_tdev == -1 for "no device" */
      proc->tty_nr = ps->kp_eproc.e_tdev & 0xff; /* TODO tty_nr is unsigned */

      DarwinProcess_setStartTime(proc, ep, now);

      /* The command is from the old Mac htop */
      char *slash;

      proc->comm = DarwinProcess_getCmdLine(ps, false);
      slash = strrchr(proc->comm, '/');
      proc->basenameOffset = (NULL != slash) ? (slash - proc->comm) : 0;
   }

   /* Mutable information */
   proc->nice = ep->p_nice;
   proc->priority = ep->p_priority;

   /* Set the state */
   switch(ep->p_stat) {
   case SIDL:   proc->state = 'I'; break;
   case SRUN:   proc->state = 'R'; break;
   case SSLEEP: proc->state = 'S'; break;
   case SSTOP:  proc->state = 'T'; break;
   case SZOMB:  proc->state = 'Z'; break;
   default:     proc->state = '?'; break;
   }

   /* Make sure the updated flag is set */
   proc->updated = true;
}

void DarwinProcess_setFromLibprocPidinfo(DarwinProcess *proc, DarwinProcessList *dpl) {
   struct proc_taskinfo pti;

   if(sizeof(pti) == proc_pidinfo(proc->super.pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti))) {
      if(0 != proc->utime || 0 != proc->stime) {
         uint64_t diff = (pti.pti_total_system - proc->stime)
                  + (pti.pti_total_user - proc->utime);

         proc->super.percent_cpu = (double)diff * (double)dpl->super.cpuCount
                  / ((double)dpl->global_diff * 100000.0);

//       fprintf(stderr, "%f %llu %llu %llu %llu %llu\n", proc->super.percent_cpu,
//               proc->stime, proc->utime, pti.pti_total_system, pti.pti_total_user, dpl->global_diff);
//       exit(7);
      }

      proc->super.time = (pti.pti_total_system + pti.pti_total_user) / 10000000;
      proc->super.nlwp = pti.pti_threadnum;
      proc->super.m_size = pti.pti_virtual_size / 1024 / PAGE_SIZE_KB;
      proc->super.m_resident = pti.pti_resident_size / 1024 / PAGE_SIZE_KB;
      proc->super.majflt = pti.pti_faults;
      proc->super.percent_mem = (double)pti.pti_resident_size * 100.0
              / (double)dpl->host_info.max_mem;

      proc->stime = pti.pti_total_system;
      proc->utime = pti.pti_total_user;

      dpl->super.kernelThreads += 0; /*pti.pti_threads_system;*/
      dpl->super.userlandThreads += pti.pti_threadnum; /*pti.pti_threads_user;*/
      dpl->super.totalTasks += pti.pti_threadnum;
      dpl->super.runningTasks += pti.pti_numrunning;
   }
}
