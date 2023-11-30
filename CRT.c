/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CRT.h"

#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CommandLine.h"
#include "ProvideCurses.h"
#include "ProvideTerm.h"
#include "XUtils.h"

#if !defined(NDEBUG) && defined(HAVE_MEMFD_CREATE)
#include <sys/mman.h>
#endif

#if defined(HAVE_LIBUNWIND_H) && defined(HAVE_LIBUNWIND)
# define PRINT_BACKTRACE
# define UNW_LOCAL_ONLY
# include <libunwind.h>
# if defined(HAVE_DLADDR)
#  include <dlfcn.h>
# endif
#elif defined(HAVE_EXECINFO_H)
# define PRINT_BACKTRACE
# include <execinfo.h>
#endif


#define ColorIndex(i,j) ((7-(i))*8+(j))

#define ColorPair(i,j) COLOR_PAIR(ColorIndex(i,j))

#define Black   COLOR_BLACK
#define Red     COLOR_RED
#define Green   COLOR_GREEN
#define Yellow  COLOR_YELLOW
#define Blue    COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan    COLOR_CYAN
#define White   COLOR_WHITE

#define ColorPairGrayBlack  ColorPair(Magenta,Magenta)
#define ColorIndexGrayBlack ColorIndex(Magenta,Magenta)

#define ColorPairWhiteDefault  ColorPair(Red, Red)
#define ColorIndexWhiteDefault ColorIndex(Red, Red)

static const char* const CRT_treeStrAscii[LAST_TREE_STR] = {
   [TREE_STR_VERT] = "|",
   [TREE_STR_RTEE] = "`",
   [TREE_STR_BEND] = "`",
   [TREE_STR_TEND] = ",",
   [TREE_STR_OPEN] = "+",
   [TREE_STR_SHUT] = "-",
   [TREE_STR_ASC]  = "+",
   [TREE_STR_DESC] = "-",
};

#ifdef HAVE_LIBNCURSESW

static const char* const CRT_treeStrUtf8[LAST_TREE_STR] = {
   [TREE_STR_VERT] = "\xe2\x94\x82", // │
   [TREE_STR_RTEE] = "\xe2\x94\x9c", // ├
   [TREE_STR_BEND] = "\xe2\x94\x94", // └
   [TREE_STR_TEND] = "\xe2\x94\x8c", // ┌
   [TREE_STR_OPEN] = "+",            // +, TODO use 🮯 'BOX DRAWINGS LIGHT HORIZONTAL
                                     // WITH VERTICAL STROKE' (U+1FBAF, "\xf0\x9f\xae\xaf") when
                                     // Unicode 13 is common
   [TREE_STR_SHUT] = "\xe2\x94\x80", // ─
   [TREE_STR_ASC]  = "\xe2\x96\xb3", // △
   [TREE_STR_DESC] = "\xe2\x96\xbd", // ▽
};

bool CRT_utf8 = false;

#endif

const char* const* CRT_treeStr = CRT_treeStrAscii;

static const Settings* CRT_crashSettings;
static const int* CRT_delay;

const char* CRT_degreeSign;

static const char* initDegreeSign(void) {
#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8)
      return "\xc2\xb0";

   static char buffer[4];
   // this might fail if the current locale does not support wide characters
   int r = snprintf(buffer, sizeof(buffer), "%lc", 176);
   if (r > 0)
      return buffer;
#endif

   return "";
}

const int* CRT_colors;

static int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT] = {
   [COLORSCHEME_DEFAULT] = {
      [RESET_COLOR] = ColorPair(White, Black),
      [DEFAULT_COLOR] = ColorPair(White, Black),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = ColorPair(White, Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Black, White),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = A_BOLD | ColorPair(Red, Black),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = A_BOLD | ColorPair(Cyan, Black),
      [BATTERY] = A_BOLD | ColorPair(Cyan, Black),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red, Black),
      [METER_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [METER_TEXT] = ColorPair(Cyan, Black),
      [METER_VALUE] = A_BOLD | ColorPair(Cyan, Black),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Black),
      [METER_VALUE_IOREAD] = ColorPair(Green, Black),
      [METER_VALUE_IOWRITE] = A_BOLD | ColorPair(Blue, Black),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(White, Black),
      [METER_VALUE_OK] = ColorPair(Green, Black),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Black),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green, Black),
      [PROCESS] = A_NORMAL,
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow, Black),
      [PROCESS_MEGABYTES] = ColorPair(Cyan, Black),
      [PROCESS_GIGABYTES] = ColorPair(Green, Black),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan, Black),
      [PROCESS_TREE] = ColorPair(Cyan, Black),
      [PROCESS_RUN_STATE] = ColorPair(Green, Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Black),
      [PROCESS_NEW] = ColorPair(Black, Green),
      [PROCESS_TOMB] = ColorPair(Black, Red),
      [PROCESS_THREAD] = ColorPair(Green, Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green, Black),
      [PROCESS_COMM] = ColorPair(Magenta, Black),
      [PROCESS_THREAD_COMM] = A_BOLD | ColorPair(Blue, Black),
      [PROCESS_PRIV] = ColorPair(Magenta, Black),
      [BAR_BORDER] = A_BOLD,
      [BAR_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [SWAP] = ColorPair(Red, Black),
      [SWAP_CACHE] = ColorPair(Yellow, Black),
      [SWAP_FRONTSWAP] = A_BOLD | ColorPairGrayBlack,
      [GRAPH_1] = A_BOLD | ColorPair(Cyan, Black),
      [GRAPH_2] = ColorPair(Cyan, Black),
      [MEMORY_USED] = ColorPair(Green, Black),
      [MEMORY_BUFFERS] = A_BOLD | ColorPair(Blue, Black),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [MEMORY_CACHE] = ColorPair(Yellow, Black),
      [MEMORY_SHARED] = ColorPair(Magenta, Black),
      [MEMORY_COMPRESSED] = A_BOLD | ColorPairGrayBlack,
      [HUGEPAGE_1] = ColorPair(Green, Black),
      [HUGEPAGE_2] = ColorPair(Yellow, Black),
      [HUGEPAGE_3] = ColorPair(Red, Black),
      [HUGEPAGE_4] = A_BOLD | ColorPair(Blue, Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Cyan, Black),
      [LOAD_AVERAGE_FIVE] = A_BOLD | ColorPair(Cyan, Black),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White, Black),
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan, Black),
      [HELP_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [CLOCK] = A_BOLD,
      [DATE] = A_BOLD,
      [DATETIME] = A_BOLD,
      [CHECK_BOX] = ColorPair(Cyan, Black),
      [CHECK_MARK] = A_BOLD,
      [CHECK_TEXT] = A_NORMAL,
      [HOSTNAME] = A_BOLD,
      [CPU_NICE] = A_BOLD | ColorPair(Blue, Black),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [CPU_NORMAL] = ColorPair(Green, Black),
      [CPU_SYSTEM] = ColorPair(Red, Black),
      [CPU_IOWAIT] = A_BOLD | ColorPairGrayBlack,
      [CPU_IRQ] = ColorPair(Yellow, Black),
      [CPU_SOFTIRQ] = ColorPair(Magenta, Black),
      [CPU_STEAL] = ColorPair(Cyan, Black),
      [CPU_GUEST] = ColorPair(Cyan, Black),
      [PANEL_EDIT] = ColorPair(White, Blue),
      [SCREENS_OTH_BORDER] = ColorPair(Blue, Blue),
      [SCREENS_OTH_TEXT] = ColorPair(Black, Blue),
      [SCREENS_CUR_BORDER] = ColorPair(Green, Green),
      [SCREENS_CUR_TEXT] = ColorPair(Black, Green),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Cyan, Black),
      [PRESSURE_STALL_SIXTY] = A_BOLD | ColorPair(Cyan, Black),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(White, Black),
      [FILE_DESCRIPTOR_USED] = ColorPair(Green, Black),
      [FILE_DESCRIPTOR_MAX] = A_BOLD | ColorPair(Blue, Black),
      [ZFS_MFU] = A_BOLD | ColorPair(Blue, Black),
      [ZFS_MRU] = ColorPair(Yellow, Black),
      [ZFS_ANON] = ColorPair(Magenta, Black),
      [ZFS_HEADER] = ColorPair(Cyan, Black),
      [ZFS_OTHER] = ColorPair(Magenta, Black),
      [ZFS_COMPRESSED] = A_BOLD | ColorPair(Blue, Black),
      [ZFS_RATIO] = ColorPair(Magenta, Black),
      [ZRAM_COMPRESSED] = A_BOLD | ColorPair(Blue, Black),
      [ZRAM_UNCOMPRESSED] = ColorPair(Yellow, Black),
      [DYNAMIC_GRAY] = ColorPairGrayBlack,
      [DYNAMIC_DARKGRAY] = A_BOLD | ColorPairGrayBlack,
      [DYNAMIC_RED] = ColorPair(Red, Black),
      [DYNAMIC_GREEN] = ColorPair(Green, Black),
      [DYNAMIC_BLUE] = A_BOLD | ColorPair(Blue, Black),
      [DYNAMIC_CYAN] = ColorPair(Cyan, Black),
      [DYNAMIC_MAGENTA] = ColorPair(Magenta, Black),
      [DYNAMIC_YELLOW] = ColorPair(Yellow, Black),
      [DYNAMIC_WHITE] = ColorPair(White, Black),
   },
   [COLORSCHEME_MONOCHROME] = {
      [RESET_COLOR] = A_NORMAL,
      [DEFAULT_COLOR] = A_NORMAL,
      [FUNCTION_BAR] = A_REVERSE,
      [FUNCTION_KEY] = A_NORMAL,
      [PANEL_HEADER_FOCUS] = A_REVERSE,
      [PANEL_HEADER_UNFOCUS] = A_REVERSE,
      [PANEL_SELECTION_FOCUS] = A_REVERSE,
      [PANEL_SELECTION_FOLLOW] = A_REVERSE,
      [PANEL_SELECTION_UNFOCUS] = A_BOLD,
      [FAILED_SEARCH] = A_REVERSE | A_BOLD,
      [FAILED_READ] = A_BOLD,
      [PAUSED] = A_BOLD | A_REVERSE,
      [UPTIME] = A_BOLD,
      [BATTERY] = A_BOLD,
      [LARGE_NUMBER] = A_BOLD,
      [METER_SHADOW] = A_DIM,
      [METER_TEXT] = A_NORMAL,
      [METER_VALUE] = A_BOLD,
      [METER_VALUE_ERROR] = A_BOLD,
      [METER_VALUE_IOREAD] = A_NORMAL,
      [METER_VALUE_IOWRITE] = A_NORMAL,
      [METER_VALUE_NOTICE] = A_BOLD,
      [METER_VALUE_OK] = A_NORMAL,
      [METER_VALUE_WARN] = A_BOLD,
      [LED_COLOR] = A_NORMAL,
      [TASKS_RUNNING] = A_BOLD,
      [PROCESS] = A_NORMAL,
      [PROCESS_SHADOW] = A_DIM,
      [PROCESS_TAG] = A_BOLD,
      [PROCESS_MEGABYTES] = A_BOLD,
      [PROCESS_GIGABYTES] = A_BOLD,
      [PROCESS_BASENAME] = A_BOLD,
      [PROCESS_TREE] = A_BOLD,
      [PROCESS_RUN_STATE] = A_BOLD,
      [PROCESS_D_STATE] = A_BOLD,
      [PROCESS_HIGH_PRIORITY] = A_BOLD,
      [PROCESS_LOW_PRIORITY] = A_DIM,
      [PROCESS_NEW] = A_BOLD,
      [PROCESS_TOMB] = A_DIM,
      [PROCESS_THREAD] = A_BOLD,
      [PROCESS_THREAD_BASENAME] = A_REVERSE,
      [PROCESS_COMM] = A_BOLD,
      [PROCESS_THREAD_COMM] = A_REVERSE,
      [PROCESS_PRIV] = A_BOLD,
      [BAR_BORDER] = A_BOLD,
      [BAR_SHADOW] = A_DIM,
      [SWAP] = A_BOLD,
      [SWAP_CACHE] = A_NORMAL,
      [SWAP_FRONTSWAP] = A_DIM,
      [GRAPH_1] = A_BOLD,
      [GRAPH_2] = A_NORMAL,
      [MEMORY_USED] = A_BOLD,
      [MEMORY_BUFFERS] = A_NORMAL,
      [MEMORY_BUFFERS_TEXT] = A_NORMAL,
      [MEMORY_CACHE] = A_NORMAL,
      [MEMORY_SHARED] = A_NORMAL,
      [MEMORY_COMPRESSED] = A_DIM,
      [HUGEPAGE_1] = A_BOLD,
      [HUGEPAGE_2] = A_NORMAL,
      [HUGEPAGE_3] = A_REVERSE | A_BOLD,
      [HUGEPAGE_4] = A_REVERSE,
      [LOAD_AVERAGE_FIFTEEN] = A_DIM,
      [LOAD_AVERAGE_FIVE] = A_NORMAL,
      [LOAD_AVERAGE_ONE] = A_BOLD,
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD,
      [HELP_SHADOW] = A_DIM,
      [CLOCK] = A_BOLD,
      [DATE] = A_BOLD,
      [DATETIME] = A_BOLD,
      [CHECK_BOX] = A_BOLD,
      [CHECK_MARK] = A_NORMAL,
      [CHECK_TEXT] = A_NORMAL,
      [HOSTNAME] = A_BOLD,
      [CPU_NICE] = A_NORMAL,
      [CPU_NICE_TEXT] = A_NORMAL,
      [CPU_NORMAL] = A_BOLD,
      [CPU_SYSTEM] = A_BOLD,
      [CPU_IOWAIT] = A_NORMAL,
      [CPU_IRQ] = A_BOLD,
      [CPU_SOFTIRQ] = A_BOLD,
      [CPU_STEAL] = A_DIM,
      [CPU_GUEST] = A_DIM,
      [PANEL_EDIT] = A_BOLD,
      [SCREENS_OTH_BORDER] = A_DIM,
      [SCREENS_OTH_TEXT] = A_DIM,
      [SCREENS_CUR_BORDER] = A_REVERSE,
      [SCREENS_CUR_TEXT] = A_REVERSE,
      [PRESSURE_STALL_THREEHUNDRED] = A_DIM,
      [PRESSURE_STALL_SIXTY] = A_NORMAL,
      [PRESSURE_STALL_TEN] = A_BOLD,
      [FILE_DESCRIPTOR_USED] = A_BOLD,
      [FILE_DESCRIPTOR_MAX] = A_BOLD,
      [ZFS_MFU] = A_NORMAL,
      [ZFS_MRU] = A_NORMAL,
      [ZFS_ANON] = A_DIM,
      [ZFS_HEADER] = A_BOLD,
      [ZFS_OTHER] = A_DIM,
      [ZFS_COMPRESSED] = A_BOLD,
      [ZFS_RATIO] = A_BOLD,
      [ZRAM_COMPRESSED] = A_NORMAL,
      [ZRAM_UNCOMPRESSED] = A_NORMAL,
      [DYNAMIC_GRAY] = A_DIM,
      [DYNAMIC_DARKGRAY] = A_DIM,
      [DYNAMIC_RED] = A_BOLD,
      [DYNAMIC_GREEN] = A_NORMAL,
      [DYNAMIC_BLUE] = A_NORMAL,
      [DYNAMIC_CYAN] = A_BOLD,
      [DYNAMIC_MAGENTA] = A_NORMAL,
      [DYNAMIC_YELLOW] = A_NORMAL,
      [DYNAMIC_WHITE] = A_BOLD,
   },
   [COLORSCHEME_BLACKONWHITE] = {
      [RESET_COLOR] = ColorPair(Black, White),
      [DEFAULT_COLOR] = ColorPair(Black, White),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = ColorPair(Black, White),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Blue, White),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = ColorPair(Red, White),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = ColorPair(Yellow, White),
      [BATTERY] = ColorPair(Yellow, White),
      [LARGE_NUMBER] = ColorPair(Red, White),
      [METER_SHADOW] = ColorPair(Blue, White),
      [METER_TEXT] = ColorPair(Blue, White),
      [METER_VALUE] = ColorPair(Black, White),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, White),
      [METER_VALUE_IOREAD] = ColorPair(Green, White),
      [METER_VALUE_IOWRITE] = ColorPair(Yellow, White),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(Yellow, White),
      [METER_VALUE_OK] = ColorPair(Green, White),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, White),
      [LED_COLOR] = ColorPair(Green, White),
      [TASKS_RUNNING] = ColorPair(Green, White),
      [PROCESS] = ColorPair(Black, White),
      [PROCESS_SHADOW] = A_BOLD | ColorPair(Black, White),
      [PROCESS_TAG] = ColorPair(White, Blue),
      [PROCESS_MEGABYTES] = ColorPair(Blue, White),
      [PROCESS_GIGABYTES] = ColorPair(Green, White),
      [PROCESS_BASENAME] = ColorPair(Blue, White),
      [PROCESS_TREE] = ColorPair(Green, White),
      [PROCESS_RUN_STATE] = ColorPair(Green, White),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, White),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, White),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, White),
      [PROCESS_NEW] = ColorPair(White, Green),
      [PROCESS_TOMB] = ColorPair(White, Red),
      [PROCESS_THREAD] = ColorPair(Blue, White),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue, White),
      [PROCESS_COMM] = ColorPair(Magenta, White),
      [PROCESS_THREAD_COMM] = ColorPair(Green, White),
      [PROCESS_PRIV] = ColorPair(Magenta, White),
      [BAR_BORDER] = ColorPair(Blue, White),
      [BAR_SHADOW] = ColorPair(Black, White),
      [SWAP] = ColorPair(Red, White),
      [SWAP_CACHE] = ColorPair(Yellow, White),
      [SWAP_FRONTSWAP] = A_BOLD | ColorPair(Black, White),
      [GRAPH_1] = A_BOLD | ColorPair(Blue, White),
      [GRAPH_2] = ColorPair(Blue, White),
      [MEMORY_USED] = ColorPair(Green, White),
      [MEMORY_BUFFERS] = ColorPair(Cyan, White),
      [MEMORY_BUFFERS_TEXT] = ColorPair(Cyan, White),
      [MEMORY_CACHE] = ColorPair(Yellow, White),
      [MEMORY_SHARED] = ColorPair(Magenta, White),
      [MEMORY_COMPRESSED] = A_BOLD | ColorPair(Black, White),
      [HUGEPAGE_1] = ColorPair(Green, White),
      [HUGEPAGE_2] = ColorPair(Yellow, White),
      [HUGEPAGE_3] = ColorPair(Red, White),
      [HUGEPAGE_4] = ColorPair(Blue, White),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Black, White),
      [LOAD_AVERAGE_FIVE] = ColorPair(Black, White),
      [LOAD_AVERAGE_ONE] = ColorPair(Black, White),
      [LOAD] = ColorPair(Black, White),
      [HELP_BOLD] = ColorPair(Blue, White),
      [HELP_SHADOW] = A_BOLD | ColorPair(Black, White),
      [CLOCK] = ColorPair(Black, White),
      [DATE] = ColorPair(Black, White),
      [DATETIME] = ColorPair(Black, White),
      [CHECK_BOX] = ColorPair(Blue, White),
      [CHECK_MARK] = ColorPair(Black, White),
      [CHECK_TEXT] = ColorPair(Black, White),
      [HOSTNAME] = ColorPair(Black, White),
      [CPU_NICE] = ColorPair(Cyan, White),
      [CPU_NICE_TEXT] = ColorPair(Cyan, White),
      [CPU_NORMAL] = ColorPair(Green, White),
      [CPU_SYSTEM] = ColorPair(Red, White),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black, White),
      [CPU_IRQ] = ColorPair(Blue, White),
      [CPU_SOFTIRQ] = ColorPair(Blue, White),
      [CPU_STEAL] = ColorPair(Cyan, White),
      [CPU_GUEST] = ColorPair(Cyan, White),
      [PANEL_EDIT] = ColorPair(White, Blue),
      [SCREENS_OTH_BORDER] = A_BOLD | ColorPair(Black, White),
      [SCREENS_OTH_TEXT] = A_BOLD | ColorPair(Black, White),
      [SCREENS_CUR_BORDER] = ColorPair(Green, Green),
      [SCREENS_CUR_TEXT] = ColorPair(Black, Green),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Black, White),
      [PRESSURE_STALL_SIXTY] = ColorPair(Black, White),
      [PRESSURE_STALL_TEN] = ColorPair(Black, White),
      [FILE_DESCRIPTOR_USED] = ColorPair(Green, White),
      [FILE_DESCRIPTOR_MAX] = ColorPair(Blue, White),
      [ZFS_MFU] = ColorPair(Cyan, White),
      [ZFS_MRU] = ColorPair(Yellow, White),
      [ZFS_ANON] = ColorPair(Magenta, White),
      [ZFS_HEADER] = ColorPair(Yellow, White),
      [ZFS_OTHER] = ColorPair(Magenta, White),
      [ZFS_COMPRESSED] = ColorPair(Cyan, White),
      [ZFS_RATIO] = ColorPair(Magenta, White),
      [ZRAM_COMPRESSED] = ColorPair(Cyan, White),
      [ZRAM_UNCOMPRESSED] = ColorPair(Yellow, White),
      [DYNAMIC_GRAY] = ColorPair(Black, White),
      [DYNAMIC_DARKGRAY] = A_BOLD | ColorPair(Black, White),
      [DYNAMIC_RED] = ColorPair(Red, White),
      [DYNAMIC_GREEN] = ColorPair(Green, White),
      [DYNAMIC_BLUE] = ColorPair(Blue, White),
      [DYNAMIC_CYAN] = ColorPair(Yellow, White),
      [DYNAMIC_MAGENTA] = ColorPair(Magenta, White),
      [DYNAMIC_YELLOW] = ColorPair(Yellow, White),
      [DYNAMIC_WHITE] = A_BOLD | ColorPair(Black, White),
   },
   [COLORSCHEME_LIGHTTERMINAL] = {
      [RESET_COLOR] = ColorPair(Black, Black),
      [DEFAULT_COLOR] = ColorPair(Black, Black),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = ColorPair(Black, Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Blue, Black),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = ColorPair(Red, Black),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = ColorPair(Yellow, Black),
      [BATTERY] = ColorPair(Yellow, Black),
      [LARGE_NUMBER] = ColorPair(Red, Black),
      [METER_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [METER_TEXT] = ColorPair(Blue, Black),
      [METER_VALUE] = ColorPair(Black, Black),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Black),
      [METER_VALUE_IOREAD] = ColorPair(Green, Black),
      [METER_VALUE_IOWRITE] = ColorPair(Yellow, Black),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPairWhiteDefault,
      [METER_VALUE_OK] = ColorPair(Green, Black),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Black),
      [TASKS_RUNNING] = ColorPair(Green, Black),
      [PROCESS] = ColorPair(Black, Black),
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = ColorPair(White, Blue),
      [PROCESS_MEGABYTES] = ColorPair(Blue, Black),
      [PROCESS_GIGABYTES] = ColorPair(Green, Black),
      [PROCESS_BASENAME] = ColorPair(Green, Black),
      [PROCESS_TREE] = ColorPair(Blue, Black),
      [PROCESS_RUN_STATE] = ColorPair(Green, Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Black),
      [PROCESS_NEW] = ColorPair(Black, Green),
      [PROCESS_TOMB] = ColorPair(Black, Red),
      [PROCESS_THREAD] = ColorPair(Blue, Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue, Black),
      [PROCESS_COMM] = ColorPair(Magenta, Black),
      [PROCESS_THREAD_COMM] = ColorPair(Yellow, Black),
      [PROCESS_PRIV] = ColorPair(Magenta, Black),
      [BAR_BORDER] = ColorPair(Blue, Black),
      [BAR_SHADOW] = ColorPairGrayBlack,
      [SWAP] = ColorPair(Red, Black),
      [SWAP_CACHE] = ColorPair(Yellow, Black),
      [SWAP_FRONTSWAP] = ColorPairGrayBlack,
      [GRAPH_1] = A_BOLD | ColorPair(Cyan, Black),
      [GRAPH_2] = ColorPair(Cyan, Black),
      [MEMORY_USED] = ColorPair(Green, Black),
      [MEMORY_BUFFERS] = ColorPair(Cyan, Black),
      [MEMORY_BUFFERS_TEXT] = ColorPair(Cyan, Black),
      [MEMORY_CACHE] = ColorPair(Yellow, Black),
      [MEMORY_SHARED] = ColorPair(Magenta, Black),
      [MEMORY_COMPRESSED] = ColorPairGrayBlack,
      [HUGEPAGE_1] = ColorPair(Green, Black),
      [HUGEPAGE_2] = ColorPair(Yellow, Black),
      [HUGEPAGE_3] = ColorPair(Red, Black),
      [HUGEPAGE_4] = ColorPair(Blue, Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Black, Black),
      [LOAD_AVERAGE_FIVE] = ColorPair(Black, Black),
      [LOAD_AVERAGE_ONE] = ColorPair(Black, Black),
      [LOAD] = ColorPairWhiteDefault,
      [HELP_BOLD] = ColorPair(Blue, Black),
      [HELP_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [CLOCK] = ColorPairWhiteDefault,
      [DATE] = ColorPairWhiteDefault,
      [DATETIME] = ColorPairWhiteDefault,
      [CHECK_BOX] = ColorPair(Blue, Black),
      [CHECK_MARK] = ColorPair(Black, Black),
      [CHECK_TEXT] = ColorPair(Black, Black),
      [HOSTNAME] = ColorPairWhiteDefault,
      [CPU_NICE] = ColorPair(Cyan, Black),
      [CPU_NICE_TEXT] = ColorPair(Cyan, Black),
      [CPU_NORMAL] = ColorPair(Green, Black),
      [CPU_SYSTEM] = ColorPair(Red, Black),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black, Black),
      [CPU_IRQ] = A_BOLD | ColorPair(Blue, Black),
      [CPU_SOFTIRQ] = ColorPair(Blue, Black),
      [CPU_STEAL] = ColorPair(Black, Black),
      [CPU_GUEST] = ColorPair(Black, Black),
      [PANEL_EDIT] = ColorPair(White, Blue),
      [SCREENS_OTH_BORDER] = ColorPair(Blue, Black),
      [SCREENS_OTH_TEXT] = ColorPair(Blue, Black),
      [SCREENS_CUR_BORDER] = ColorPair(Green, Green),
      [SCREENS_CUR_TEXT] = ColorPair(Black, Green),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Black, Black),
      [PRESSURE_STALL_SIXTY] = ColorPair(Black, Black),
      [PRESSURE_STALL_TEN] = ColorPair(Black, Black),
      [FILE_DESCRIPTOR_USED] = ColorPair(Green, Black),
      [FILE_DESCRIPTOR_MAX] = A_BOLD | ColorPair(Blue, Black),
      [ZFS_MFU] = ColorPair(Cyan, Black),
      [ZFS_MRU] = ColorPair(Yellow, Black),
      [ZFS_ANON] = A_BOLD | ColorPair(Magenta, Black),
      [ZFS_HEADER] = ColorPair(Black, Black),
      [ZFS_OTHER] = A_BOLD | ColorPair(Magenta, Black),
      [ZFS_COMPRESSED] = ColorPair(Cyan, Black),
      [ZFS_RATIO] = A_BOLD | ColorPair(Magenta, Black),
      [ZRAM_COMPRESSED] = ColorPair(Cyan, Black),
      [ZRAM_UNCOMPRESSED] = ColorPair(Yellow, Black),
      [DYNAMIC_GRAY] = ColorPairGrayBlack,
      [DYNAMIC_DARKGRAY] = A_BOLD | ColorPairGrayBlack,
      [DYNAMIC_RED] = ColorPair(Red, Black),
      [DYNAMIC_GREEN] = ColorPair(Green, Black),
      [DYNAMIC_BLUE] = ColorPair(Blue, Black),
      [DYNAMIC_CYAN] = ColorPair(Cyan, Black),
      [DYNAMIC_MAGENTA] = ColorPair(Magenta, Black),
      [DYNAMIC_YELLOW] = ColorPair(Yellow, Black),
      [DYNAMIC_WHITE] = ColorPairWhiteDefault,
   },
   [COLORSCHEME_MIDNIGHT] = {
      [RESET_COLOR] = ColorPair(White, Blue),
      [DEFAULT_COLOR] = ColorPair(White, Blue),
      [FUNCTION_BAR] = ColorPair(Black, Cyan),
      [FUNCTION_KEY] = A_NORMAL,
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, White),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = A_BOLD | ColorPair(Yellow, Blue),
      [FAILED_SEARCH] = ColorPair(Red, Cyan),
      [FAILED_READ] = A_BOLD | ColorPair(Red, Blue),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Cyan),
      [UPTIME] = A_BOLD | ColorPair(Yellow, Blue),
      [BATTERY] = A_BOLD | ColorPair(Yellow, Blue),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red, Blue),
      [METER_SHADOW] = ColorPair(Cyan, Blue),
      [METER_TEXT] = ColorPair(Cyan, Blue),
      [METER_VALUE] = A_BOLD | ColorPair(Cyan, Blue),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Blue),
      [METER_VALUE_IOREAD] = ColorPair(Green, Blue),
      [METER_VALUE_IOWRITE] = ColorPair(Black, Blue),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(White, Blue),
      [METER_VALUE_OK] = ColorPair(Green, Blue),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Blue),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green, Blue),
      [PROCESS] = ColorPair(White, Blue),
      [PROCESS_SHADOW] = A_BOLD | ColorPair(Black, Blue),
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow, Blue),
      [PROCESS_MEGABYTES] = ColorPair(Cyan, Blue),
      [PROCESS_GIGABYTES] = ColorPair(Green, Blue),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan, Blue),
      [PROCESS_TREE] = ColorPair(Cyan, Blue),
      [PROCESS_RUN_STATE] = ColorPair(Green, Blue),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Blue),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Blue),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Blue),
      [PROCESS_NEW] = ColorPair(Blue, Green),
      [PROCESS_TOMB] = ColorPair(Blue, Red),
      [PROCESS_THREAD] = ColorPair(Green, Blue),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green, Blue),
      [PROCESS_COMM] = ColorPair(Magenta, Blue),
      [PROCESS_THREAD_COMM] = ColorPair(Black, Blue),
      [PROCESS_PRIV] = ColorPair(Magenta, Blue),
      [BAR_BORDER] = A_BOLD | ColorPair(Yellow, Blue),
      [BAR_SHADOW] = ColorPair(Cyan, Blue),
      [SWAP] = ColorPair(Red, Blue),
      [SWAP_CACHE] = A_BOLD | ColorPair(Yellow, Blue),
      [SWAP_FRONTSWAP] = A_BOLD | ColorPair(Black, Blue),
      [GRAPH_1] = A_BOLD | ColorPair(Cyan, Blue),
      [GRAPH_2] = ColorPair(Cyan, Blue),
      [MEMORY_USED] = A_BOLD | ColorPair(Green, Blue),
      [MEMORY_BUFFERS] = A_BOLD | ColorPair(Cyan, Blue),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Cyan, Blue),
      [MEMORY_CACHE] = A_BOLD | ColorPair(Yellow, Blue),
      [MEMORY_SHARED] = A_BOLD | ColorPair(Magenta, Blue),
      [MEMORY_COMPRESSED] = A_BOLD | ColorPair(Black, Blue),
      [HUGEPAGE_1] = A_BOLD | ColorPair(Green, Blue),
      [HUGEPAGE_2] = A_BOLD | ColorPair(Yellow, Blue),
      [HUGEPAGE_3] = A_BOLD | ColorPair(Red, Blue),
      [HUGEPAGE_4] = A_BOLD | ColorPair(White, Blue),
      [LOAD_AVERAGE_FIFTEEN] = A_BOLD | ColorPair(Black, Blue),
      [LOAD_AVERAGE_FIVE] = A_NORMAL | ColorPair(White, Blue),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White, Blue),
      [LOAD] = A_BOLD | ColorPair(White, Blue),
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan, Blue),
      [HELP_SHADOW] = A_BOLD | ColorPair(Black, Blue),
      [CLOCK] = ColorPair(White, Blue),
      [DATE] = ColorPair(White, Blue),
      [DATETIME] = ColorPair(White, Blue),
      [CHECK_BOX] = ColorPair(Cyan, Blue),
      [CHECK_MARK] = A_BOLD | ColorPair(White, Blue),
      [CHECK_TEXT] = A_NORMAL | ColorPair(White, Blue),
      [HOSTNAME] = ColorPair(White, Blue),
      [CPU_NICE] = A_BOLD | ColorPair(Cyan, Blue),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Cyan, Blue),
      [CPU_NORMAL] = A_BOLD | ColorPair(Green, Blue),
      [CPU_SYSTEM] = A_BOLD | ColorPair(Red, Blue),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black, Blue),
      [CPU_IRQ] = A_BOLD | ColorPair(Black, Blue),
      [CPU_SOFTIRQ] = ColorPair(Black, Blue),
      [CPU_STEAL] = ColorPair(White, Blue),
      [CPU_GUEST] = ColorPair(White, Blue),
      [PANEL_EDIT] = ColorPair(White, Blue),
      [SCREENS_OTH_BORDER] = A_BOLD | ColorPair(Yellow, Blue),
      [SCREENS_OTH_TEXT] = ColorPair(Cyan, Blue),
      [SCREENS_CUR_BORDER] = ColorPair(Cyan, Cyan),
      [SCREENS_CUR_TEXT] = ColorPair(Black, Cyan),
      [PRESSURE_STALL_THREEHUNDRED] = A_BOLD | ColorPair(Black, Blue),
      [PRESSURE_STALL_SIXTY] = A_NORMAL | ColorPair(White, Blue),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(White, Blue),
      [FILE_DESCRIPTOR_USED] = A_BOLD | ColorPair(Green, Blue),
      [FILE_DESCRIPTOR_MAX] = A_BOLD | ColorPair(Red, Blue),
      [ZFS_MFU] = A_BOLD | ColorPair(White, Blue),
      [ZFS_MRU] = A_BOLD | ColorPair(Yellow, Blue),
      [ZFS_ANON] = A_BOLD | ColorPair(Magenta, Blue),
      [ZFS_HEADER] = A_BOLD | ColorPair(Yellow, Blue),
      [ZFS_OTHER] = A_BOLD | ColorPair(Magenta, Blue),
      [ZFS_COMPRESSED] = A_BOLD | ColorPair(White, Blue),
      [ZFS_RATIO] = A_BOLD | ColorPair(Magenta, Blue),
      [ZRAM_COMPRESSED] = ColorPair(Cyan, Blue),
      [ZRAM_UNCOMPRESSED] = ColorPair(Yellow, Blue),
      [DYNAMIC_GRAY] = ColorPairGrayBlack,
      [DYNAMIC_DARKGRAY] = A_BOLD | ColorPairGrayBlack,
      [DYNAMIC_RED] = ColorPair(Red, Blue),
      [DYNAMIC_GREEN] = ColorPair(Green, Blue),
      [DYNAMIC_BLUE] = ColorPair(Black, Blue),
      [DYNAMIC_CYAN] = ColorPair(Cyan, Blue),
      [DYNAMIC_MAGENTA] = ColorPair(Magenta, Blue),
      [DYNAMIC_YELLOW] = ColorPair(Yellow, Blue),
      [DYNAMIC_WHITE] = ColorPair(White, Blue),
   },
   [COLORSCHEME_BLACKNIGHT] = {
      [RESET_COLOR] = ColorPair(Cyan, Black),
      [DEFAULT_COLOR] = ColorPair(Cyan, Black),
      [FUNCTION_BAR] = ColorPair(Black, Green),
      [FUNCTION_KEY] = ColorPair(Cyan, Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black, Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black, Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black, Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black, Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Black, White),
      [FAILED_SEARCH] = ColorPair(Red, Green),
      [FAILED_READ] = A_BOLD | ColorPair(Red, Black),
      [PAUSED] = A_BOLD | ColorPair(Yellow, Green),
      [UPTIME] = ColorPair(Green, Black),
      [BATTERY] = ColorPair(Green, Black),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red, Black),
      [METER_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [METER_TEXT] = ColorPair(Cyan, Black),
      [METER_VALUE] = ColorPair(Green, Black),
      [METER_VALUE_ERROR] = A_BOLD | ColorPair(Red, Black),
      [METER_VALUE_IOREAD] = ColorPair(Green, Black),
      [METER_VALUE_IOWRITE] = ColorPair(Blue, Black),
      [METER_VALUE_NOTICE] = A_BOLD | ColorPair(White, Black),
      [METER_VALUE_OK] = ColorPair(Green, Black),
      [METER_VALUE_WARN] = A_BOLD | ColorPair(Yellow, Black),
      [LED_COLOR] = ColorPair(Green, Black),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green, Black),
      [PROCESS] = ColorPair(Cyan, Black),
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow, Black),
      [PROCESS_MEGABYTES] = A_BOLD | ColorPair(Green, Black),
      [PROCESS_GIGABYTES] = A_BOLD | ColorPair(Yellow, Black),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Green, Black),
      [PROCESS_TREE] = ColorPair(Cyan, Black),
      [PROCESS_THREAD] = ColorPair(Green, Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue, Black),
      [PROCESS_COMM] = ColorPair(Magenta, Black),
      [PROCESS_THREAD_COMM] = ColorPair(Yellow, Black),
      [PROCESS_RUN_STATE] = ColorPair(Green, Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red, Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red, Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green, Black),
      [PROCESS_NEW] = ColorPair(Black, Green),
      [PROCESS_TOMB] = ColorPair(Black, Red),
      [PROCESS_PRIV] = ColorPair(Magenta, Black),
      [BAR_BORDER] = A_BOLD | ColorPair(Green, Black),
      [BAR_SHADOW] = ColorPair(Cyan, Black),
      [SWAP] = ColorPair(Red, Black),
      [SWAP_CACHE] = ColorPair(Yellow, Black),
      [SWAP_FRONTSWAP] = ColorPair(Yellow, Black),
      [GRAPH_1] = A_BOLD | ColorPair(Green, Black),
      [GRAPH_2] = ColorPair(Green, Black),
      [MEMORY_USED] = ColorPair(Green, Black),
      [MEMORY_BUFFERS] = ColorPair(Blue, Black),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [MEMORY_CACHE] = ColorPair(Yellow, Black),
      [MEMORY_SHARED] = ColorPair(Magenta, Black),
      [MEMORY_COMPRESSED] = ColorPair(Yellow, Black),
      [HUGEPAGE_1] = ColorPair(Green, Black),
      [HUGEPAGE_2] = ColorPair(Yellow, Black),
      [HUGEPAGE_3] = ColorPair(Red, Black),
      [HUGEPAGE_4] = ColorPair(Blue, Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Green, Black),
      [LOAD_AVERAGE_FIVE] = ColorPair(Green, Black),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(Green, Black),
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan, Black),
      [HELP_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [CLOCK] = ColorPair(Green, Black),
      [CHECK_BOX] = ColorPair(Green, Black),
      [CHECK_MARK] = A_BOLD | ColorPair(Green, Black),
      [CHECK_TEXT] = ColorPair(Cyan, Black),
      [HOSTNAME] = ColorPair(Green, Black),
      [CPU_NICE] = ColorPair(Blue, Black),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue, Black),
      [CPU_NORMAL] = ColorPair(Green, Black),
      [CPU_SYSTEM] = ColorPair(Red, Black),
      [CPU_IOWAIT] = ColorPair(Yellow, Black),
      [CPU_IRQ] = A_BOLD | ColorPair(Blue, Black),
      [CPU_SOFTIRQ] = ColorPair(Blue, Black),
      [CPU_STEAL] = ColorPair(Cyan, Black),
      [CPU_GUEST] = ColorPair(Cyan, Black),
      [PANEL_EDIT] = ColorPair(White, Cyan),
      [SCREENS_OTH_BORDER] = ColorPair(White, Black),
      [SCREENS_OTH_TEXT] = ColorPair(Cyan, Black),
      [SCREENS_CUR_BORDER] = A_BOLD | ColorPair(White, Black),
      [SCREENS_CUR_TEXT] = A_BOLD | ColorPair(Green, Black),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Green, Black),
      [PRESSURE_STALL_SIXTY] = ColorPair(Green, Black),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(Green, Black),
      [FILE_DESCRIPTOR_USED] = ColorPair(Green, Black),
      [FILE_DESCRIPTOR_MAX] = A_BOLD | ColorPair(Blue, Black),
      [ZFS_MFU] = ColorPair(Blue, Black),
      [ZFS_MRU] = ColorPair(Yellow, Black),
      [ZFS_ANON] = ColorPair(Magenta, Black),
      [ZFS_HEADER] = ColorPair(Yellow, Black),
      [ZFS_OTHER] = ColorPair(Magenta, Black),
      [ZFS_COMPRESSED] = ColorPair(Blue, Black),
      [ZFS_RATIO] = ColorPair(Magenta, Black),
      [ZRAM_COMPRESSED] = ColorPair(Blue, Black),
      [ZRAM_UNCOMPRESSED] = ColorPair(Yellow, Black),
      [DYNAMIC_GRAY] = ColorPairGrayBlack,
      [DYNAMIC_DARKGRAY] = A_BOLD | ColorPairGrayBlack,
      [DYNAMIC_RED] = ColorPair(Red, Black),
      [DYNAMIC_GREEN] = ColorPair(Green, Black),
      [DYNAMIC_BLUE] = ColorPair(Blue, Black),
      [DYNAMIC_CYAN] = ColorPair(Cyan, Black),
      [DYNAMIC_MAGENTA] = ColorPair(Magenta, Black),
      [DYNAMIC_YELLOW] = ColorPair(Yellow, Black),
      [DYNAMIC_WHITE] = ColorPair(White, Black),
   },
   [COLORSCHEME_BROKENGRAY] = { 0 } // dynamically generated.
};

static bool CRT_retainScreenOnExit = false;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

ColorScheme CRT_colorScheme = COLORSCHEME_DEFAULT;

ATTR_NORETURN
static void CRT_handleSIGTERM(ATTR_UNUSED int sgn) {
   CRT_done();
   _exit(0);
}

#ifndef NDEBUG

static int stderrRedirectNewFd = -1;
static int stderrRedirectBackupFd = -1;

static int createStderrCacheFile(void) {
#if defined(HAVE_MEMFD_CREATE)
   return memfd_create("htop.stderr-redirect", 0);
#elif defined(O_TMPFILE)
   return open("/tmp", O_TMPFILE | O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
#else
   char tmpName[] = "htop.stderr-redirectXXXXXX";
   mode_t curUmask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
   int r = mkstemp(tmpName);
   umask(curUmask);
   if (r < 0)
      return r;

   (void) unlink(tmpName);

   return r;
#endif /* HAVE_MEMFD_CREATE */
}

static void redirectStderr(void) {
   stderrRedirectNewFd = createStderrCacheFile();
   if (stderrRedirectNewFd < 0) {
      /* ignore failure */
      return;
   }

   stderrRedirectBackupFd = dup(STDERR_FILENO);
   dup2(stderrRedirectNewFd, STDERR_FILENO);
}

static void dumpStderr(void) {
   if (stderrRedirectNewFd < 0)
      return;

   fsync(STDERR_FILENO);
   dup2(stderrRedirectBackupFd, STDERR_FILENO);
   close(stderrRedirectBackupFd);
   stderrRedirectBackupFd = -1;
   lseek(stderrRedirectNewFd, 0, SEEK_SET);

   bool header = false;
   char buffer[8192];
   for (;;) {
      errno = 0;
      ssize_t res = read(stderrRedirectNewFd, buffer, sizeof(buffer));
      if (res < 0) {
         if (errno == EINTR)
            continue;

         break;
      }

      if (res == 0) {
         break;
      }

      if (res > 0) {
         if (!header) {
            fprintf(stderr, ">>>>>>>>>> stderr output >>>>>>>>>>\n");
            header = true;
         }
         full_write(STDERR_FILENO, buffer, res);
      }
   }

   if (header)
      fprintf(stderr, "\n<<<<<<<<<< stderr output <<<<<<<<<<\n");

   close(stderrRedirectNewFd);
   stderrRedirectNewFd = -1;
}

void CRT_debug_impl(const char* file, size_t lineno, const char* func, const char* fmt, ...)  {
   va_list args;

   fprintf(stderr, "[%s:%zu (%s)]: ", file, lineno, func);
   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
   fprintf(stderr, "\n");
}

#else /* !NDEBUG */

static void redirectStderr(void) {
}

static void dumpStderr(void) {
}

#endif /* !NDEBUG */

static struct sigaction old_sig_handler[32];

static void CRT_installSignalHandlers(void) {
   struct sigaction act;
   sigemptyset(&act.sa_mask);
   act.sa_flags = (int)SA_RESETHAND | SA_NODEFER;
   act.sa_handler = CRT_handleSIGSEGV;
   sigaction(SIGSEGV, &act, &old_sig_handler[SIGSEGV]);
   sigaction(SIGFPE, &act, &old_sig_handler[SIGFPE]);
   sigaction(SIGILL, &act, &old_sig_handler[SIGILL]);
   sigaction(SIGBUS, &act, &old_sig_handler[SIGBUS]);
   sigaction(SIGPIPE, &act, &old_sig_handler[SIGPIPE]);
   sigaction(SIGSYS, &act, &old_sig_handler[SIGSYS]);
   sigaction(SIGABRT, &act, &old_sig_handler[SIGABRT]);

   signal(SIGCHLD, SIG_DFL);
   signal(SIGINT, CRT_handleSIGTERM);
   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);
}

void CRT_resetSignalHandlers(void) {
   sigaction(SIGSEGV, &old_sig_handler[SIGSEGV], NULL);
   sigaction(SIGFPE, &old_sig_handler[SIGFPE], NULL);
   sigaction(SIGILL, &old_sig_handler[SIGILL], NULL);
   sigaction(SIGBUS, &old_sig_handler[SIGBUS], NULL);
   sigaction(SIGPIPE, &old_sig_handler[SIGPIPE], NULL);
   sigaction(SIGSYS, &old_sig_handler[SIGSYS], NULL);
   sigaction(SIGABRT, &old_sig_handler[SIGABRT], NULL);

   signal(SIGINT, SIG_DFL);
   signal(SIGTERM, SIG_DFL);
   signal(SIGQUIT, SIG_DFL);
}

#ifdef HAVE_GETMOUSE
void CRT_setMouse(bool enabled) {
   if (enabled) {
#if NCURSES_MOUSE_VERSION > 1
      mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
#else
      mousemask(BUTTON1_RELEASED, NULL);
#endif
   } else {
      mousemask(0, NULL);
   }
}
#endif

void CRT_init(const Settings* settings, bool allowUnicode, bool retainScreenOnExit) {
   initscr();

   if (retainScreenOnExit) {
      CRT_retainScreenOnExit = true;
      refresh();
      tputs(exit_ca_mode, 0, putchar);
      tputs(clear_screen, 0, putchar);
      fflush(stdout);
      enter_ca_mode = 0;
      exit_ca_mode = 0;
   }

   redirectStderr();
   noecho();
   CRT_crashSettings = settings;
   CRT_delay = &(settings->delay);
   CRT_colors = CRT_colorSchemes[settings->colorScheme];
   CRT_colorScheme = settings->colorScheme;

   for (int i = 0; i < LAST_COLORELEMENT; i++) {
      unsigned int color = CRT_colorSchemes[COLORSCHEME_DEFAULT][i];
      CRT_colorSchemes[COLORSCHEME_BROKENGRAY][i] = color == (A_BOLD | ColorPairGrayBlack) ? ColorPair(White, Black) : color;
   }

   halfdelay(*CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
#ifdef HAVE_GETMOUSE
   mouseinterval(0);
#endif
   curs_set(0);

   if (has_colors()) {
      start_color();
   }

   const char* termType = getenv("TERM");
   if (termType && String_eq(termType, "linux")) {
      CRT_scrollHAmount = 20;
   } else {
      CRT_scrollHAmount = 5;
   }

   if (termType && (String_startsWith(termType, "xterm") || String_eq(termType, "vt220"))) {
#ifdef HTOP_NETBSD
#define define_key(s_, k_) define_key((char*)s_, k_)
IGNORE_WCASTQUAL_BEGIN
#endif
      define_key("\033[H", KEY_HOME);
      define_key("\033[F", KEY_END);
      define_key("\033[7~", KEY_HOME);
      define_key("\033[8~", KEY_END);
      define_key("\033OP", KEY_F(1));
      define_key("\033OQ", KEY_F(2));
      define_key("\033OR", KEY_F(3));
      define_key("\033OS", KEY_F(4));
      define_key("\033O2R", KEY_F(15));
      define_key("\033[11~", KEY_F(1));
      define_key("\033[12~", KEY_F(2));
      define_key("\033[13~", KEY_F(3));
      define_key("\033[14~", KEY_F(4));
      define_key("\033[14;2~", KEY_F(15));
      define_key("\033[17;2~", KEY_F(18));
      define_key("\033[Z", KEY_SHIFT_TAB);
      char sequence[3] = "\033a";
      for (char c = 'a'; c <= 'z'; c++) {
         sequence[1] = c;
         define_key(sequence, KEY_ALT('A' + (c - 'a')));
      }
#ifdef HTOP_NETBSD
IGNORE_WCASTQUAL_END
#undef define_key
#endif
   }
   if (termType && (String_startsWith(termType, "rxvt"))) {
      define_key("\033[Z", KEY_SHIFT_TAB);
   }

   CRT_installSignalHandlers();

   use_default_colors();
   if (!has_colors())
      CRT_colorScheme = COLORSCHEME_MONOCHROME;
   CRT_setColors(CRT_colorScheme);

#ifdef HAVE_LIBNCURSESW
   if (allowUnicode && String_eq(nl_langinfo(CODESET), "UTF-8")) {
      CRT_utf8 = true;
   } else {
      CRT_utf8 = false;
   }
#else
   (void) allowUnicode;
#endif

   CRT_treeStr =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? CRT_treeStrUtf8 :
#endif
      CRT_treeStrAscii;

   CRT_setMouse(settings->enableMouse);

   CRT_degreeSign = initDegreeSign();
}

void CRT_done(void) {
   int resetColor = CRT_colors ? CRT_colors[RESET_COLOR] : CRT_colorSchemes[COLORSCHEME_DEFAULT][RESET_COLOR];

   attron(resetColor);
   mvhline(LINES - 1, 0, ' ', COLS);
   attroff(resetColor);
   refresh();

   if (CRT_retainScreenOnExit) {
      mvcur(-1, -1, LINES - 1, 0);
   }

   curs_set(1);
   endwin();

   dumpStderr();
}

void CRT_fatalError(const char* note) {
   const char* sysMsg = strerror(errno);
   CRT_done();
   fprintf(stderr, "%s: %s\n", note, sysMsg);
   exit(2);
}

int CRT_readKey(void) {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int ret = getch();
   halfdelay(*CRT_delay);
   return ret;
}

void CRT_disableDelay(void) {
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay(void) {
   halfdelay(*CRT_delay);
}

void CRT_setColors(int colorScheme) {
   CRT_colorScheme = colorScheme;

   for (short int i = 0; i < 8; i++) {
      for (short int j = 0; j < 8; j++) {
         if (ColorIndex(i, j) != ColorIndexGrayBlack && ColorIndex(i, j) != ColorIndexWhiteDefault) {
            short int bg = (colorScheme != COLORSCHEME_BLACKNIGHT) && (j == 0) ? -1 : j;
            init_pair(ColorIndex(i, j), i, bg);
         }
      }
   }

   short int grayBlackFg = COLORS > 8 ? 8 : 0;
   short int grayBlackBg = (colorScheme != COLORSCHEME_BLACKNIGHT) ? -1 : 0;
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   init_pair(ColorIndexWhiteDefault, White, -1);

   CRT_colors = CRT_colorSchemes[colorScheme];
}

#ifdef PRINT_BACKTRACE
static void print_backtrace(void) {
#if defined(HAVE_LIBUNWIND_H) && defined(HAVE_LIBUNWIND)
   unw_context_t context;
   unw_getcontext(&context);

   unw_cursor_t cursor;
   unw_init_local(&cursor, &context);

   unsigned int item = 0;

   while (unw_step(&cursor) > 0) {
      unw_word_t pc;
      unw_get_reg(&cursor, UNW_REG_IP, &pc);
      if (pc == 0)
         break;

      char symbolName[256] = "?";
      unw_word_t offset = 0;
      unw_get_proc_name(&cursor, symbolName, sizeof(symbolName), &offset);

      unw_proc_info_t pip;
      pip.unwind_info = 0;

      const char* fname = "?";
      const void* ptr = 0;
      if (unw_get_proc_info(&cursor, &pip) == 0) {
         ptr = (const void*)(pip.start_ip + offset);

         #ifdef HAVE_DLADDR
         Dl_info dlinfo;
         if (dladdr(ptr, &dlinfo) && dlinfo.dli_fname && *dlinfo.dli_fname)
            fname = dlinfo.dli_fname;
         #endif
      }

      const bool is_signal_frame = unw_is_signal_frame(&cursor) > 0;
      const char* frame = is_signal_frame ? "  {signal frame}" : "";

      fprintf(stderr, "%2u: %#14lx  %s  (%s+%#lx)  [%p]%s\n", item++, pc, fname, symbolName, offset, ptr, frame);
   }
#elif defined(HAVE_EXECINFO_H)
   void* backtraceArray[256];

   size_t size = backtrace(backtraceArray, ARRAYSIZE(backtraceArray));
   backtrace_symbols_fd(backtraceArray, size, STDERR_FILENO);
#else
#error No implementation for print_backtrace()!
#endif
}
#endif

void CRT_handleSIGSEGV(int signal) {
   CRT_done();

   fprintf(stderr, "\n\n"
      "FATAL PROGRAM ERROR DETECTED\n"
      "============================\n"
      "Please check at https://htop.dev/issues whether this issue has already been reported.\n"
      "If no similar issue has been reported before, please create a new issue with the following information:\n"
      "  - Your %s version: '"VERSION"'\n"
      "  - Your OS and kernel version (uname -a)\n"
      "  - Your distribution and release (lsb_release -a)\n"
      "  - Likely steps to reproduce (How did it happen?)\n",
      program
   );

#ifdef PRINT_BACKTRACE
   fprintf(stderr, "  - Backtrace of the issue (see below)\n");
#endif

   fprintf(stderr,
      "\n"
   );

   const char* signal_str = strsignal(signal);
   if (!signal_str) {
      signal_str = "unknown reason";
   }
   fprintf(stderr,
      "Error information:\n"
      "------------------\n"
      "A signal %d (%s) was received.\n"
      "\n",
      signal, signal_str
   );

   fprintf(stderr,
      "Setting information:\n"
      "--------------------\n");
   Settings_write(CRT_crashSettings, true);
   fprintf(stderr, "\n\n");

#ifdef PRINT_BACKTRACE
   fprintf(stderr,
      "Backtrace information:\n"
      "----------------------\n"
   );

   print_backtrace();

   fprintf(stderr,
      "\n"
      "To make the above information more practical to work with, "
      "please also provide a disassembly of your %s binary. "
      "This can usually be done by running the following command:\n"
      "\n",
      program
   );

#ifdef HTOP_DARWIN
   fprintf(stderr, "   otool -tvV `which %s` > ~/%s.otool\n", program, program);
#else
   fprintf(stderr, "   objdump -d -S -w `which %s` > ~/%s.objdump\n", program, program);
#endif

   fprintf(stderr,
      "\n"
      "Please include the generated file in your report.\n"
   );
#endif

   fprintf(stderr,
      "Running this program with debug symbols or inside a debugger may provide further insights.\n"
      "\n"
      "Thank you for helping to improve %s!\n"
      "\n",
      program
   );

   /* Call old sigsegv handler; may be default exit or third party one (e.g. ASAN) */
   if (sigaction(signal, &old_sig_handler[signal], NULL) < 0) {
      /* This avoids an infinite loop in case the handler could not be reset. */
      fprintf(stderr,
         "!!! Chained handler could not be restored. Forcing exit.\n"
      );
      _exit(1);
   }

   /* Trigger the previous signal handler. */
   raise(signal);

   // Always terminate, even if installed handler returns
   fprintf(stderr,
      "!!! Chained handler did not exit. Forcing exit.\n"
   );
   _exit(1);
}
