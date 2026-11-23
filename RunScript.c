/*
htop - RunScript.h
(C) 2025 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "RunScript.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Action.h"
#include "InfoScreen.h"
#include "MainPanel.h"
#include "Object.h"
#include "Panel.h"
#include "Process.h"
#include "Row.h"
#include "ScriptOutputScreen.h"
#include "Settings.h"
#include "XUtils.h"


static void write_row(Row* row, int write_fd) {
   Process* this = (Process*) row;
   assert(Object_isA((const Object*) this, (const ObjectClass*) &Process_class));

   int pid_len = 0;
   int pid = row->id;
   while (pid > 0) {
      pid /= 10;
      pid_len++;
   }

   char* line;
   int user_len = strlen(this->user);
   int cmd_len = strlen(Process_getCommand(this));
   // writes pid_len:PID,user_len:User,cmd_len:Command\n in netstring format
   xAsprintf(&line, "%d:%d,%d:%s,%d:%s\n", pid_len, row->id, user_len, this->user, cmd_len, Process_getCommand(this));

   size_t count = strlen(line);
   char* line_start = line;
   while (count > 0) {
      ssize_t res = write(write_fd, line_start, strlen(line_start));
      if ((res == -1 && errno != EINTR) || res == 0)
         break;
      count -= res;
      line_start += res;
   }
  free(line);
}

void RunScript(State* st) {
   int child_read[2] = {0, 0};
   int child_write[2] = {0, 0};

   if (pipe(child_read) == -1)
      return;
   if (pipe(child_write) == -1) {
      close(child_read[0]);
      close(child_read[1]);
      return;
   }

   pid_t child = fork();
   if (child == -1) {
      close(child_read[0]);
      close(child_read[1]);
      close(child_write[0]);
      close(child_write[1]);
      fprintf(stderr, "fork failed\n");
      return;
   } else if (child == 0) {
      close(child_read[1]);
      dup2(child_read[0], STDIN_FILENO);
      close(child_read[0]);

      close(child_write[0]);
      dup2(child_write[1], STDOUT_FILENO);
      dup2(child_write[1], STDERR_FILENO);
      close(child_write[1]);

      char* home = getenv("XDG_CONFIG_HOME");
      if (!home)
         home = String_cat(getenv("HOME"), "/.config");

      const char* path = String_cat(home, "/htop/run_script");
      FILE* file = fopen(path, "r");
      if (file) {
         // executing script in root's directory, probably not malicious
         root_exec(path, false);
         // should not reach here unless fexecve fails
         fprintf(stderr, "error excuting %s\n", path);
      } else {
         // check if htoprc has something
         const char* htoprc_path = st->host->settings->scriptLocation;
         // path can point to anything, so drop sudo for safety
         root_exec(htoprc_path, true);

         // only reach here if fexecve fails
         fprintf(stderr, "error executing %s from htoprc. ", htoprc_path);
         fprintf(stderr, "if you expected your runscript to be executed, htop looked for it at %s", path);
      }
      exit(127);
   }

   close(child_read[0]);
   close(child_write[1]);

   bool anyTagged = false;
   Panel* super = &st->mainPanel->super;
   for (int i = 0; i < Panel_size(super); i++) {
      Row* row = (Row*) Panel_get(super, i);
      if (row->tag) {
         write_row(row, child_read[1]);
         anyTagged = true;
      }
   }
   // if nothing was tagged, operate on the highlighted row
   if (!anyTagged) {
      Row* row = (Row*) Panel_getSelected(super);
      if (row)
         write_row(row, child_read[1]);
   }

   // tell script/child we're done with sending input
   close(child_read[1]);

   const Process* p = (Process*) Panel_getSelected((Panel*)st->mainPanel);
   if (!p)
      return;

   assert(Object_isA((const Object*) p, (const ObjectClass*) &Process_class));
   ScriptOutputScreen* sos = ScriptOutputScreen_new(p);
   if (fcntl(child_write[0], F_SETFL, O_NONBLOCK) >= 0) {
      ScriptOutputScreen_SetFd(sos, child_write[0]);
      InfoScreen_run((InfoScreen*)sos);
   }
   ScriptOutputScreen_delete((Object*)sos);
}

void root_exec(const char* path, bool drop_sudo) {
   // do not use O_CLOEXEC flag as that will cause fexecve to fail with ENOENT on a script
   int fd = open(path, O_RDONLY);
   if (fd < 0) {
      perror("open");
      return;
   }
   // check that path is even a file
   struct stat st;
   if (fstat(fd, &st) == -1) {
      perror("fstat");
      return;
   }

   uid_t curr_uid = getuid();
   if (drop_sudo) {
      // need to remove root from ourselves if we are root
      if (curr_uid == 0) {
         char* sudo_uid_str = getenv("SUDO_UID");
         if (!sudo_uid_str) {
            fprintf(stderr, "sudo uid envar does not exist\n");
            return;
         }
         uid_t uid = strtoul(sudo_uid_str, NULL, 10);
         if (uid == 0) {
            fprintf(stderr, "sudo uid envar is root, failed to get uid of invoking user\n");
            return;
         }

         char* sudo_gid_str = getenv("SUDO_GID");
         if (!sudo_gid_str) {
            fprintf(stderr, "sudo gid envar does not exist\n");
            return;
         }
         gid_t gid = strtoul(sudo_gid_str, NULL, 10);
         if (gid == 0) {
            fprintf(stderr, "sudo gid envar is root group, failed to get gid of invoking user\n");
            return;
         }
         
         // remove supplementary groups
         if (setgroups(0, NULL) == -1) {
            perror("setgroups");
            return;
         }
         if (setgid(gid) == -1) {
            perror("setgid");
            return;
         }
         if (setuid(uid) == -1) {
            perror("setuid");
            return;
         }
      }
   } else if (curr_uid == 0 && (st.st_gid != 0 || st.st_uid != 0)) {
      // we are root and script does not belongs to root, consider it unsafe
      fprintf(stderr, "%s does not belong to root; has gid %u and uid %u\n", path, st.st_gid, st.st_uid);
      return;
   }

   static char* argv[] = {NULL, NULL};
   argv[0] = xStrdup(path);
   static char* env[] = {NULL};
   fexecve(fd, argv, env);

   perror("fexecve");
}
