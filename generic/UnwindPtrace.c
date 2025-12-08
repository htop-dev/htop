/*
htop - generic/UnwindPtrace.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "generic/UnwindPtrace.h"

#include <errno.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "BacktraceScreen.h"
#include "XUtils.h"
#include "generic/Demangle.h"

#ifdef HAVE_LIBUNWIND_PTRACE
#include <libunwind-ptrace.h>
#include <libunwind.h>
#endif


#ifdef HAVE_LIBUNWIND_PTRACE
static int ptraceAttach(pid_t pid) {
   errno = 0;
# if defined(HTOP_LINUX)
   return !ptrace(PTRACE_ATTACH, pid, (void*)0, (void*)0) ? 0 : errno;
# elif defined(HTOP_FREEBSD) || defined(HTOP_NETBSD) || defined(HTOP_OPENBSD) || defined(HTOP_DARWIN)
   return !ptrace(PT_ATTACH, pid, (caddr_t)0, 0) ? 0 : errno;
# endif
   (void)pid;
   return ENOSYS;
}

static int ptraceDetach(pid_t pid) {
   errno = 0;
# if defined(HTOP_LINUX)
   return !ptrace(PTRACE_DETACH, pid, (void*)0, (void*)0) ? 0 : errno;
# elif defined(HTOP_FREEBSD) || defined(HTOP_NETBSD) || defined(HTOP_OPENBSD) || defined(HTOP_DARWIN)
   return !ptrace(PT_DETACH, pid, (caddr_t)0, 0) ? 0 : errno;
# endif
   (void)pid;
   return ENOSYS;
}

void UnwindPtrace_makeBacktrace(Vector* frames, pid_t pid, char** error) {
   *error = NULL;

   unw_addr_space_t addrSpace = unw_create_addr_space(&_UPT_accessors, 0);
   if (!addrSpace) {
      xAsprintf(error, "Unable to initialize libunwind.");
      return;
   }

   if (pid <= 0) {
      xAsprintf(error, "Unable to get the pid");
      goto addr_space_error;
   }

   int ptraceErrno = ptraceAttach(pid);
   if (ptraceErrno) {
      xAsprintf(error, "ptrace: %s (%d)", strerror(ptraceErrno), ptraceErrno);
      goto addr_space_error;
   }

   int waitStatus = 0;
   if (wait(&waitStatus) == -1) {
      int waitErrno = errno;
      xAsprintf(error, "wait: %s (%d)", strerror(waitErrno), waitErrno);
      goto ptrace_error;
   }

   if (WIFSTOPPED(waitStatus) == 0) {
      *error = xStrdup("The process chosen is not stopped correctly.");
      goto ptrace_error;
   }

   struct UPT_info* context = _UPT_create(pid);
   if (!context) {
      xAsprintf(error, "Unable to create the context of libunwind-ptrace");
      goto ptrace_error;
   }

   unw_cursor_t cursor;
   int ret = unw_init_remote(&cursor, addrSpace, context);
   if (ret < 0) {
      xAsprintf(error, "libunwind cursor: ret=%d", ret);
      goto context_error;
   }

   int index = 0;
   do {
      char procName[2048] = "?";
      unw_word_t offset;
      unw_word_t pc;

      BacktraceFrameData* frame = BacktraceFrameData_new();
      frame->index = index;
      if (unw_get_proc_name(&cursor, procName, sizeof(procName), &offset) == 0) {
         ret = unw_get_reg(&cursor, UNW_REG_IP, &pc);
         if (ret < 0) {
            xAsprintf(error, "unable to get program counter register: %d", ret);
            BacktraceFrameData_delete((Object *)frame);
            break;
         }

         frame->address = pc;
         frame->offset = offset;
         frame->isSignalFrame = unw_is_signal_frame(&cursor);

         frame->functionName = xStrndup(procName, 2048);

# if defined(HAVE_DEMANGLING)
         char* demangledName = Generic_Demangle(frame->functionName);
         frame->demangleFunctionName = demangledName;
# endif

# if defined(HAVE_LIBUNWIND_ELF_FILENAME)
         unw_word_t offsetElfFileName;
         char elfFileName[2048] = { 0 };
         if (unw_get_elf_filename(&cursor, elfFileName, sizeof(elfFileName), &offsetElfFileName) == 0) {
            frame->objectPath = xStrndup(elfFileName, 2048);

            char *lastSlash = strrchr(frame->objectPath, '/');
            frame->objectName = xStrndup(lastSlash + 1, 2048);
         }
# endif

      } else {
         frame->functionName = xStrdup("???");
      }
      Vector_add(frames, (Object *)frame);
      index++;
   } while (unw_step(&cursor) > 0);

context_error:
   _UPT_destroy(context);

ptrace_error:
   ptraceDetach(pid);

addr_space_error:
   unw_destroy_addr_space(addrSpace);
}
#endif
