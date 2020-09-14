/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "CRT.h"

#include "StringUtils.h"
#include "RichString.h"

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
#if HAVE_SETUID_ENABLED
#include <unistd.h>
#include <sys/types.h>
#endif

#define ColorIndex(i,j) ((7-i)*8+j)

#define ColorPair(i,j) COLOR_PAIR(ColorIndex(i,j))

#define Black COLOR_BLACK
#define Red COLOR_RED
#define Green COLOR_GREEN
#define Yellow COLOR_YELLOW
#define Blue COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan COLOR_CYAN
#define White COLOR_WHITE

#define ColorPairGrayBlack ColorPair(Magenta,Magenta)
#define ColorIndexGrayBlack ColorIndex(Magenta,Magenta)

const char *CRT_treeStrAscii[TREE_STR_COUNT] = {
   "-", // TREE_STR_HORZ
   "|", // TREE_STR_VERT
   "`", // TREE_STR_RTEE
   "`", // TREE_STR_BEND
   ",", // TREE_STR_TEND
   "+", // TREE_STR_OPEN
   "-", // TREE_STR_SHUT
};

#ifdef HAVE_LIBNCURSESW

const char *CRT_treeStrUtf8[TREE_STR_COUNT] = {
   "\xe2\x94\x80", // TREE_STR_HORZ ─
   "\xe2\x94\x82", // TREE_STR_VERT │
   "\xe2\x94\x9c", // TREE_STR_RTEE ├
   "\xe2\x94\x94", // TREE_STR_BEND └
   "\xe2\x94\x8c", // TREE_STR_TEND ┌
   "+",            // TREE_STR_OPEN +, TODO use 🮯 'BOX DRAWINGS LIGHT HORIZONTAL
                   // WITH VERTICAL STROKE' (U+1FBAF, "\xf0\x9f\xae\xaf") when
                   // Unicode 13 is common
   "\xe2\x94\x80", // TREE_STR_SHUT ─
};

bool CRT_utf8 = false;

#endif

const char **CRT_treeStr = CRT_treeStrAscii;

static bool CRT_hasColors;

int CRT_delay = 0;

int* CRT_colors;

int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT] = {
   [COLORSCHEME_DEFAULT] = {
      [RESET_COLOR] = ColorPair(White,Black),
      [DEFAULT_COLOR] = ColorPair(White,Black),
      [FUNCTION_BAR] = ColorPair(Black,Cyan),
      [FUNCTION_KEY] = ColorPair(White,Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Black,White),
      [FAILED_SEARCH] = ColorPair(Red,Cyan),
      [UPTIME] = A_BOLD | ColorPair(Cyan,Black),
      [BATTERY] = A_BOLD | ColorPair(Cyan,Black),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red,Black),
      [METER_TEXT] = ColorPair(Cyan,Black),
      [METER_VALUE] = A_BOLD | ColorPair(Cyan,Black),
      [LED_COLOR] = ColorPair(Green,Black),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green,Black),
      [PROCESS] = A_NORMAL,
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Black),
      [PROCESS_MEGABYTES] = ColorPair(Cyan,Black),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan,Black),
      [PROCESS_TREE] = ColorPair(Cyan,Black),
      [PROCESS_R_STATE] = ColorPair(Green,Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green,Black),
      [PROCESS_THREAD] = ColorPair(Green,Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green,Black),
      [BAR_BORDER] = A_BOLD,
      [BAR_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [SWAP] = ColorPair(Red,Black),
      [GRAPH_1] = A_BOLD | ColorPair(Cyan,Black),
      [GRAPH_2] = ColorPair(Cyan,Black),
      [MEMORY_USED] = ColorPair(Green,Black),
      [MEMORY_BUFFERS] = ColorPair(Blue,Black),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [MEMORY_CACHE] = ColorPair(Yellow,Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Cyan,Black),
      [LOAD_AVERAGE_FIVE] = A_BOLD | ColorPair(Cyan,Black),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White,Black),
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan,Black),
      [CLOCK] = A_BOLD,
      [CHECK_BOX] = ColorPair(Cyan,Black),
      [CHECK_MARK] = A_BOLD,
      [CHECK_TEXT] = A_NORMAL,
      [HOSTNAME] = A_BOLD,
      [CPU_NICE] = ColorPair(Blue,Black),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [CPU_NORMAL] = ColorPair(Green,Black),
      [CPU_SYSTEM] = ColorPair(Red,Black),
      [CPU_IOWAIT] = A_BOLD | ColorPairGrayBlack,
      [CPU_IRQ] = ColorPair(Yellow,Black),
      [CPU_SOFTIRQ] = ColorPair(Magenta,Black),
      [CPU_STEAL] = ColorPair(Cyan,Black),
      [CPU_GUEST] = ColorPair(Cyan,Black),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Cyan,Black),
      [PRESSURE_STALL_SIXTY] = A_BOLD | ColorPair(Cyan,Black),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(White,Black),
      [ZFS_MFU] = ColorPair(Blue,Black),
      [ZFS_MRU] = ColorPair(Yellow,Black),
      [ZFS_ANON] = ColorPair(Magenta,Black),
      [ZFS_HEADER] = ColorPair(Cyan,Black),
      [ZFS_OTHER] = ColorPair(Magenta,Black),
      [ZFS_COMPRESSED] = ColorPair(Blue,Black),
      [ZFS_RATIO] = ColorPair(Magenta,Black),
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
      [UPTIME] = A_BOLD,
      [BATTERY] = A_BOLD,
      [LARGE_NUMBER] = A_BOLD,
      [METER_TEXT] = A_NORMAL,
      [METER_VALUE] = A_BOLD,
      [LED_COLOR] = A_NORMAL,
      [TASKS_RUNNING] = A_BOLD,
      [PROCESS] = A_NORMAL,
      [PROCESS_SHADOW] = A_DIM,
      [PROCESS_TAG] = A_BOLD,
      [PROCESS_MEGABYTES] = A_BOLD,
      [PROCESS_BASENAME] = A_BOLD,
      [PROCESS_TREE] = A_BOLD,
      [PROCESS_R_STATE] = A_BOLD,
      [PROCESS_D_STATE] = A_BOLD,
      [PROCESS_HIGH_PRIORITY] = A_BOLD,
      [PROCESS_LOW_PRIORITY] = A_DIM,
      [PROCESS_THREAD] = A_BOLD,
      [PROCESS_THREAD_BASENAME] = A_REVERSE,
      [BAR_BORDER] = A_BOLD,
      [BAR_SHADOW] = A_DIM,
      [SWAP] = A_BOLD,
      [GRAPH_1] = A_BOLD,
      [GRAPH_2] = A_NORMAL,
      [MEMORY_USED] = A_BOLD,
      [MEMORY_BUFFERS] = A_NORMAL,
      [MEMORY_BUFFERS_TEXT] = A_NORMAL,
      [MEMORY_CACHE] = A_NORMAL,
      [LOAD_AVERAGE_FIFTEEN] = A_DIM,
      [LOAD_AVERAGE_FIVE] = A_NORMAL,
      [LOAD_AVERAGE_ONE] = A_BOLD,
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD,
      [CLOCK] = A_BOLD,
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
      [CPU_STEAL] = A_REVERSE,
      [CPU_GUEST] = A_REVERSE,
      [PRESSURE_STALL_THREEHUNDRED] = A_DIM,
      [PRESSURE_STALL_SIXTY] = A_NORMAL,
      [PRESSURE_STALL_TEN] = A_BOLD,
      [ZFS_MFU] = A_NORMAL,
      [ZFS_MRU] = A_NORMAL,
      [ZFS_ANON] = A_DIM,
      [ZFS_HEADER] = A_BOLD,
      [ZFS_OTHER] = A_DIM,
      [ZFS_COMPRESSED] = A_BOLD,
      [ZFS_RATIO] = A_BOLD,
   },
   [COLORSCHEME_BLACKONWHITE] = {
      [RESET_COLOR] = ColorPair(Black,White),
      [DEFAULT_COLOR] = ColorPair(Black,White),
      [FUNCTION_BAR] = ColorPair(Black,Cyan),
      [FUNCTION_KEY] = ColorPair(Black,White),
      [PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Blue,White),
      [FAILED_SEARCH] = ColorPair(Red,Cyan),
      [UPTIME] = ColorPair(Yellow,White),
      [BATTERY] = ColorPair(Yellow,White),
      [LARGE_NUMBER] = ColorPair(Red,White),
      [METER_TEXT] = ColorPair(Blue,White),
      [METER_VALUE] = ColorPair(Black,White),
      [LED_COLOR] = ColorPair(Green,White),
      [TASKS_RUNNING] = ColorPair(Green,White),
      [PROCESS] = ColorPair(Black,White),
      [PROCESS_SHADOW] = A_BOLD | ColorPair(Black,White),
      [PROCESS_TAG] = ColorPair(White,Blue),
      [PROCESS_MEGABYTES] = ColorPair(Blue,White),
      [PROCESS_BASENAME] = ColorPair(Blue,White),
      [PROCESS_TREE] = ColorPair(Green,White),
      [PROCESS_R_STATE] = ColorPair(Green,White),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red,White),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red,White),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green,White),
      [PROCESS_THREAD] = ColorPair(Blue,White),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,White),
      [BAR_BORDER] = ColorPair(Blue,White),
      [BAR_SHADOW] = ColorPair(Black,White),
      [SWAP] = ColorPair(Red,White),
      [GRAPH_1] = A_BOLD | ColorPair(Blue,White),
      [GRAPH_2] = ColorPair(Blue,White),
      [MEMORY_USED] = ColorPair(Green,White),
      [MEMORY_BUFFERS] = ColorPair(Cyan,White),
      [MEMORY_BUFFERS_TEXT] = ColorPair(Cyan,White),
      [MEMORY_CACHE] = ColorPair(Yellow,White),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Black,White),
      [LOAD_AVERAGE_FIVE] = ColorPair(Black,White),
      [LOAD_AVERAGE_ONE] = ColorPair(Black,White),
      [LOAD] = ColorPair(Black,White),
      [HELP_BOLD] = ColorPair(Blue,White),
      [CLOCK] = ColorPair(Black,White),
      [CHECK_BOX] = ColorPair(Blue,White),
      [CHECK_MARK] = ColorPair(Black,White),
      [CHECK_TEXT] = ColorPair(Black,White),
      [HOSTNAME] = ColorPair(Black,White),
      [CPU_NICE] = ColorPair(Cyan,White),
      [CPU_NICE_TEXT] = ColorPair(Cyan,White),
      [CPU_NORMAL] = ColorPair(Green,White),
      [CPU_SYSTEM] = ColorPair(Red,White),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black,White),
      [CPU_IRQ] = ColorPair(Blue,White),
      [CPU_SOFTIRQ] = ColorPair(Blue,White),
      [CPU_STEAL] = ColorPair(Cyan,White),
      [CPU_GUEST] = ColorPair(Cyan,White),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Black,White),
      [PRESSURE_STALL_SIXTY] = ColorPair(Black,White),
      [PRESSURE_STALL_TEN] = ColorPair(Black,White),
      [ZFS_MFU] = ColorPair(Cyan,White),
      [ZFS_MRU] = ColorPair(Yellow,White),
      [ZFS_ANON] = ColorPair(Magenta,White),
      [ZFS_HEADER] = ColorPair(Yellow,White),
      [ZFS_OTHER] = ColorPair(Magenta,White),
      [ZFS_COMPRESSED] = ColorPair(Cyan,White),
      [ZFS_RATIO] = ColorPair(Magenta,White),
   },
   [COLORSCHEME_LIGHTTERMINAL] = {
      [RESET_COLOR] = ColorPair(Black,Black),
      [DEFAULT_COLOR] = ColorPair(Black,Black),
      [FUNCTION_BAR] = ColorPair(Black,Cyan),
      [FUNCTION_KEY] = ColorPair(Black,Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Blue,Black),
      [FAILED_SEARCH] = ColorPair(Red,Cyan),
      [UPTIME] = ColorPair(Yellow,Black),
      [BATTERY] = ColorPair(Yellow,Black),
      [LARGE_NUMBER] = ColorPair(Red,Black),
      [METER_TEXT] = ColorPair(Blue,Black),
      [METER_VALUE] = ColorPair(Black,Black),
      [LED_COLOR] = ColorPair(Green,Black),
      [TASKS_RUNNING] = ColorPair(Green,Black),
      [PROCESS] = ColorPair(Black,Black),
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = ColorPair(White,Blue),
      [PROCESS_MEGABYTES] = ColorPair(Blue,Black),
      [PROCESS_BASENAME] = ColorPair(Green,Black),
      [PROCESS_TREE] = ColorPair(Blue,Black),
      [PROCESS_R_STATE] = ColorPair(Green,Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green,Black),
      [PROCESS_THREAD] = ColorPair(Blue,Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,Black),
      [BAR_BORDER] = ColorPair(Blue,Black),
      [BAR_SHADOW] = ColorPairGrayBlack,
      [SWAP] = ColorPair(Red,Black),
      [GRAPH_1] = A_BOLD | ColorPair(Cyan,Black),
      [GRAPH_2] = ColorPair(Cyan,Black),
      [MEMORY_USED] = ColorPair(Green,Black),
      [MEMORY_BUFFERS] = ColorPair(Cyan,Black),
      [MEMORY_BUFFERS_TEXT] = ColorPair(Cyan,Black),
      [MEMORY_CACHE] = ColorPair(Yellow,Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Black,Black),
      [LOAD_AVERAGE_FIVE] = ColorPair(Black,Black),
      [LOAD_AVERAGE_ONE] = ColorPair(Black,Black),
      [LOAD] = ColorPair(White,Black),
      [HELP_BOLD] = ColorPair(Blue,Black),
      [CLOCK] = ColorPair(White,Black),
      [CHECK_BOX] = ColorPair(Blue,Black),
      [CHECK_MARK] = ColorPair(Black,Black),
      [CHECK_TEXT] = ColorPair(Black,Black),
      [HOSTNAME] = ColorPair(White,Black),
      [CPU_NICE] = ColorPair(Cyan,Black),
      [CPU_NICE_TEXT] = ColorPair(Cyan,Black),
      [CPU_NORMAL] = ColorPair(Green,Black),
      [CPU_SYSTEM] = ColorPair(Red,Black),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Black,Black),
      [CPU_IRQ] = A_BOLD | ColorPair(Blue,Black),
      [CPU_SOFTIRQ] = ColorPair(Blue,Black),
      [CPU_STEAL] = ColorPair(Black,Black),
      [CPU_GUEST] = ColorPair(Black,Black),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Black,Black),
      [PRESSURE_STALL_SIXTY] = ColorPair(Black,Black),
      [PRESSURE_STALL_TEN] = ColorPair(Black,Black),
      [ZFS_MFU] = ColorPair(Cyan,Black),
      [ZFS_MRU] = ColorPair(Yellow,Black),
      [ZFS_ANON] = A_BOLD | ColorPair(Magenta,Black),
      [ZFS_HEADER] = ColorPair(Black,Black),
      [ZFS_OTHER] = A_BOLD | ColorPair(Magenta,Black),
      [ZFS_COMPRESSED] = ColorPair(Cyan,Black),
      [ZFS_RATIO] = A_BOLD | ColorPair(Magenta,Black),
   },
   [COLORSCHEME_MIDNIGHT] = {
      [RESET_COLOR] = ColorPair(White,Blue),
      [DEFAULT_COLOR] = ColorPair(White,Blue),
      [FUNCTION_BAR] = ColorPair(Black,Cyan),
      [FUNCTION_KEY] = A_NORMAL,
      [PANEL_HEADER_FOCUS] = ColorPair(Black,Cyan),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black,Cyan),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black,White),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [PANEL_SELECTION_UNFOCUS] = A_BOLD | ColorPair(Yellow,Blue),
      [FAILED_SEARCH] = ColorPair(Red,Cyan),
      [UPTIME] = A_BOLD | ColorPair(Yellow,Blue),
      [BATTERY] = A_BOLD | ColorPair(Yellow,Blue),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red,Blue),
      [METER_TEXT] = ColorPair(Cyan,Blue),
      [METER_VALUE] = A_BOLD | ColorPair(Cyan,Blue),
      [LED_COLOR] = ColorPair(Green,Blue),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green,Blue),
      [PROCESS] = ColorPair(White,Blue),
      [PROCESS_SHADOW] = A_BOLD | ColorPair(Black,Blue),
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Blue),
      [PROCESS_MEGABYTES] = ColorPair(Cyan,Blue),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Cyan,Blue),
      [PROCESS_TREE] = ColorPair(Cyan,Blue),
      [PROCESS_R_STATE] = ColorPair(Green,Blue),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Blue),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red,Blue),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green,Blue),
      [PROCESS_THREAD] = ColorPair(Green,Blue),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Green,Blue),
      [BAR_BORDER] = A_BOLD | ColorPair(Yellow,Blue),
      [BAR_SHADOW] = ColorPair(Cyan,Blue),
      [SWAP] = ColorPair(Red,Blue),
      [GRAPH_1] = A_BOLD | ColorPair(Cyan,Blue),
      [GRAPH_2] = ColorPair(Cyan,Blue),
      [MEMORY_USED] = A_BOLD | ColorPair(Green,Blue),
      [MEMORY_BUFFERS] = A_BOLD | ColorPair(Cyan,Blue),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Cyan,Blue),
      [MEMORY_CACHE] = A_BOLD | ColorPair(Yellow,Blue),
      [LOAD_AVERAGE_FIFTEEN] = A_BOLD | ColorPair(Black,Blue),
      [LOAD_AVERAGE_FIVE] = A_NORMAL | ColorPair(White,Blue),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(White,Blue),
      [LOAD] = A_BOLD | ColorPair(White,Blue),
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan,Blue),
      [CLOCK] = ColorPair(White,Blue),
      [CHECK_BOX] = ColorPair(Cyan,Blue),
      [CHECK_MARK] = A_BOLD | ColorPair(White,Blue),
      [CHECK_TEXT] = A_NORMAL | ColorPair(White,Blue),
      [HOSTNAME] = ColorPair(White,Blue),
      [CPU_NICE] = A_BOLD | ColorPair(Cyan,Blue),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Cyan,Blue),
      [CPU_NORMAL] = A_BOLD | ColorPair(Green,Blue),
      [CPU_SYSTEM] = A_BOLD | ColorPair(Red,Blue),
      [CPU_IOWAIT] = A_BOLD | ColorPair(Blue,Blue),
      [CPU_IRQ] = A_BOLD | ColorPair(Black,Blue),
      [CPU_SOFTIRQ] = ColorPair(Black,Blue),
      [CPU_STEAL] = ColorPair(White,Blue),
      [CPU_GUEST] = ColorPair(White,Blue),
      [PRESSURE_STALL_THREEHUNDRED] = A_BOLD | ColorPair(Black,Blue),
      [PRESSURE_STALL_SIXTY] = A_NORMAL | ColorPair(White,Blue),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(White,Blue),
      [ZFS_MFU] = A_BOLD | ColorPair(White,Blue),
      [ZFS_MRU] = A_BOLD | ColorPair(Yellow,Blue),
      [ZFS_ANON] = A_BOLD | ColorPair(Magenta,Blue),
      [ZFS_HEADER] = A_BOLD | ColorPair(Yellow,Blue),
      [ZFS_OTHER] = A_BOLD | ColorPair(Magenta,Blue),
      [ZFS_COMPRESSED] = A_BOLD | ColorPair(White,Blue),
      [ZFS_RATIO] = A_BOLD | ColorPair(Magenta,Blue),
   },
   [COLORSCHEME_BLACKNIGHT] = {
      [RESET_COLOR] = ColorPair(Cyan,Black),
      [DEFAULT_COLOR] = ColorPair(Cyan,Black),
      [FUNCTION_BAR] = ColorPair(Black,Green),
      [FUNCTION_KEY] = ColorPair(Cyan,Black),
      [PANEL_HEADER_FOCUS] = ColorPair(Black,Green),
      [PANEL_HEADER_UNFOCUS] = ColorPair(Black,Green),
      [PANEL_SELECTION_FOCUS] = ColorPair(Black,Cyan),
      [PANEL_SELECTION_FOLLOW] = ColorPair(Black,Yellow),
      [PANEL_SELECTION_UNFOCUS] = ColorPair(Black,White),
      [FAILED_SEARCH] = ColorPair(Red,Cyan),
      [UPTIME] = ColorPair(Green,Black),
      [BATTERY] = ColorPair(Green,Black),
      [LARGE_NUMBER] = A_BOLD | ColorPair(Red,Black),
      [METER_TEXT] = ColorPair(Cyan,Black),
      [METER_VALUE] = ColorPair(Green,Black),
      [LED_COLOR] = ColorPair(Green,Black),
      [TASKS_RUNNING] = A_BOLD | ColorPair(Green,Black),
      [PROCESS] = ColorPair(Cyan,Black),
      [PROCESS_SHADOW] = A_BOLD | ColorPairGrayBlack,
      [PROCESS_TAG] = A_BOLD | ColorPair(Yellow,Black),
      [PROCESS_MEGABYTES] = A_BOLD | ColorPair(Green,Black),
      [PROCESS_BASENAME] = A_BOLD | ColorPair(Green,Black),
      [PROCESS_TREE] = ColorPair(Cyan,Black),
      [PROCESS_THREAD] = ColorPair(Green,Black),
      [PROCESS_THREAD_BASENAME] = A_BOLD | ColorPair(Blue,Black),
      [PROCESS_R_STATE] = ColorPair(Green,Black),
      [PROCESS_D_STATE] = A_BOLD | ColorPair(Red,Black),
      [PROCESS_HIGH_PRIORITY] = ColorPair(Red,Black),
      [PROCESS_LOW_PRIORITY] = ColorPair(Green,Black),
      [BAR_BORDER] = A_BOLD | ColorPair(Green,Black),
      [BAR_SHADOW] = ColorPair(Cyan,Black),
      [SWAP] = ColorPair(Red,Black),
      [GRAPH_1] = A_BOLD | ColorPair(Green,Black),
      [GRAPH_2] = ColorPair(Green,Black),
      [MEMORY_USED] = ColorPair(Green,Black),
      [MEMORY_BUFFERS] = ColorPair(Blue,Black),
      [MEMORY_BUFFERS_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [MEMORY_CACHE] = ColorPair(Yellow,Black),
      [LOAD_AVERAGE_FIFTEEN] = ColorPair(Green,Black),
      [LOAD_AVERAGE_FIVE] = ColorPair(Green,Black),
      [LOAD_AVERAGE_ONE] = A_BOLD | ColorPair(Green,Black),
      [LOAD] = A_BOLD,
      [HELP_BOLD] = A_BOLD | ColorPair(Cyan,Black),
      [CLOCK] = ColorPair(Green,Black),
      [CHECK_BOX] = ColorPair(Green,Black),
      [CHECK_MARK] = A_BOLD | ColorPair(Green,Black),
      [CHECK_TEXT] = ColorPair(Cyan,Black),
      [HOSTNAME] = ColorPair(Green,Black),
      [CPU_NICE] = ColorPair(Blue,Black),
      [CPU_NICE_TEXT] = A_BOLD | ColorPair(Blue,Black),
      [CPU_NORMAL] = ColorPair(Green,Black),
      [CPU_SYSTEM] = ColorPair(Red,Black),
      [CPU_IOWAIT] = ColorPair(Yellow,Black),
      [CPU_IRQ] = A_BOLD | ColorPair(Blue,Black),
      [CPU_SOFTIRQ] = ColorPair(Blue,Black),
      [CPU_STEAL] = ColorPair(Cyan,Black),
      [CPU_GUEST] = ColorPair(Cyan,Black),
      [PRESSURE_STALL_THREEHUNDRED] = ColorPair(Green,Black),
      [PRESSURE_STALL_SIXTY] = ColorPair(Green,Black),
      [PRESSURE_STALL_TEN] = A_BOLD | ColorPair(Green,Black),
      [ZFS_MFU] = ColorPair(Blue,Black),
      [ZFS_MRU] = ColorPair(Yellow,Black),
      [ZFS_ANON] = ColorPair(Magenta,Black),
      [ZFS_HEADER] = ColorPair(Yellow,Black),
      [ZFS_OTHER] = ColorPair(Magenta,Black),
      [ZFS_COMPRESSED] = ColorPair(Blue,Black),
      [ZFS_RATIO] = ColorPair(Magenta,Black),
   },
   [COLORSCHEME_BROKENGRAY] = { 0 } // dynamically generated.
};

int CRT_cursorX = 0;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

char* CRT_termType;

// TODO move color scheme to Settings, perhaps?

int CRT_colorScheme = 0;

void *backtraceArray[128];

static void CRT_handleSIGTERM(int sgn) {
   (void) sgn;
   CRT_done();
   exit(0);
}

#if HAVE_SETUID_ENABLED

static int CRT_euid = -1;

static int CRT_egid = -1;

#define DIE(msg) do { CRT_done(); fprintf(stderr, msg); exit(1); } while(0)

void CRT_dropPrivileges() {
   CRT_egid = getegid();
   CRT_euid = geteuid();
   if (setegid(getgid()) == -1) {
      DIE("Fatal error: failed dropping group privileges.\n");
   }
   if (seteuid(getuid()) == -1) {
      DIE("Fatal error: failed dropping user privileges.\n");
   }
}

void CRT_restorePrivileges() {
   if (CRT_egid == -1 || CRT_euid == -1) {
      DIE("Fatal error: internal inconsistency.\n");
   }
   if (setegid(CRT_egid) == -1) {
      DIE("Fatal error: failed restoring group privileges.\n");
   }
   if (seteuid(CRT_euid) == -1) {
      DIE("Fatal error: failed restoring user privileges.\n");
   }
}

#else

// In this case, the setuid operations are defined as macros in CRT.h

#endif

// TODO: pass an instance of Settings instead.

void CRT_init(int delay, int colorScheme, bool allowUnicode) {
   initscr();
   noecho();
   CRT_delay = delay;
   if (CRT_delay == 0) {
      CRT_delay = 1;
   }
   CRT_colors = CRT_colorSchemes[colorScheme];
   CRT_colorScheme = colorScheme;

   for (int i = 0; i < LAST_COLORELEMENT; i++) {
      unsigned int color = CRT_colorSchemes[COLORSCHEME_DEFAULT][i];
      CRT_colorSchemes[COLORSCHEME_BROKENGRAY][i] = color == (A_BOLD | ColorPairGrayBlack) ? ColorPair(White,Black) : color;
   }

   halfdelay(CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   mouseinterval(0);
   curs_set(0);
   if (has_colors()) {
      start_color();
      CRT_hasColors = true;
   } else {
      CRT_hasColors = false;
   }
   CRT_termType = getenv("TERM");
   if (String_eq(CRT_termType, "linux"))
      CRT_scrollHAmount = 20;
   else
      CRT_scrollHAmount = 5;
   if (String_startsWith(CRT_termType, "xterm") || String_eq(CRT_termType, "vt220")) {
      define_key("\033[H", KEY_HOME);
      define_key("\033[F", KEY_END);
      define_key("\033[7~", KEY_HOME);
      define_key("\033[8~", KEY_END);
      define_key("\033OP", KEY_F(1));
      define_key("\033OQ", KEY_F(2));
      define_key("\033OR", KEY_F(3));
      define_key("\033OS", KEY_F(4));
      define_key("\033[11~", KEY_F(1));
      define_key("\033[12~", KEY_F(2));
      define_key("\033[13~", KEY_F(3));
      define_key("\033[14~", KEY_F(4));
      define_key("\033[17;2~", KEY_F(18));
      char sequence[3] = "\033a";
      for (char c = 'a'; c <= 'z'; c++) {
         sequence[1] = c;
         define_key(sequence, KEY_ALT('A' + (c - 'a')));
      }
   }
#ifndef DEBUG
   signal(11, CRT_handleSIGSEGV);
#endif
   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);
   use_default_colors();
   if (!has_colors())
      CRT_colorScheme = 1;
   CRT_setColors(CRT_colorScheme);

   /* initialize locale */
   setlocale(LC_CTYPE, "");

#ifdef HAVE_LIBNCURSESW
   if (allowUnicode && strcmp(nl_langinfo(CODESET), "UTF-8") == 0)
      CRT_utf8 = true;
   else
      CRT_utf8 = false;
#else
   (void) allowUnicode;
#endif

   CRT_treeStr =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? CRT_treeStrUtf8 :
#endif
      CRT_treeStrAscii;

#if NCURSES_MOUSE_VERSION > 1
   mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
#else
   mousemask(BUTTON1_RELEASED, NULL);
#endif

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

   for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
         if (ColorIndex(i,j) != ColorPairGrayBlack) {
            int bg = (colorScheme != COLORSCHEME_BLACKNIGHT)
                     ? (j==0 ? -1 : j)
                     : j;
            init_pair(ColorIndex(i,j), i, bg);
         }
      }
   }

   int grayBlackFg = COLORS > 8 ? 8 : 0;
   int grayBlackBg = (colorScheme != COLORSCHEME_BLACKNIGHT)
                     ? -1
                     : 0;
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   CRT_colors = CRT_colorSchemes[colorScheme];
}
