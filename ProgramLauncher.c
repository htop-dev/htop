/*
htop - ProgramLauncher.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ProgramLauncher.h"

#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "XUtils.h"


typedef struct ProgramFileRef_ {
   dev_t dev;
   ino_t ino;
   char* path;
} ProgramFileRef;

extern char** environ;

static gid_t ProgramLauncher_savedSetGid = (gid_t)-1;
static uid_t ProgramLauncher_savedSetUid = (uid_t)-1;

void ProgramLauncher_done(ProgramLauncher* this) {
   if (this->fileRef) {
      free(((const ProgramFileRef*)this->fileRef)->path);
   }
   free(this->fileRef);
   this->fileRef = NULL;
   this->lastErrno = 0;
}

static void ProgramLauncher_dropSetUid(void) {
   gid_t egid = getegid();
   if (ProgramLauncher_savedSetGid == (gid_t)-1)
      ProgramLauncher_savedSetGid = egid;

   assert(ProgramLauncher_savedSetGid == egid);

   uid_t euid = geteuid();
   if (ProgramLauncher_savedSetUid == (uid_t)-1)
      ProgramLauncher_savedSetUid = euid;

   assert(ProgramLauncher_savedSetUid == euid);

   int res = 0;

   gid_t rgid = getgid();
#ifdef _POSIX_SAVED_IDS
   if (egid != rgid)
      res = setegid(rgid);
#else
   // See Glibc manual about the real UID and effective UID swap:
   // https://sourceware.org/glibc/manual/latest/html_node/Enable_002fDisable-Setuid.html
   if (ProgramLauncher_savedSetGid != rgid)
      res = setregid(egid, rgid);
#endif
   if (res != 0)
      fail();

   uid_t ruid = getuid();
#ifdef _POSIX_SAVED_IDS
   if (euid != ruid)
      res = seteuid(ruid);
#else
   if (ProgramLauncher_savedSetUid != ruid)
      res = setreuid(euid, ruid);
#endif
   if (res != 0)
      fail();
}

static void ProgramLauncher_restoreSetUid(void) {
   assert(ProgramLauncher_savedSetGid != (gid_t)-1);
   assert(ProgramLauncher_savedSetUid != (uid_t)-1);
   if (ProgramLauncher_savedSetGid == (gid_t)-1 || ProgramLauncher_savedSetUid == (uid_t)-1) {
      fail();
   }

   gid_t egid = getegid();
   if (egid != ProgramLauncher_savedSetGid) {
#ifdef _POSIX_SAVED_IDS
      int res = setegid(ProgramLauncher_savedSetGid);
#else
      int res = setregid(egid, ProgramLauncher_savedSetGid);
#endif
      if (res != 0)
         fail();
   }

   uid_t euid = geteuid();
   if (euid != ProgramLauncher_savedSetUid) {
#ifdef _POSIX_SAVED_IDS
      int res = seteuid(ProgramLauncher_savedSetUid);
#else
      int res = setreuid(euid, ProgramLauncher_savedSetUid);
#endif
      if (res != 0)
         fail();
   }
}

#ifndef HAVE_GROUP_MEMBER
static int group_member(gid_t gid) {
   int n = getgroups(0, NULL);
   if (n < 0)
      return 0;

   gid_t* groups = xMallocArray((size_t)n, sizeof(*groups));

   int found = 0;
   if (getgroups(n, groups) >= 0) {
      for (size_t i = 0; i < (size_t)n; i++) {
         if (groups[i] == gid) {
            found = 1;
            break;
         }
      }
   }
   free(groups);
   return found;
}
#endif

static bool ProgramLauncher_canTrustExecStat(const struct stat* sb) {
   if (sb->st_uid == geteuid())
      return S_ISREG(sb->st_mode) && (sb->st_mode & S_IXUSR);
   if (sb->st_gid == getegid() || group_member(sb->st_gid))
      return S_ISREG(sb->st_mode) && (sb->st_mode & S_IXGRP);

   // To prevent users from executing programs they might not trust,
   // ignore S_IXOTH except for programs owned by the root user.
   // This is stricter than what OS would permit executing.
   if (sb->st_uid == 0)
      return S_ISREG(sb->st_mode) && (sb->st_mode & S_IXOTH);

   return false;
}

static int ProgramLauncher_openAndCheckStat(const ProgramLauncher* this, ProgramFileRef* newFileRef) {
   const char* path;
   if (newFileRef) {
      assert(!this->fileRef);
      path = newFileRef->path;
   } else {
      assert(this->fileRef);
      path = ((const ProgramFileRef*)this->fileRef)->path;
   }
   assert(path);

   int fd = -1;
   int savedErrno = errno;
   if (!(this->options & PROGRAM_LAUNCH_NO_SCRIPT)) {
      int openFlags = O_RDONLY | O_NOCTTY | O_NONBLOCK;
      // Consider including O_REGULAR flag in the future.
      // O_REGULAR is not POSIX but supported in NetBSD. There is a
      // proposal for adding it in Linux.
      do {
         fd = open(path, openFlags);
      } while (fd < 0 && errno == EINTR);

      if (fd < 0 && errno != EACCES) {
         return fd;
      }
   }

   if (fd < 0) {
      int openFlags = O_CLOEXEC | O_NOCTTY | O_NONBLOCK;
#ifdef O_EXEC
      openFlags |= O_EXEC;
#else
      // O_EXEC is not supported in Linux; use (O_RDONLY | O_PATH)
      // instead.
      openFlags |= O_RDONLY;
#endif
#ifdef O_PATH
      // O_PATH is specific to Linux and FreeBSD.
      openFlags |= O_PATH;
#endif
      do {
         fd = open(path, openFlags);
      } while (fd < 0 && errno == EINTR);
   }

   if (fd < 0)
      return fd;

   errno = savedErrno;

   struct stat sb;
   int res = fstat(fd, &sb);
   if (res != 0) {
      savedErrno = errno;
      goto fail;
   }

   if (!newFileRef) {
      const ProgramFileRef* fileRef = (const ProgramFileRef*)this->fileRef;
      if (fileRef->dev != sb.st_dev || fileRef->ino != sb.st_ino) {
         // The original file is gone and another file takes its place
         // with the same name. Deny execution for safety.
         savedErrno = ENOENT;
         goto fail;
      }
   }

   if (!ProgramLauncher_canTrustExecStat(&sb)) {
      savedErrno = EACCES;
      goto fail;
   }

   if (newFileRef) {
      newFileRef->dev = sb.st_dev;
      newFileRef->ino = sb.st_ino;
   }
   return fd;

fail:
   close(fd);
   errno = savedErrno;
   return -1;
}

static bool ProgramLauncher_isScriptFile(int fd) {
   assert(fd >= 0);

   char buf[2];
   ssize_t len = pread(fd, buf, sizeof(buf), 0);
   return (len == sizeof(buf) && buf[0] == '#' && buf[1]== '!');
}

void ProgramLauncher_setPath(ProgramLauncher* this, const char* path) {
   if (this->lastErrno != 0)
      return;

   ProgramFileRef* newFileRef = NULL;
   const char* envPath = NULL;
   char* csPathBuf = NULL;
   if (this->fileRef) {
      path = ((const ProgramFileRef*)this->fileRef)->path;
   } else {
      newFileRef = xMalloc(sizeof(*newFileRef));

      if (!strchr(path, '/')) {
         envPath = getenv("PATH");

         if (!(envPath && envPath[0])) {
            size_t csPathSize = confstr(_CS_PATH, NULL, 0);
            if (csPathSize > sizeof("")) {
               csPathBuf = xMalloc(csPathSize);
               if (confstr(_CS_PATH, csPathBuf, csPathSize) == csPathSize) {
                  assert(csPathBuf[0] != '\0');
                  envPath = csPathBuf;
               }
            }
         }

         if (!(envPath && envPath[0])) {
            this->lastErrno = ENOENT;
            goto end;
         }
      }
   }

   if (!(this->options & PROGRAM_LAUNCH_KEEP_SETUID))
      ProgramLauncher_dropSetUid();

   const char* pathPrefix = envPath;
   while (true) {
      char* newPath = NULL;
      const char* pathPrefixEnd = NULL;
      if (newFileRef) {
         if (!envPath) {
            newPath = xStrdup(path);
         } else {
            pathPrefixEnd = String_strchrnul(pathPrefix, ':');
            assert(pathPrefixEnd >= pathPrefix);

            if (pathPrefixEnd > pathPrefix) {
               int pathPrefixLen = (int)(pathPrefixEnd - pathPrefix) - (*(pathPrefixEnd - 1) == '/');
               assert(pathPrefixLen >= 0);
               xAsprintf(&newPath, "%.*s/%s", pathPrefixLen, pathPrefix, path);
            } else {
               // POSIX allows zero-length prefix as "a legacy feature".
               newPath = String_cat("./", path);
            }
         }
         newFileRef->path = newPath;
      }

      int fd = ProgramLauncher_openAndCheckStat(this, newFileRef);
      // execlp() and execvp() from libc stop searching when there is a
      // file with execute permission is found. They can stop searching
      // even when the file has no read permission and the script
      // interpreter would definitely fail on reading the script.

      if (fd >= 0) {
         this->lastErrno = 0;
         if (newFileRef)
            this->fileRef = newFileRef;

         if (!(this->options & PROGRAM_LAUNCH_NO_SCRIPT) && !ProgramLauncher_isScriptFile(fd))
            this->options |= PROGRAM_LAUNCH_NO_SCRIPT;

         close(fd);
         break;
      }

      // Keep "Permission denied" error if there is one during the
      // search.
      if (this->lastErrno != EACCES)
         this->lastErrno = errno;

      free(newPath);

      if (!(pathPrefixEnd && pathPrefixEnd[0]))
         break;

      assert(pathPrefixEnd[0] == ':');
      pathPrefix = pathPrefixEnd + strlen(":");
   }

   if (!(this->options & PROGRAM_LAUNCH_KEEP_SETUID))
      ProgramLauncher_restoreSetUid();

end:
   free(csPathBuf);

   assert(this->lastErrno != 0 || this->fileRef);
   if (this->lastErrno != 0) {
      free(newFileRef);
      errno = this->lastErrno;
   }
}

void ProgramLauncher_execve(ProgramLauncher* this, char* const* argv, char* const* envp) {
   if (!envp)
      envp = environ;

   assert(this->fileRef);
   assert(((const ProgramFileRef*)this->fileRef)->path);
   if (!this->fileRef || !((const ProgramFileRef*)this->fileRef)->path) {
      errno = EFAULT;
      return;
   }

   if (!(this->options & PROGRAM_LAUNCH_KEEP_SETUID)) {
      // Permanently drop privileges
      if (setgid(getgid()) != 0) {
         return;
      }
      if (setuid(getuid()) != 0) {
         return;
      }
   }

   int fd = ProgramLauncher_openAndCheckStat(this, NULL);
   if (fd < 0)
      return;

#ifdef HAVE_FEXECVE
   fexecve(fd, argv, envp);

   // Clean up if fexecve() fails
   int savedErrno = errno;
   close(fd);
   errno = savedErrno;
#else
   close(fd);
   execve(((const ProgramFileRef*)this->fileRef)->path, argv, envp);
#endif
}
