#ifndef HEADER_ProgramLauncher
#define HEADER_ProgramLauncher
/*
htop - ProgramLauncher.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Macros.h"


enum ProgramLauncherOptions_ {
   PROGRAM_LAUNCH_KEEP_SETUID = 1 << 0,
   PROGRAM_LAUNCH_NO_SCRIPT = 1 << 2,
};

typedef unsigned int ProgramLauncherOptions;

typedef struct ProgramLauncher_ {
   void* fileRef;
   int lastErrno;
   ProgramLauncherOptions options;
} ProgramLauncher;

// POSIX declares the type of "argv" and "envp" arguments in execve()
// as "char *const[]" for compability reason (see the "Ratinale" of
// <https://pubs.opengroup.org/onlinepubs/9799919799/functions/exec.html#tag_17_129_08>).
// Unfortunately that prevents the function from accepting the
// "const char *const[]" type, which is arguably more const-correct.
// An explicit cast from "char **" to "const char *const *" will
// generate a "-Wcast-qual" warning. Use this union as a workaround.
typedef union ExecStrPtrPtr_ {
   const char* const* cpp;
   char* const* pp;
} ExecStrPtrPtr;

ATTR_NONNULL
void ProgramLauncher_done(ProgramLauncher* this);

ATTR_NONNULL
void ProgramLauncher_setPath(ProgramLauncher* this, const char* path);

ATTR_NONNULL_N(1, 2)
void ProgramLauncher_execve(ProgramLauncher* this, char* const* argv, char* const* envp);

ATTR_NONNULL_N(1, 2)
static inline void ProgramLauncher_execve_const(ProgramLauncher* this, const char* const* argv, const char* const* envp) {
   ExecStrPtrPtr argv_cast, envp_cast;
   argv_cast.cpp = argv;
   envp_cast.cpp = envp;
   ProgramLauncher_execve(this, argv_cast.pp, envp_cast.pp);
}

ATTR_NONNULL
static inline void ProgramLauncher_execv(ProgramLauncher* this, char* const* argv) {
   ProgramLauncher_execve(this, argv, NULL);
}

ATTR_NONNULL
static inline void ProgramLauncher_execv_const(ProgramLauncher* this, const char* const* argv) {
   ExecStrPtrPtr argv_cast;
   argv_cast.cpp = argv;
   ProgramLauncher_execv(this, argv_cast.pp);
}

#endif
