/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CRT.h"

#include "config.h"
#include "String.h"

#include <curses.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#define ColorPair(i,j) COLOR_PAIR((7-i)*8+j)

#define COLORSCHEME_DEFAULT 0
#define COLORSCHEME_MONOCHROME 1
#define COLORSCHEME_BLACKONWHITE 2
#define COLORSCHEME_BLACKONWHITE2 3
#define COLORSCHEME_MIDNIGHT 4
#define COLORSCHEME_BLACKNIGHT 5

#define Black COLOR_BLACK
#define Red COLOR_RED
#define Green COLOR_GREEN
#define Yellow COLOR_YELLOW
#define Blue COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan COLOR_CYAN
#define White COLOR_WHITE

//#link curses

/*{
#include <stdbool.h>

typedef enum ColorElements_ {
   RESET_COLOR,
   DEFAULT_COLOR,
   FUNCTION_BAR,
   FUNCTION_KEY,
   FAILED_SEARCH,
   PANEL_HEADER_FOCUS,
   PANEL_HEADER_UNFOCUS,
   PANEL_HIGHLIGHT_FOCUS,
   PANEL_HIGHLIGHT_UNFOCUS,
   LARGE_NUMBER,
   METER_TEXT,
   METER_VALUE,
   LED_COLOR,
   UPTIME,
   BATTERY,
   TASKS_RUNNING,
   SWAP,
   PROCESS,
   PROCESS_SHADOW,
   PROCESS_TAG,
   PROCESS_MEGABYTES,
   PROCESS_TREE,
   PROCESS_R_STATE,
   PROCESS_BASENAME,
   PROCESS_HIGH_PRIORITY,
   PROCESS_LOW_PRIORITY,
   PROCESS_THREAD,
   PROCESS_THREAD_BASENAME,
   BAR_BORDER,
   BAR_SHADOW,
   GRAPH_1,
   GRAPH_2,
   GRAPH_3,
   GRAPH_4,
   GRAPH_5,
   GRAPH_6,
   GRAPH_7,
   GRAPH_8,
   GRAPH_9,
   MEMORY_USED,
   MEMORY_BUFFERS,
   MEMORY_CACHE,
   LOAD,
   LOAD_AVERAGE_FIFTEEN,
   LOAD_AVERAGE_FIVE,
   LOAD_AVERAGE_ONE,
   CHECK_BOX,
   CHECK_MARK,
   CHECK_TEXT,
   CLOCK,
   HELP_BOLD,
   HOSTNAME,
   CPU_NICE,
   CPU_NORMAL,
   CPU_KERNEL,
   CPU_IOWAIT,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   LAST_COLORELEMENT
} ColorElements;

}*/

// TODO: centralize these in Settings.

static bool CRT_hasColors;

int CRT_delay = 0;

int CRT_colorScheme = 0;

int CRT_colors[LAST_COLORELEMENT] = { 0 };

int CRT_cursorX = 0;

char* CRT_termType;

void *backtraceArray[128];

static void CRT_handleSIGSEGV(int sgn) {
   (void) sgn;
   CRT_done();
   #if __linux
   fprintf(stderr, "\n\nhtop " VERSION " aborting. Please report bug at http://htop.sf.net\n");
   #ifdef HAVE_EXECINFO_H
   size_t size = backtrace(backtraceArray, sizeof(backtraceArray) / sizeof(void *));
   fprintf(stderr, "\n Please include in your report the following backtrace: \n");
   backtrace_symbols_fd(backtraceArray, size, 2);
   fprintf(stderr, "\nAdditionally, in order to make the above backtrace useful,");
   fprintf(stderr, "\nplease also run the following command to generate a disassembly of your binary:");
   fprintf(stderr, "\n\n   objdump -d `which htop` > ~/htop.objdump");
   fprintf(stderr, "\n\nand then attach the file ~/htop.objdump to your bug report.");
   fprintf(stderr, "\n\nThank you for helping to improve htop!\n\n");
   #endif
   #else
   fprintf(stderr, "\n\nhtop " VERSION " aborting. Unsupported platform.\n");
   #endif
   abort();
}

static void CRT_handleSIGTERM(int sgn) {
   (void) sgn;
   CRT_done();
   exit(0);
}

// TODO: pass an instance of Settings instead.

void CRT_init(int delay, int colorScheme) {
   initscr();
   noecho();
   CRT_delay = delay;
   CRT_colorScheme = colorScheme;
   halfdelay(CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   curs_set(0);
   if (has_colors()) {
      start_color();
      CRT_hasColors = true;
   } else {
      CRT_hasColors = false;
   }
   CRT_termType = getenv("TERM");
   if (String_eq(CRT_termType, "xterm") || String_eq(CRT_termType, "xterm-color") || String_eq(CRT_termType, "vt220")) {
      define_key("\033[H", KEY_HOME);
      define_key("\033[F", KEY_END);
      define_key("\033OP", KEY_F(1));
      define_key("\033OQ", KEY_F(2));
      define_key("\033OR", KEY_F(3));
      define_key("\033OS", KEY_F(4));
      define_key("\033[11~", KEY_F(1));
      define_key("\033[12~", KEY_F(2));
      define_key("\033[13~", KEY_F(3));
      define_key("\033[14~", KEY_F(4));
      define_key("\033[17;2~", KEY_F(18));
   }
#ifndef DEBUG
   signal(11, CRT_handleSIGSEGV);
#endif
   signal(SIGTERM, CRT_handleSIGTERM);
   use_default_colors();
   if (!has_colors())
      CRT_colorScheme = 1;
   CRT_setColors(CRT_colorScheme);

   mousemask(BUTTON1_CLICKED, NULL);
}

void CRT_done() {
   curs_set(1);
   endwin();
}

void CRT_fatalError(const char* note) {
   char* sysMsg = strerror(errno);
   CRT_done();
   fprintf(stderr, "%s: %s\n", note, sysMsg);
   exit(2);
}

int CRT_readKey() {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int ret = getch();
   halfdelay(CRT_delay);
   return ret;
}

void CRT_disableDelay() {
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay() {
   halfdelay(CRT_delay);
}

void CRT_setColors(int colorScheme) {
   CRT_colorScheme = colorScheme;
   if (colorScheme == COLORSCHEME_BLACKNIGHT) {
      for (int i = 0; i < 8; i++)
         for (int j = 0; j < 8; j++)
            init_pair((7-i)*8+j, i, j);
   } else {
      for (int i = 0; i < 8; i++) 
         for (int j = 0; j < 8; j++)
            init_pair((7-i)*8+j, i, (j==0?-1:j));
   }

   if (colorScheme == COLORSCHEME_MONOCHROME) {
      CRT_colors[RESET_COLOR] = A_NORMAL;
      CRT_colors[DEFAULT_COLOR] = A_NORMAL;
      CRT_colors[FUNCTION_BAR] = A_REVERSE;
      CRT_colors[FUNCTION_KEY] = A_NORMAL;
      CRT_colors[PANEL_HEADER_FOCUS] = A_REVERSE;
      CRT_colors[PANEL_HEADER_UNFOCUS] = A_REVERSE;
      CRT_colors[PANEL_HIGHLIGHT_FOCUS] = A_REVERSE;
      CRT_colors[PANEL_HIGHLIGHT_UNFOCUS] = A_BOLD;
      CRT_colors[FAILED_SEARCH] = A_REVERSE | A_BOLD;
      CRT_colors[UPTIME] = A_BOLD;
      CRT_colors[BATTERY] = A_BOLD;
      CRT_colors[LARGE_NUMBER] = A_BOLD;
      CRT_colors[METER_TEXT] = A_NORMAL;
      CRT_colors[METER_VALUE] = A_BOLD;
      CRT_colors[LED_COLOR] = A_NORMAL;
      CRT_colors[TASKS_RUNNING] = A_BOLD;
      CRT_colors[PROCESS] = A_NORMAL;
      CRT_colors[PROCESS_SHADOW] = A_DIM;
      CRT_colors[PROCESS_TAG] = A_BOLD;
      CRT_colors[PROCESS_MEGABYTES] = A_BOLD;
      CRT_colors[PROCESS_BASENAME] = A_BOLD;
      CRT_colors[PROCESS_TREE] = A_BOLD;
      CRT_colors[PROCESS_R_STATE] = A_BOLD;
      CRT_colors[PROCESS_HIGH_PRIORITY] = A_BOLD;
      CRT_colors[PROCESS_LOW_PRIORITY] = A_DIM;
      CRT_colors[PROCESS_THREAD] = A_BOLD;
      CRT_colors[PROCESS_THREAD_BASENAME] = A_REVERSE;
      CRT_colors[BAR_BORDER] = A_BOLD;
      CRT_colors[BAR_SHADOW] = A_DIM;
      CRT_colors[SWAP] = A_BOLD;
      CRT_colors[GRAPH_1] = A_BOLD;
      CRT_colors[GRAPH_2] = A_BOLD;
      CRT_colors[GRAPH_3] = A_BOLD;
      CRT_colors[GRAPH_4] = A_NORMAL;
      CRT_colors[GRAPH_5] = A_NORMAL;
      CRT_colors[GRAPH_6] = A_NORMAL;
      CRT_colors[GRAPH_7] = A_DIM;
      CRT_colors[GRAPH_8] = A_DIM;
      CRT_colors[GRAPH_9] = A_DIM;
      CRT_colors[MEMORY_USED] = A_BOLD;
      CRT_colors[MEMORY_BUFFERS] = A_NORMAL;
      CRT_colors[MEMORY_CACHE] = A_NORMAL;
      CRT_colors[LOAD_AVERAGE_FIFTEEN] = A_DIM;
      CRT_colors[LOAD_AVERAGE_FIVE] = A_NORMAL;
      CRT_colors[LOAD_AVERAGE_ONE] = A_BOLD;
      CRT_colors[LOAD] = A_BOLD;
      CRT_colors[HELP_BOLD] = A_BOLD;
      CRT_colors[CLOCK] = A_BOLD;
      CRT_colors[CHECK_BOX] = A_BOLD;
      CRT_colors[CHECK_MARK] = A_NORMAL;
      CRT_colors[CHECK_TEXT] = A_NORMAL;
      CRT_colors[HOSTNAME] = A_BOLD;
      CRT_colors[CPU_NICE] = A_NORMAL;
      CRT_colors[CPU_NORMAL] = A_BOLD;
      CRT_colors[CPU_KERNEL] = A_BOLD;
      CRT_colors[CPU_IOWAIT] = A_NORMAL;
      CRT_colors[CPU_IRQ] = A_BOLD;
      CRT_colors[CPU_SOFTIRQ] = A_BOLD;
      CRT_colors[CPU_STEAL] = A_REVERSE;
      CRT_colors[CPU_GUEST] = A_REVERSE;
   } else if (CRT_colorScheme == COLORSCHEME_BLACKONWHITE) {
      CRT_colors[RESET_COLOR] = ColorPair(Black,White);
      CRT_colors[DEFAULT_COLOR] = ColorPair(Black,White);
      CRT_colors[FUNCTION_BAR] = ColorPair(Black,Cyan);
      CRT_colors[FUNCTION_KEY] = ColorPair(Black,White);
      CRT_colors[PANEL_HEADER_FOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HIGHLIGHT_FOCUS] = ColorPair(Black,Cyan);
      CRT_colors[PANEL_HIGHLIGHT_UNFOCUS] = ColorPair(Blue,White);
      CRT_colors[FAILED_SEARCH] = ColorPair(Red,Cyan);
      CRT_colors[UPTIME] = ColorPair(Yellow,White);
      CRT_colors[BATTERY] = ColorPair(Yellow,White);
      CRT_colors[LARGE_NUMBER] = ColorPair(Red,White);
      CRT_colors[METER_TEXT] = ColorPair(Blue,White);
      CRT_colors[METER_VALUE] = ColorPair(Black,White);
      CRT_colors[LED_COLOR] = ColorPair(Green,White);
      CRT_colors[TASKS_RUNNING] = ColorPair(Green,White);
      CRT_colors[PROCESS] = ColorPair(Black,White);
      CRT_colors[PROCESS_SHADOW] = A_BOLD | ColorPair(Black,White);
      CRT_colors[PROCESS_TAG] = ColorPair(White,Blue);
      CRT_colors[PROCESS_MEGABYTES] = ColorPair(Blue,White);
      CRT_colors[PROCESS_BASENAME] = ColorPair(Blue,White);
      CRT_colors[PROCESS_TREE] = ColorPair(Green,White);
      CRT_colors[PROCESS_R_STATE] = ColorPair(Green,White);
      CRT_colors[PROCESS_HIGH_PRIORITY] = ColorPair(Red,White);
      CRT_colors[PROCESS_LOW_PRIORITY] = ColorPair(Red,White);
      CRT_colors[PROCESS_THREAD] = ColorPair(Blue,White);
      CRT_colors[PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,White);
      CRT_colors[BAR_BORDER] = ColorPair(Blue,White);
      CRT_colors[BAR_SHADOW] = ColorPair(Black,White);
      CRT_colors[SWAP] = ColorPair(Red,White);
      CRT_colors[GRAPH_1] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_2] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_3] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_4] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_5] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_6] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_7] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_8] = ColorPair(Yellow,White);
      CRT_colors[GRAPH_9] = ColorPair(Yellow,White);
      CRT_colors[MEMORY_USED] = ColorPair(Green,White);
      CRT_colors[MEMORY_BUFFERS] = ColorPair(Cyan,White);
      CRT_colors[MEMORY_CACHE] = ColorPair(Yellow,White);
      CRT_colors[LOAD_AVERAGE_FIFTEEN] = ColorPair(Black,White);
      CRT_colors[LOAD_AVERAGE_FIVE] = ColorPair(Black,White);
      CRT_colors[LOAD_AVERAGE_ONE] = ColorPair(Black,White);
      CRT_colors[LOAD] = ColorPair(Black,White);
      CRT_colors[HELP_BOLD] = ColorPair(Blue,White);
      CRT_colors[CLOCK] = ColorPair(Black,White);
      CRT_colors[CHECK_BOX] = ColorPair(Blue,White);
      CRT_colors[CHECK_MARK] = ColorPair(Black,White);
      CRT_colors[CHECK_TEXT] = ColorPair(Black,White);
      CRT_colors[HOSTNAME] = ColorPair(Black,White);
      CRT_colors[CPU_NICE] = ColorPair(Cyan,White);
      CRT_colors[CPU_NORMAL] = ColorPair(Green,White);
      CRT_colors[CPU_KERNEL] = ColorPair(Red,White);
      CRT_colors[CPU_IOWAIT] = A_BOLD | ColorPair(Black, White);
      CRT_colors[CPU_IRQ] = ColorPair(Blue,White);
      CRT_colors[CPU_SOFTIRQ] = ColorPair(Blue,White);
      CRT_colors[CPU_STEAL] = ColorPair(Cyan,White);
      CRT_colors[CPU_GUEST] = ColorPair(Cyan,White);
   } else if (CRT_colorScheme == COLORSCHEME_BLACKONWHITE2) {
      CRT_colors[RESET_COLOR] = ColorPair(Black,Black);
      CRT_colors[DEFAULT_COLOR] = ColorPair(Black,Black);
      CRT_colors[FUNCTION_BAR] = ColorPair(Black,Cyan);
      CRT_colors[FUNCTION_KEY] = ColorPair(Black,Black);
      CRT_colors[PANEL_HEADER_FOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HIGHLIGHT_FOCUS] = ColorPair(Black,Cyan);
      CRT_colors[PANEL_HIGHLIGHT_UNFOCUS] = ColorPair(Blue,Black);
      CRT_colors[FAILED_SEARCH] = ColorPair(Red,Cyan);
      CRT_colors[UPTIME] = ColorPair(Yellow,Black);
      CRT_colors[BATTERY] = ColorPair(Yellow,Black);
      CRT_colors[LARGE_NUMBER] = ColorPair(Red,Black);
      CRT_colors[METER_TEXT] = ColorPair(Blue,Black);
      CRT_colors[METER_VALUE] = ColorPair(Black,Black);
      CRT_colors[LED_COLOR] = ColorPair(Green,Black);
      CRT_colors[TASKS_RUNNING] = ColorPair(Green,Black);
      CRT_colors[PROCESS] = ColorPair(Black,Black);
      CRT_colors[PROCESS_SHADOW] = A_BOLD | ColorPair(Black,Black);
      CRT_colors[PROCESS_TAG] = ColorPair(White,Blue);
      CRT_colors[PROCESS_MEGABYTES] = ColorPair(Blue,Black);
      CRT_colors[PROCESS_BASENAME] = ColorPair(Green,Black);
      CRT_colors[PROCESS_TREE] = ColorPair(Blue,Black);
      CRT_colors[PROCESS_R_STATE] = ColorPair(Green,Black);
      CRT_colors[PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black);
      CRT_colors[PROCESS_LOW_PRIORITY] = ColorPair(Red,Black);
      CRT_colors[PROCESS_THREAD] = ColorPair(Blue,Black);
      CRT_colors[PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,Black);
      CRT_colors[BAR_BORDER] = ColorPair(Blue,Black);
      CRT_colors[BAR_SHADOW] = ColorPair(Black,Black);
      CRT_colors[SWAP] = ColorPair(Red,Black);
      CRT_colors[GRAPH_1] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_2] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_3] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_4] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_5] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_6] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_7] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_8] = ColorPair(Yellow,Black);
      CRT_colors[GRAPH_9] = ColorPair(Yellow,Black);
      CRT_colors[MEMORY_USED] = ColorPair(Green,Black);
      CRT_colors[MEMORY_BUFFERS] = ColorPair(Cyan,Black);
      CRT_colors[MEMORY_CACHE] = ColorPair(Yellow,Black);
      CRT_colors[LOAD_AVERAGE_FIFTEEN] = ColorPair(Black,Black);
      CRT_colors[LOAD_AVERAGE_FIVE] = ColorPair(Black,Black);
      CRT_colors[LOAD_AVERAGE_ONE] = ColorPair(Black,Black);
      CRT_colors[LOAD] = ColorPair(White,Black);
      CRT_colors[HELP_BOLD] = ColorPair(Blue,Black);
      CRT_colors[CLOCK] = ColorPair(White,Black);
      CRT_colors[CHECK_BOX] = ColorPair(Blue,Black);
      CRT_colors[CHECK_MARK] = ColorPair(Black,Black);
      CRT_colors[CHECK_TEXT] = ColorPair(Black,Black);
      CRT_colors[HOSTNAME] = ColorPair(White,Black);
      CRT_colors[CPU_NICE] = ColorPair(Cyan,Black);
      CRT_colors[CPU_NORMAL] = ColorPair(Green,Black);
      CRT_colors[CPU_KERNEL] = ColorPair(Red,Black);
      CRT_colors[CPU_IOWAIT] = A_BOLD | ColorPair(Black, Black);
      CRT_colors[CPU_IRQ] = A_BOLD | ColorPair(Blue,Black);
      CRT_colors[CPU_SOFTIRQ] = ColorPair(Blue,Black);
      CRT_colors[CPU_STEAL] = ColorPair(Black,Black);
      CRT_colors[CPU_GUEST] = ColorPair(Black,Black);
   } else if (CRT_colorScheme == COLORSCHEME_MIDNIGHT) {
      CRT_colors[RESET_COLOR] = ColorPair(White,Blue);
      CRT_colors[DEFAULT_COLOR] = ColorPair(White,Blue);
      CRT_colors[FUNCTION_BAR] = ColorPair(Black,Cyan);
      CRT_colors[FUNCTION_KEY] = A_NORMAL;
      CRT_colors[PANEL_HEADER_FOCUS] = ColorPair(Black,Cyan);
      CRT_colors[PANEL_HEADER_UNFOCUS] = ColorPair(Black,Cyan);
      CRT_colors[PANEL_HIGHLIGHT_FOCUS] = ColorPair(Black,White);
      CRT_colors[PANEL_HIGHLIGHT_UNFOCUS] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[FAILED_SEARCH] = ColorPair(Red,Cyan);
      CRT_colors[UPTIME] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[BATTERY] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[LARGE_NUMBER] = A_BOLD | ColorPair(Red,Blue);
      CRT_colors[METER_TEXT] = ColorPair(Cyan,Blue);
      CRT_colors[METER_VALUE] = A_BOLD | ColorPair(Cyan,Blue);
      CRT_colors[LED_COLOR] = ColorPair(Green,Blue);
      CRT_colors[TASKS_RUNNING] = A_BOLD | ColorPair(Green,Blue);
      CRT_colors[PROCESS] = ColorPair(White,Blue);
      CRT_colors[PROCESS_SHADOW] = A_BOLD | ColorPair(Black,Blue);
      CRT_colors[PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[PROCESS_MEGABYTES] = ColorPair(Cyan,Blue);
      CRT_colors[PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan,Blue);
      CRT_colors[PROCESS_TREE] = ColorPair(Cyan,Blue);
      CRT_colors[PROCESS_R_STATE] = ColorPair(Green,Blue);
      CRT_colors[PROCESS_HIGH_PRIORITY] = ColorPair(Red,Blue);
      CRT_colors[PROCESS_LOW_PRIORITY] = ColorPair(Red,Blue);
      CRT_colors[PROCESS_THREAD] = ColorPair(Green,Blue);
      CRT_colors[PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green,Blue);
      CRT_colors[BAR_BORDER] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[BAR_SHADOW] = ColorPair(Cyan,Blue);
      CRT_colors[SWAP] = ColorPair(Red,Blue);
      CRT_colors[GRAPH_1] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_2] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_3] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_4] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_5] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_6] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_7] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_8] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[GRAPH_9] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[MEMORY_USED] = A_BOLD | ColorPair(Green,Blue);
      CRT_colors[MEMORY_BUFFERS] = A_BOLD | ColorPair(Cyan,Blue);
      CRT_colors[MEMORY_CACHE] = A_BOLD | ColorPair(Yellow,Blue);
      CRT_colors[LOAD_AVERAGE_FIFTEEN] = A_BOLD | ColorPair(Black,Blue);
      CRT_colors[LOAD_AVERAGE_FIVE] = A_NORMAL | ColorPair(White,Blue);
      CRT_colors[LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White,Blue);
      CRT_colors[LOAD] = A_BOLD | ColorPair(White,Blue);
      CRT_colors[HELP_BOLD] = A_BOLD | ColorPair(Cyan,Blue);
      CRT_colors[CLOCK] = ColorPair(White,Blue);
      CRT_colors[CHECK_BOX] = ColorPair(Cyan,Blue);
      CRT_colors[CHECK_MARK] = A_BOLD | ColorPair(White,Blue);
      CRT_colors[CHECK_TEXT] = A_NORMAL | ColorPair(White,Blue);
      CRT_colors[HOSTNAME] = ColorPair(White,Blue);
      CRT_colors[CPU_NICE] = A_BOLD | ColorPair(Cyan,Blue);
      CRT_colors[CPU_NORMAL] = A_BOLD | ColorPair(Green,Blue);
      CRT_colors[CPU_KERNEL] = A_BOLD | ColorPair(Red,Blue);
      CRT_colors[CPU_IOWAIT] = A_BOLD | ColorPair(Blue,Blue);
      CRT_colors[CPU_IRQ] = A_BOLD | ColorPair(Black,Blue);
      CRT_colors[CPU_SOFTIRQ] = ColorPair(Black,Blue);
      CRT_colors[CPU_STEAL] = ColorPair(White,Blue);
      CRT_colors[CPU_GUEST] = ColorPair(White,Blue);
   } else if (CRT_colorScheme == COLORSCHEME_BLACKNIGHT) {
      CRT_colors[RESET_COLOR] = ColorPair(Cyan,Black);
      CRT_colors[DEFAULT_COLOR] = ColorPair(Cyan,Black);
      CRT_colors[FUNCTION_BAR] = ColorPair(Black,Green);
      CRT_colors[FUNCTION_KEY] = ColorPair(Cyan,Black);
      CRT_colors[PANEL_HEADER_FOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HIGHLIGHT_FOCUS] = ColorPair(Black,Cyan);
      CRT_colors[PANEL_HIGHLIGHT_UNFOCUS] = ColorPair(Black,White);
      CRT_colors[FAILED_SEARCH] = ColorPair(Red,Cyan);
      CRT_colors[UPTIME] = ColorPair(Green,Black);
      CRT_colors[BATTERY] = ColorPair(Green,Black);
      CRT_colors[LARGE_NUMBER] = A_BOLD | ColorPair(Red,Black);
      CRT_colors[METER_TEXT] = ColorPair(Cyan,Black);
      CRT_colors[METER_VALUE] = ColorPair(Green,Black);
      CRT_colors[LED_COLOR] = ColorPair(Green,Black);
      CRT_colors[TASKS_RUNNING] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[PROCESS] = ColorPair(Cyan,Black);
      CRT_colors[PROCESS_SHADOW] = A_BOLD | ColorPair(Black,Black);
      CRT_colors[PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Black);
      CRT_colors[PROCESS_MEGABYTES] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[PROCESS_BASENAME] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[PROCESS_TREE] = ColorPair(Cyan,Black);
      CRT_colors[PROCESS_THREAD] = ColorPair(Green,Black);
      CRT_colors[PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,Black);
      CRT_colors[PROCESS_R_STATE] = ColorPair(Green,Black);
      CRT_colors[PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black);
      CRT_colors[PROCESS_LOW_PRIORITY] = ColorPair(Red,Black);
      CRT_colors[BAR_BORDER] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[BAR_SHADOW] = ColorPair(Cyan,Black);
      CRT_colors[SWAP] = ColorPair(Red,Black);
      CRT_colors[GRAPH_1] = A_BOLD | ColorPair(Red,Black);
      CRT_colors[GRAPH_2] = ColorPair(Red,Black);
      CRT_colors[GRAPH_3] = A_BOLD | ColorPair(Yellow,Black);
      CRT_colors[GRAPH_4] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[GRAPH_5] = ColorPair(Green,Black);
      CRT_colors[GRAPH_6] = ColorPair(Cyan,Black);
      CRT_colors[GRAPH_7] = A_BOLD | ColorPair(Blue,Black);
      CRT_colors[GRAPH_8] = ColorPair(Blue,Black);
      CRT_colors[GRAPH_9] = A_BOLD | ColorPair(Black,Black);
      CRT_colors[MEMORY_USED] = ColorPair(Green,Black);
      CRT_colors[MEMORY_BUFFERS] = ColorPair(Blue,Black);
      CRT_colors[MEMORY_CACHE] = ColorPair(Yellow,Black);
      CRT_colors[LOAD_AVERAGE_FIFTEEN] = ColorPair(Green,Black);
      CRT_colors[LOAD_AVERAGE_FIVE] = ColorPair(Green,Black);
      CRT_colors[LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[LOAD] = A_BOLD;
      CRT_colors[HELP_BOLD] = A_BOLD | ColorPair(Cyan,Black);
      CRT_colors[CLOCK] = ColorPair(Green,Black);
      CRT_colors[CHECK_BOX] = ColorPair(Green,Black);
      CRT_colors[CHECK_MARK] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[CHECK_TEXT] = ColorPair(Cyan,Black);
      CRT_colors[HOSTNAME] = ColorPair(Green,Black);
      CRT_colors[CPU_NICE] = ColorPair(Blue,Black);
      CRT_colors[CPU_NORMAL] = ColorPair(Green,Black);
      CRT_colors[CPU_KERNEL] = ColorPair(Red,Black);
      CRT_colors[CPU_IOWAIT] = ColorPair(Yellow,Black);
      CRT_colors[CPU_IRQ] = A_BOLD | ColorPair(Blue,Black);
      CRT_colors[CPU_SOFTIRQ] = ColorPair(Blue,Black);
      CRT_colors[CPU_STEAL] = ColorPair(Cyan,Black);
      CRT_colors[CPU_GUEST] = ColorPair(Cyan,Black);
   } else {
      /* Default */
      CRT_colors[RESET_COLOR] = ColorPair(White,Black);
      CRT_colors[DEFAULT_COLOR] = ColorPair(White,Black);
      CRT_colors[FUNCTION_BAR] = ColorPair(Black,Cyan);
      CRT_colors[FUNCTION_KEY] = ColorPair(White,Black);
      CRT_colors[PANEL_HEADER_FOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green);
      CRT_colors[PANEL_HIGHLIGHT_FOCUS] = ColorPair(Black,Cyan);
      CRT_colors[PANEL_HIGHLIGHT_UNFOCUS] = ColorPair(Black,White);
      CRT_colors[FAILED_SEARCH] = ColorPair(Red,Cyan);
      CRT_colors[UPTIME] = A_BOLD | ColorPair(Cyan,Black);
      CRT_colors[BATTERY] = A_BOLD | ColorPair(Cyan,Black);
      CRT_colors[LARGE_NUMBER] = A_BOLD | ColorPair(Red,Black);
      CRT_colors[METER_TEXT] = ColorPair(Cyan,Black);
      CRT_colors[METER_VALUE] = A_BOLD | ColorPair(Cyan,Black);
      CRT_colors[LED_COLOR] = ColorPair(Green,Black);
      CRT_colors[TASKS_RUNNING] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[PROCESS] = A_NORMAL;
      CRT_colors[PROCESS_SHADOW] = A_BOLD | ColorPair(Black,Black);
      CRT_colors[PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Black);
      CRT_colors[PROCESS_MEGABYTES] = ColorPair(Cyan,Black);
      CRT_colors[PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan,Black);
      CRT_colors[PROCESS_TREE] = ColorPair(Cyan,Black);
      CRT_colors[PROCESS_R_STATE] = ColorPair(Green,Black);
      CRT_colors[PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black);
      CRT_colors[PROCESS_LOW_PRIORITY] = ColorPair(Red,Black);
      CRT_colors[PROCESS_THREAD] = ColorPair(Green,Black);
      CRT_colors[PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[BAR_BORDER] = A_BOLD;
      CRT_colors[BAR_SHADOW] = A_BOLD | ColorPair(Black,Black);
      CRT_colors[SWAP] = ColorPair(Red,Black);
      CRT_colors[GRAPH_1] = A_BOLD | ColorPair(Red,Black);
      CRT_colors[GRAPH_2] = ColorPair(Red,Black);
      CRT_colors[GRAPH_3] = A_BOLD | ColorPair(Yellow,Black);
      CRT_colors[GRAPH_4] = A_BOLD | ColorPair(Green,Black);
      CRT_colors[GRAPH_5] = ColorPair(Green,Black);
      CRT_colors[GRAPH_6] = ColorPair(Cyan,Black);
      CRT_colors[GRAPH_7] = A_BOLD | ColorPair(Blue,Black);
      CRT_colors[GRAPH_8] = ColorPair(Blue,Black);
      CRT_colors[GRAPH_9] = A_BOLD | ColorPair(Black,Black);
      CRT_colors[MEMORY_USED] = ColorPair(Green,Black);
      CRT_colors[MEMORY_BUFFERS] = ColorPair(Blue,Black);
      CRT_colors[MEMORY_CACHE] = ColorPair(Yellow,Black);
      CRT_colors[LOAD_AVERAGE_FIFTEEN] = A_BOLD | ColorPair(Black,Black);
      CRT_colors[LOAD_AVERAGE_FIVE] = A_NORMAL;
      CRT_colors[LOAD_AVERAGE_ONE] = A_BOLD;
      CRT_colors[LOAD] = A_BOLD;
      CRT_colors[HELP_BOLD] = A_BOLD | ColorPair(Cyan,Black);
      CRT_colors[CLOCK] = A_BOLD;
      CRT_colors[CHECK_BOX] = ColorPair(Cyan,Black);
      CRT_colors[CHECK_MARK] = A_BOLD;
      CRT_colors[CHECK_TEXT] = A_NORMAL;
      CRT_colors[HOSTNAME] = A_BOLD;
      CRT_colors[CPU_NICE] = ColorPair(Blue,Black);
      CRT_colors[CPU_NORMAL] = ColorPair(Green,Black);
      CRT_colors[CPU_KERNEL] = ColorPair(Red,Black);
      CRT_colors[CPU_IOWAIT] = A_BOLD | ColorPair(Black, Black);
      CRT_colors[CPU_IRQ] = ColorPair(Yellow,Black);
      CRT_colors[CPU_SOFTIRQ] = ColorPair(Magenta,Black);
      CRT_colors[CPU_STEAL] = ColorPair(Cyan,Black);
      CRT_colors[CPU_GUEST] = ColorPair(Cyan,Black);
   }
}
