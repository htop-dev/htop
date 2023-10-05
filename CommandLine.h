#ifndef HEADER_CommandLine
#define HEADER_CommandLine
/*
htop - CommandLine.h
(C) 2004-2011 Hisham H. Muhammad
(C) 2020-2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

typedef enum {
   STATUS_OK,
   STATUS_ERROR_EXIT,
   STATUS_OK_EXIT
} CommandLineStatus;

extern const char* program;

int CommandLine_run(int argc, char** argv);

#endif
