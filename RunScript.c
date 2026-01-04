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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

   int pid_length = snprintf(NULL, 0, "%d", row->id);
   char pid[pid_length + 1];
   snprintf(pid, pid_length + 1, "%d", row->id);

   char* pid_str = String_cat(pid, "\t");
   char* user = String_cat(this->user, "\t");
   char* cmd = String_cat(Process_getCommand(this), "\n");
   char* user_and_cmd = String_cat(user, cmd);
   char* line = String_cat(pid_str, user_and_cmd);

   // writes PID\tUser\tCommand\n, using TSV format
   size_t count = strlen(line);
   char* line_start = line;
   while (count > 0) {
      ssize_t res = write(write_fd, line_start, strlen(line));
      if ((res == -1 && errno != EINTR) || res == 0)
         break;

      count -= res;
      line_start += res;
   }

   free(pid_str);
   free(user);
   free(cmd);
   free(user_and_cmd);
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
         home = String_cat(getenv("HOME"), "./config");

      const char* path = String_cat(home, "/htop/run_script");
      FILE* file = fopen(path, "r");
      if (file) {
         execl(path, path, NULL);
         // should not reach here unless execl fails
         fprintf(stderr, "error excuting %s\n", path);
         perror("execl");
      } else {
         // check if htoprc has something
         const char* htoprc_path = st->host->settings->scriptLocation;
         execl(htoprc_path, htoprc_path, NULL);

         // only reach here if execl fails
         fprintf(stderr, "error executing %s from htoprc", htoprc_path);
         fprintf(stderr, "if you expected your runscript to be executed, htop looked for it at %s", path);
         perror("execl");
      }
      exit(1);
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
