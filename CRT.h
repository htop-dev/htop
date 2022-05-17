#ifndef HEADER_CRT
#define HEADER_CRT
/*
htop - CRT.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include <stdbool.h>

#include "Macros.h"
#include "ProvideCurses.h"
#include "Settings.h"


typedef enum TreeStr_ {
   TREE_STR_VERT,
   TREE_STR_RTEE,
   TREE_STR_BEND,
   TREE_STR_TEND,
   TREE_STR_OPEN,
   TREE_STR_SHUT,
   TREE_STR_ASC,
   TREE_STR_DESC,
   LAST_TREE_STR
} TreeStr;

typedef enum ColorScheme_ {
   COLORSCHEME_DEFAULT,
   COLORSCHEME_MONOCHROME,
   COLORSCHEME_BLACKONWHITE,
   COLORSCHEME_LIGHTTERMINAL,
   COLORSCHEME_MIDNIGHT,
   COLORSCHEME_BLACKNIGHT,
   COLORSCHEME_BROKENGRAY,
   LAST_COLORSCHEME
} ColorScheme;

typedef enum ColorElements_ {
   RESET_COLOR,
   DEFAULT_COLOR,
   FUNCTION_BAR,
   FUNCTION_KEY,
   FAILED_SEARCH,
   FAILED_READ,
   PAUSED,
   PANEL_HEADER_FOCUS,
   PANEL_HEADER_UNFOCUS,
   PANEL_SELECTION_FOCUS,
   PANEL_SELECTION_FOLLOW,
   PANEL_SELECTION_UNFOCUS,
   LARGE_NUMBER,
   METER_SHADOW,
   METER_TEXT,
   METER_VALUE,
   METER_VALUE_ERROR,
   METER_VALUE_IOREAD,
   METER_VALUE_IOWRITE,
   METER_VALUE_NOTICE,
   METER_VALUE_OK,
   METER_VALUE_WARN,
   LED_COLOR,
   UPTIME,
   BATTERY,
   TASKS_RUNNING,
   SWAP,
   SWAP_CACHE,
   PROCESS,
   PROCESS_SHADOW,
   PROCESS_TAG,
   PROCESS_MEGABYTES,
   PROCESS_GIGABYTES,
   PROCESS_TREE,
   PROCESS_RUN_STATE,
   PROCESS_D_STATE,
   PROCESS_BASENAME,
   PROCESS_HIGH_PRIORITY,
   PROCESS_LOW_PRIORITY,
   PROCESS_NEW,
   PROCESS_TOMB,
   PROCESS_THREAD,
   PROCESS_THREAD_BASENAME,
   PROCESS_COMM,
   PROCESS_THREAD_COMM,
   BAR_BORDER,
   BAR_SHADOW,
   GRAPH_1,
   GRAPH_2,
   MEMORY_USED,
   MEMORY_BUFFERS,
   MEMORY_BUFFERS_TEXT,
   MEMORY_CACHE,
   MEMORY_SHARED,
   HUGEPAGE_1,
   HUGEPAGE_2,
   HUGEPAGE_3,
   HUGEPAGE_4,
   LOAD,
   LOAD_AVERAGE_FIFTEEN,
   LOAD_AVERAGE_FIVE,
   LOAD_AVERAGE_ONE,
   CHECK_BOX,
   CHECK_MARK,
   CHECK_TEXT,
   CLOCK,
   DATE,
   DATETIME,
   HELP_BOLD,
   HELP_SHADOW,
   HOSTNAME,
   CPU_NICE,
   CPU_NICE_TEXT,
   CPU_NORMAL,
   CPU_SYSTEM,
   CPU_IOWAIT,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   PANEL_EDIT,
   SCREENS_OTH_BORDER,
   SCREENS_OTH_TEXT,
   SCREENS_CUR_BORDER,
   SCREENS_CUR_TEXT,
   PRESSURE_STALL_TEN,
   PRESSURE_STALL_SIXTY,
   PRESSURE_STALL_THREEHUNDRED,
   ZFS_MFU,
   ZFS_MRU,
   ZFS_ANON,
   ZFS_HEADER,
   ZFS_OTHER,
   ZFS_COMPRESSED,
   ZFS_RATIO,
   ZRAM,
   DYNAMIC_GRAY,
   DYNAMIC_DARKGRAY,
   DYNAMIC_RED,
   DYNAMIC_GREEN,
   DYNAMIC_BLUE,
   DYNAMIC_CYAN,
   DYNAMIC_MAGENTA,
   DYNAMIC_YELLOW,
   DYNAMIC_WHITE,
   LAST_COLORELEMENT
} ColorElements;

void CRT_fatalError(const char* note) ATTR_NORETURN;

#ifdef NDEBUG
# define CRT_debug(...)
#else
void CRT_debug_impl(const char* file, size_t lineno, const char* func, const char* fmt, ...) ATTR_FORMAT(printf, 4, 5);
# define CRT_debug(...) CRT_debug_impl(__FILE__, __LINE__, __func__, __VA_ARGS__)
#endif

void CRT_handleSIGSEGV(int signal) ATTR_NORETURN;

#define KEY_WHEELUP   KEY_F(30)
#define KEY_WHEELDOWN KEY_F(31)
#define KEY_RECLICK   KEY_F(32)
#define KEY_SHIFT_TAB KEY_F(33)
#define KEY_ALT(x)    (KEY_F(64 - 26) + ((x) - 'A'))

extern const char* CRT_degreeSign;

#ifdef HAVE_LIBNCURSESW

extern bool CRT_utf8;

#endif

extern const char* const* CRT_treeStr;

extern const int* CRT_colors;

extern int CRT_cursorX;

extern int CRT_scrollHAmount;

extern int CRT_scrollWheelVAmount;

extern ColorScheme CRT_colorScheme;

void CRT_setMouse(bool enabled);

void CRT_init(const Settings* settings, bool allowUnicode);

void CRT_done(void);

void CRT_resetSignalHandlers(void);

int CRT_readKey(void);

void CRT_disableDelay(void);

void CRT_enableDelay(void);

void CRT_setColors(int colorScheme);

#endif
