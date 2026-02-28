/*
htop - generic/UnwindPtrace.c
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "generic/UnwindPtrace.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "BacktraceScreen.h"
#include "XUtils.h"

#ifdef HAVE_LIBUNWIND_PTRACE
#include <libunwind-ptrace.h>
#include <libunwind.h>
#endif

#ifdef HAVE_DEMANGLING
#include "generic/Demangle.h"
#endif


#ifdef HAVE_LIBUNWIND_PTRACE
static int ptraceAttach(pid_t pid) {
# if defined(HTOP_LINUX)
   return !ptrace(PTRACE_ATTACH, pid, (void*)0, (void*)0) ? 0 : errno;
# elif defined(HTOP_FREEBSD) || defined(HTOP_NETBSD) || defined(HTOP_OPENBSD) || defined(HTOP_DARWIN)
   return !ptrace(PT_ATTACH, pid, (caddr_t)0, 0) ? 0 : errno;
# else
   (void)pid;
   return ENOSYS;
# endif
}

static int ptraceDetach(pid_t pid) {
# if defined(HTOP_LINUX)
   return !ptrace(PTRACE_DETACH, pid, (void*)0, (void*)0) ? 0 : errno;
# elif defined(HTOP_FREEBSD) || defined(HTOP_NETBSD) || defined(HTOP_OPENBSD) || defined(HTOP_DARWIN)
   return !ptrace(PT_DETACH, pid, (caddr_t)0, 0) ? 0 : errno;
# else
   (void)pid;
   return ENOSYS;
# endif
}

void UnwindPtrace_makeBacktrace(Vector* frames, pid_t pid, char** error) {
   *error = NULL;

   if (pid <= 0) {
      *error = xStrdup("Invalid PID");
      return;
   }

   unw_addr_space_t addrSpace = unw_create_addr_space(&_UPT_accessors, 0);
   if (!addrSpace) {
      *error = xStrdup("Cannot initialize libunwind");
      return;
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
      *error = xStrdup("The process chosen is not stopped correctly");
      goto ptrace_error;
   }

   struct UPT_info* context = _UPT_create(pid);
   if (!context) {
      *error = xStrdup("Cannot create the context of libunwind-ptrace");
      goto ptrace_error;
   }

   unw_cursor_t cursor;
   int ret = unw_init_remote(&cursor, addrSpace, context);
   if (ret < 0) {
      xAsprintf(error, "libunwind cursor: ret=%d", ret);
      goto context_error;
   }

   unsigned int index = 0;
   do {
      char buffer[2048] = {0};

      BacktraceFrameData* frame = BacktraceFrameData_new();
      frame->index = index;

      unw_word_t pc;
      ret = unw_get_reg(&cursor, UNW_REG_IP, &pc);
      if (ret != 0) {
         xAsprintf(error, "Cannot get program counter register: error %d", -ret);
         BacktraceFrameData_delete((Object *)frame);
         break;
      }
      frame->address = pc;

      frame->isSignalFrame = unw_is_signal_frame(&cursor) > 0;

# if defined(HAVE_LIBUNWIND_ELF_FILENAME)
      unw_word_t offsetElfFileName;
      if (unw_get_elf_filename(&cursor, buffer, sizeof(buffer), &offsetElfFileName) == 0)
         frame->objectPath = xStrndup(buffer, sizeof(buffer));
# endif

      unw_word_t offset;
      if (unw_get_proc_name(&cursor, buffer, sizeof(buffer), &offset) == 0) {
         frame->offset = offset;
         frame->functionName = xStrndup(buffer, sizeof(buffer));

# if defined(HAVE_DEMANGLING)
         char* demangledName = Demangle_demangle(frame->functionName);
         frame->demangleFunctionName = demangledName;
# endif
      }
      Vector_add(frames, (Object *)frame);
      index++;
   } while (unw_step(&cursor) > 0 && index < INT_MAX);

context_error:
   _UPT_destroy(context);

ptrace_error:
   ptraceDetach(pid);

addr_space_error:
   unw_destroy_addr_space(addrSpace);
}
#endif
