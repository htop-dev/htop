/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "CRT.h"

#include <errno.h>
#include <langinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ProvideCurses.h"
#include "XUtils.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif


#define Default -1
#define Black   COLOR_BLACK
#define Red     COLOR_RED
#define Green   COLOR_GREEN
#define Yellow  COLOR_YELLOW
#define Blue    COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan    COLOR_CYAN
#define White   COLOR_WHITE
#define Gray    8

#define A_REVBOLD (A_REVERSE | A_BOLD)

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

typedef struct Style_ {
   int attrs;
   int fg, bg;
} Style;

static Style CRT_colorSchemes[LAST_COLORSCHEME][LAST_STYLE] = {
   [COLORSCHEME_DEFAULT] = {
      [RESET_COLOR]                 = { A_NORMAL, Default, Default },
      [DEFAULT_COLOR]               = { A_NORMAL, Default, Default },
      [FUNCTION_BAR]                = { A_NORMAL, Black,   Cyan    },
      [FUNCTION_KEY]                = { A_NORMAL, Default, Default },
      [PANEL_HEADER_FOCUS]          = { A_NORMAL, Black,   Green   },
      [PANEL_HEADER_UNFOCUS]        = { A_NORMAL, Black,   Green   },
      [PANEL_SELECTION_FOCUS]       = { A_NORMAL, Black,   Cyan    },
      [PANEL_SELECTION_FOLLOW]      = { A_NORMAL, Black,   Yellow  },
      [PANEL_SELECTION_UNFOCUS]     = { A_NORMAL, Black,   White   },
      [FAILED_SEARCH]               = { A_NORMAL, Red,     Cyan    },
      [FAILED_READ]                 = { A_BOLD,   Red,     Default },
      [PAUSED]                      = { A_BOLD,   Yellow,  Cyan    },
      [UPTIME]                      = { A_BOLD,   Cyan,    Default },
      [BATTERY]                     = { A_BOLD,   Cyan,    Default },
      [LARGE_NUMBER]                = { A_BOLD,   Red,     Default },
      [METER_TEXT]                  = { A_NORMAL, Cyan,    Default },
      [METER_VALUE]                 = { A_BOLD,   Cyan,    Default },
      [METER_VALUE_ERROR]           = { A_BOLD,   Red,     Default },
      [METER_VALUE_IOREAD]          = { A_NORMAL, Green,   Default },
      [METER_VALUE_IOWRITE]         = { A_NORMAL, Blue,    Default },
      [METER_VALUE_NOTICE]          = { A_BOLD,   Default, Default },
      [METER_VALUE_OK]              = { A_NORMAL, Green,   Default },
      [METER_VALUE_WARN]            = { A_BOLD,   Yellow,  Default },
      [LED_COLOR]                   = { A_NORMAL, Green,   Default },
      [TASKS_RUNNING]               = { A_BOLD,   Green,   Default },
      [PROCESS]                     = { A_NORMAL, Default, Default },
      [PROCESS_SHADOW]              = { A_BOLD,   Gray,    Default },
      [PROCESS_TAG]                 = { A_BOLD,   Yellow,  Default },
      [PROCESS_MEGABYTES]           = { A_NORMAL, Cyan,    Default },
      [PROCESS_GIGABYTES]           = { A_NORMAL, Green,   Default },
      [PROCESS_BASENAME]            = { A_BOLD,   Cyan,    Default },
      [PROCESS_TREE]                = { A_NORMAL, Cyan,    Default },
      [PROCESS_R_STATE]             = { A_NORMAL, Green,   Default },
      [PROCESS_D_STATE]             = { A_BOLD,   Red,     Default },
      [PROCESS_HIGH_PRIORITY]       = { A_NORMAL, Red,     Default },
      [PROCESS_LOW_PRIORITY]        = { A_NORMAL, Green,   Default },
      [PROCESS_NEW]                 = { A_NORMAL, Black,   Green   },
      [PROCESS_TOMB]                = { A_NORMAL, Black,   Red     },
      [PROCESS_THREAD]              = { A_NORMAL, Green,   Default },
      [PROCESS_THREAD_BASENAME]     = { A_BOLD,   Green,   Default },
      [PROCESS_COMM]                = { A_NORMAL, Magenta, Default },
      [PROCESS_THREAD_COMM]         = { A_NORMAL, Blue,    Default },
      [BAR_BORDER]                  = { A_BOLD,   Default, Default },
      [BAR_SHADOW]                  = { A_BOLD,   Gray,    Default },
      [SWAP]                        = { A_NORMAL, Red,     Default },
      [GRAPH_1]                     = { A_BOLD,   Cyan,    Default },
      [GRAPH_2]                     = { A_NORMAL, Cyan,    Default },
      [MEMORY_USED]                 = { A_NORMAL, Green,   Default },
      [MEMORY_BUFFERS]              = { A_NORMAL, Blue,    Default },
      [MEMORY_BUFFERS_TEXT]         = { A_BOLD,   Blue,    Default },
      [MEMORY_CACHE]                = { A_NORMAL, Yellow,  Default },
      [LOAD_AVERAGE_FIFTEEN]        = { A_NORMAL, Cyan,    Default },
      [LOAD_AVERAGE_FIVE]           = { A_BOLD,   Cyan,    Default },
      [LOAD_AVERAGE_ONE]            = { A_BOLD,   Default, Default },
      [LOAD]                        = { A_BOLD,   Default, Default },
      [HELP_BOLD]                   = { A_BOLD,   Cyan,    Default },
      [CLOCK]                       = { A_BOLD,   Default, Default },
      [DATE]                        = { A_BOLD,   Default, Default },
      [DATETIME]                    = { A_BOLD,   Default, Default },
      [CHECK_BOX]                   = { A_NORMAL, Cyan,    Default },
      [CHECK_MARK]                  = { A_BOLD,   Default, Default },
      [CHECK_TEXT]                  = { A_NORMAL, Default, Default },
      [HOSTNAME]                    = { A_BOLD,   Default, Default },
      [CPU_NICE]                    = { A_NORMAL, Blue,    Default },
      [CPU_NICE_TEXT]               = { A_BOLD,   Blue,    Default },
      [CPU_NORMAL]                  = { A_NORMAL, Green,   Default },
      [CPU_SYSTEM]                  = { A_NORMAL, Red,     Default },
      [CPU_IOWAIT]                  = { A_BOLD,   Gray,    Default },
      [CPU_IRQ]                     = { A_NORMAL, Yellow,  Default },
      [CPU_SOFTIRQ]                 = { A_NORMAL, Magenta, Default },
      [CPU_STEAL]                   = { A_NORMAL, Cyan,    Default },
      [CPU_GUEST]                   = { A_NORMAL, Cyan,    Default },
      [PRESSURE_STALL_THREEHUNDRED] = { A_NORMAL, Cyan,    Default },
      [PRESSURE_STALL_SIXTY]        = { A_BOLD,   Cyan,    Default },
      [PRESSURE_STALL_TEN]          = { A_BOLD,   Default, Default },
      [ZFS_MFU]                     = { A_NORMAL, Blue,    Default },
      [ZFS_MRU]                     = { A_NORMAL, Yellow,  Default },
      [ZFS_ANON]                    = { A_NORMAL, Magenta, Default },
      [ZFS_HEADER]                  = { A_NORMAL, Cyan,    Default },
      [ZFS_OTHER]                   = { A_NORMAL, Magenta, Default },
      [ZFS_COMPRESSED]              = { A_NORMAL, Blue,    Default },
      [ZFS_RATIO]                   = { A_NORMAL, Magenta, Default },
      [ZRAM]                        = { A_NORMAL, Yellow,  Default },
   },

   [COLORSCHEME_MONOCHROME] = {
      [RESET_COLOR]                 = { A_NORMAL,  Default, Default },
      [DEFAULT_COLOR]               = { A_NORMAL,  Default, Default },
      [FUNCTION_BAR]                = { A_REVERSE, Default, Default },
      [FUNCTION_KEY]                = { A_NORMAL,  Default, Default },
      [PANEL_HEADER_FOCUS]          = { A_REVERSE, Default, Default },
      [PANEL_HEADER_UNFOCUS]        = { A_REVERSE, Default, Default },
      [PANEL_SELECTION_FOCUS]       = { A_REVERSE, Default, Default },
      [PANEL_SELECTION_FOLLOW]      = { A_REVERSE, Default, Default },
      [PANEL_SELECTION_UNFOCUS]     = { A_BOLD,    Default, Default },
      [FAILED_SEARCH]               = { A_REVBOLD, Default, Default },
      [FAILED_READ]                 = { A_BOLD,    Default, Default },
      [PAUSED]                      = { A_REVBOLD, Default, Default },
      [UPTIME]                      = { A_BOLD,    Default, Default },
      [BATTERY]                     = { A_BOLD,    Default, Default },
      [LARGE_NUMBER]                = { A_BOLD,    Default, Default },
      [METER_TEXT]                  = { A_NORMAL,  Default, Default },
      [METER_VALUE]                 = { A_BOLD,    Default, Default },
      [METER_VALUE_ERROR]           = { A_BOLD,    Default, Default },
      [METER_VALUE_IOREAD]          = { A_NORMAL,  Default, Default },
      [METER_VALUE_IOWRITE]         = { A_NORMAL,  Default, Default },
      [METER_VALUE_NOTICE]          = { A_BOLD,    Default, Default },
      [METER_VALUE_OK]              = { A_NORMAL,  Default, Default },
      [METER_VALUE_WARN]            = { A_BOLD,    Default, Default },
      [LED_COLOR]                   = { A_NORMAL,  Default, Default },
      [TASKS_RUNNING]               = { A_BOLD,    Default, Default },
      [PROCESS]                     = { A_NORMAL,  Default, Default },
      [PROCESS_SHADOW]              = { A_DIM,     Default, Default },
      [PROCESS_TAG]                 = { A_BOLD,    Default, Default },
      [PROCESS_MEGABYTES]           = { A_BOLD,    Default, Default },
      [PROCESS_GIGABYTES]           = { A_BOLD,    Default, Default },
      [PROCESS_BASENAME]            = { A_BOLD,    Default, Default },
      [PROCESS_TREE]                = { A_BOLD,    Default, Default },
      [PROCESS_R_STATE]             = { A_BOLD,    Default, Default },
      [PROCESS_D_STATE]             = { A_BOLD,    Default, Default },
      [PROCESS_HIGH_PRIORITY]       = { A_BOLD,    Default, Default },
      [PROCESS_LOW_PRIORITY]        = { A_DIM,     Default, Default },
      [PROCESS_NEW]                 = { A_BOLD,    Default, Default },
      [PROCESS_TOMB]                = { A_DIM,     Default, Default },
      [PROCESS_THREAD]              = { A_BOLD,    Default, Default },
      [PROCESS_THREAD_BASENAME]     = { A_REVERSE, Default, Default },
      [PROCESS_COMM]                = { A_BOLD,    Default, Default },
      [PROCESS_THREAD_COMM]         = { A_REVERSE, Default, Default },
      [BAR_BORDER]                  = { A_BOLD,    Default, Default },
      [BAR_SHADOW]                  = { A_DIM,     Default, Default },
      [SWAP]                        = { A_BOLD,    Default, Default },
      [GRAPH_1]                     = { A_BOLD,    Default, Default },
      [GRAPH_2]                     = { A_NORMAL,  Default, Default },
      [MEMORY_USED]                 = { A_BOLD,    Default, Default },
      [MEMORY_BUFFERS]              = { A_NORMAL,  Default, Default },
      [MEMORY_BUFFERS_TEXT]         = { A_NORMAL,  Default, Default },
      [MEMORY_CACHE]                = { A_NORMAL,  Default, Default },
      [LOAD_AVERAGE_FIFTEEN]        = { A_DIM,     Default, Default },
      [LOAD_AVERAGE_FIVE]           = { A_NORMAL,  Default, Default },
      [LOAD_AVERAGE_ONE]            = { A_BOLD,    Default, Default },
      [LOAD]                        = { A_BOLD,    Default, Default },
      [HELP_BOLD]                   = { A_BOLD,    Default, Default },
      [CLOCK]                       = { A_BOLD,    Default, Default },
      [DATE]                        = { A_BOLD,    Default, Default },
      [DATETIME]                    = { A_BOLD,    Default, Default },
      [CHECK_BOX]                   = { A_BOLD,    Default, Default },
      [CHECK_MARK]                  = { A_NORMAL,  Default, Default },
      [CHECK_TEXT]                  = { A_NORMAL,  Default, Default },
      [HOSTNAME]                    = { A_BOLD,    Default, Default },
      [CPU_NICE]                    = { A_NORMAL,  Default, Default },
      [CPU_NICE_TEXT]               = { A_NORMAL,  Default, Default },
      [CPU_NORMAL]                  = { A_BOLD,    Default, Default },
      [CPU_SYSTEM]                  = { A_BOLD,    Default, Default },
      [CPU_IOWAIT]                  = { A_NORMAL,  Default, Default },
      [CPU_IRQ]                     = { A_BOLD,    Default, Default },
      [CPU_SOFTIRQ]                 = { A_BOLD,    Default, Default },
      [CPU_STEAL]                   = { A_DIM,     Default, Default },
      [CPU_GUEST]                   = { A_DIM,     Default, Default },
      [PRESSURE_STALL_THREEHUNDRED] = { A_DIM,     Default, Default },
      [PRESSURE_STALL_SIXTY]        = { A_NORMAL,  Default, Default },
      [PRESSURE_STALL_TEN]          = { A_BOLD,    Default, Default },
      [ZFS_MFU]                     = { A_NORMAL,  Default, Default },
      [ZFS_MRU]                     = { A_NORMAL,  Default, Default },
      [ZFS_ANON]                    = { A_DIM,     Default, Default },
      [ZFS_HEADER]                  = { A_BOLD,    Default, Default },
      [ZFS_OTHER]                   = { A_DIM,     Default, Default },
      [ZFS_COMPRESSED]              = { A_BOLD,    Default, Default },
      [ZFS_RATIO]                   = { A_BOLD,    Default, Default },
      [ZRAM]                        = { A_NORMAL,  Default, Default },
   },

   [COLORSCHEME_BLACKONWHITE] = {
      [RESET_COLOR]                 = { A_NORMAL, Black,   White  },
      [DEFAULT_COLOR]               = { A_NORMAL, Black,   White  },
      [FUNCTION_BAR]                = { A_NORMAL, Black,   Cyan   },
      [FUNCTION_KEY]                = { A_NORMAL, Black,   White  },
      [PANEL_HEADER_FOCUS]          = { A_NORMAL, Black,   Green  },
      [PANEL_HEADER_UNFOCUS]        = { A_NORMAL, Black,   Green  },
      [PANEL_SELECTION_FOCUS]       = { A_NORMAL, Black,   Cyan   },
      [PANEL_SELECTION_FOLLOW]      = { A_NORMAL, Black,   Yellow },
      [PANEL_SELECTION_UNFOCUS]     = { A_NORMAL, Blue,    White  },
      [FAILED_SEARCH]               = { A_NORMAL, Red,     Cyan   },
      [FAILED_READ]                 = { A_NORMAL, Red,     White  },
      [PAUSED]                      = { A_BOLD,   Yellow,  Cyan   },
      [UPTIME]                      = { A_NORMAL, Yellow,  White  },
      [BATTERY]                     = { A_NORMAL, Yellow,  White  },
      [LARGE_NUMBER]                = { A_NORMAL, Red,     White  },
      [METER_TEXT]                  = { A_NORMAL, Blue,    White  },
      [METER_VALUE]                 = { A_NORMAL, Black,   White  },
      [METER_VALUE_ERROR]           = { A_BOLD,   Red,     White  },
      [METER_VALUE_IOREAD]          = { A_NORMAL, Green,   White  },
      [METER_VALUE_IOWRITE]         = { A_NORMAL, Yellow,  White  },
      [METER_VALUE_NOTICE]          = { A_BOLD,   Yellow,  White  },
      [METER_VALUE_OK]              = { A_NORMAL, Green,   White  },
      [METER_VALUE_WARN]            = { A_BOLD,   Yellow,  White  },
      [LED_COLOR]                   = { A_NORMAL, Green,   White  },
      [TASKS_RUNNING]               = { A_NORMAL, Green,   White  },
      [PROCESS]                     = { A_NORMAL, Black,   White  },
      [PROCESS_SHADOW]              = { A_BOLD,   Black,   White  },
      [PROCESS_TAG]                 = { A_NORMAL, White,   Blue   },
      [PROCESS_MEGABYTES]           = { A_NORMAL, Blue,    White  },
      [PROCESS_GIGABYTES]           = { A_NORMAL, Green,   White  },
      [PROCESS_BASENAME]            = { A_NORMAL, Blue,    White  },
      [PROCESS_TREE]                = { A_NORMAL, Green,   White  },
      [PROCESS_R_STATE]             = { A_NORMAL, Green,   White  },
      [PROCESS_D_STATE]             = { A_BOLD,   Red,     White  },
      [PROCESS_HIGH_PRIORITY]       = { A_NORMAL, Red,     White  },
      [PROCESS_LOW_PRIORITY]        = { A_NORMAL, Green,   White  },
      [PROCESS_NEW]                 = { A_NORMAL, White,   Green  },
      [PROCESS_TOMB]                = { A_NORMAL, White,   Red    },
      [PROCESS_THREAD]              = { A_NORMAL, Blue,    White  },
      [PROCESS_THREAD_BASENAME]     = { A_BOLD,   Blue,    White  },
      [PROCESS_COMM]                = { A_NORMAL, Magenta, White  },
      [PROCESS_THREAD_COMM]         = { A_NORMAL, Green,   White  },
      [BAR_BORDER]                  = { A_NORMAL, Blue,    White  },
      [BAR_SHADOW]                  = { A_NORMAL, Black,   White  },
      [SWAP]                        = { A_NORMAL, Red,     White  },
      [GRAPH_1]                     = { A_BOLD,   Blue,    White  },
      [GRAPH_2]                     = { A_NORMAL, Blue,    White  },
      [MEMORY_USED]                 = { A_NORMAL, Green,   White  },
      [MEMORY_BUFFERS]              = { A_NORMAL, Cyan,    White  },
      [MEMORY_BUFFERS_TEXT]         = { A_NORMAL, Cyan,    White  },
      [MEMORY_CACHE]                = { A_NORMAL, Yellow,  White  },
      [LOAD_AVERAGE_FIFTEEN]        = { A_NORMAL, Black,   White  },
      [LOAD_AVERAGE_FIVE]           = { A_NORMAL, Black,   White  },
      [LOAD_AVERAGE_ONE]            = { A_NORMAL, Black,   White  },
      [LOAD]                        = { A_NORMAL, Black,   White  },
      [HELP_BOLD]                   = { A_NORMAL, Blue,    White  },
      [CLOCK]                       = { A_NORMAL, Black,   White  },
      [DATE]                        = { A_NORMAL, Black,   White  },
      [DATETIME]                    = { A_NORMAL, Black,   White  },
      [CHECK_BOX]                   = { A_NORMAL, Blue,    White  },
      [CHECK_MARK]                  = { A_NORMAL, Black,   White  },
      [CHECK_TEXT]                  = { A_NORMAL, Black,   White  },
      [HOSTNAME]                    = { A_NORMAL, Black,   White  },
      [CPU_NICE]                    = { A_NORMAL, Cyan,    White  },
      [CPU_NICE_TEXT]               = { A_NORMAL, Cyan,    White  },
      [CPU_NORMAL]                  = { A_NORMAL, Green,   White  },
      [CPU_SYSTEM]                  = { A_NORMAL, Red,     White  },
      [CPU_IOWAIT]                  = { A_BOLD,   Black,   White  },
      [CPU_IRQ]                     = { A_NORMAL, Blue,    White  },
      [CPU_SOFTIRQ]                 = { A_NORMAL, Blue,    White  },
      [CPU_STEAL]                   = { A_NORMAL, Cyan,    White  },
      [CPU_GUEST]                   = { A_NORMAL, Cyan,    White  },
      [PRESSURE_STALL_THREEHUNDRED] = { A_NORMAL, Black,   White  },
      [PRESSURE_STALL_SIXTY]        = { A_NORMAL, Black,   White  },
      [PRESSURE_STALL_TEN]          = { A_NORMAL, Black,   White  },
      [ZFS_MFU]                     = { A_NORMAL, Cyan,    White  },
      [ZFS_MRU]                     = { A_NORMAL, Yellow,  White  },
      [ZFS_ANON]                    = { A_NORMAL, Magenta, White  },
      [ZFS_HEADER]                  = { A_NORMAL, Yellow,  White  },
      [ZFS_OTHER]                   = { A_NORMAL, Magenta, White  },
      [ZFS_COMPRESSED]              = { A_NORMAL, Cyan,    White  },
      [ZFS_RATIO]                   = { A_NORMAL, Magenta, White  },
      [ZRAM]                        = { A_NORMAL, Yellow,  White  },
   },

   [COLORSCHEME_LIGHTTERMINAL] = {
      [RESET_COLOR]                 = { A_NORMAL, Default, Default },
      [DEFAULT_COLOR]               = { A_NORMAL, Default, Default },
      [FUNCTION_BAR]                = { A_NORMAL, Black,   Cyan    },
      [FUNCTION_KEY]                = { A_NORMAL, Default, Default },
      [PANEL_HEADER_FOCUS]          = { A_NORMAL, Black,   Green   },
      [PANEL_HEADER_UNFOCUS]        = { A_NORMAL, Black,   Green   },
      [PANEL_SELECTION_FOCUS]       = { A_NORMAL, Black,   Cyan    },
      [PANEL_SELECTION_FOLLOW]      = { A_NORMAL, Black,   Yellow  },
      [PANEL_SELECTION_UNFOCUS]     = { A_NORMAL, Blue,    Default },
      [FAILED_SEARCH]               = { A_NORMAL, Red,     Cyan    },
      [FAILED_READ]                 = { A_NORMAL, Red,     Default },
      [PAUSED]                      = { A_BOLD,   Yellow,  Cyan    },
      [UPTIME]                      = { A_NORMAL, Yellow,  Default },
      [BATTERY]                     = { A_NORMAL, Yellow,  Default },
      [LARGE_NUMBER]                = { A_NORMAL, Red,     Default },
      [METER_TEXT]                  = { A_NORMAL, Blue,    Default },
      [METER_VALUE]                 = { A_NORMAL, Default, Default },
      [METER_VALUE_ERROR]           = { A_BOLD,   Red,     Default },
      [METER_VALUE_IOREAD]          = { A_NORMAL, Green,   Default },
      [METER_VALUE_IOWRITE]         = { A_NORMAL, Yellow,  Default },
      [METER_VALUE_NOTICE]          = { A_BOLD,   White,   Default },
      [METER_VALUE_OK]              = { A_NORMAL, Green,   Default },
      [METER_VALUE_WARN]            = { A_BOLD,   Yellow,  Default },
      [LED_COLOR]                   = { A_NORMAL, Green,   Default },
      [TASKS_RUNNING]               = { A_NORMAL, Green,   Default },
      [PROCESS]                     = { A_NORMAL, Default, Default },
      [PROCESS_SHADOW]              = { A_BOLD,   Gray,    Default },
      [PROCESS_TAG]                 = { A_NORMAL, White,   Blue    },
      [PROCESS_MEGABYTES]           = { A_NORMAL, Blue,    Default },
      [PROCESS_GIGABYTES]           = { A_NORMAL, Green,   Default },
      [PROCESS_BASENAME]            = { A_NORMAL, Green,   Default },
      [PROCESS_TREE]                = { A_NORMAL, Blue,    Default },
      [PROCESS_R_STATE]             = { A_NORMAL, Green,   Default },
      [PROCESS_D_STATE]             = { A_BOLD,   Red,     Default },
      [PROCESS_HIGH_PRIORITY]       = { A_NORMAL, Red,     Default },
      [PROCESS_LOW_PRIORITY]        = { A_NORMAL, Green,   Default },
      [PROCESS_NEW]                 = { A_NORMAL, Black,   Green   },
      [PROCESS_TOMB]                = { A_NORMAL, Black,   Red     },
      [PROCESS_THREAD]              = { A_NORMAL, Blue,    Default },
      [PROCESS_THREAD_BASENAME]     = { A_BOLD,   Blue,    Default },
      [PROCESS_COMM]                = { A_NORMAL, Magenta, Default },
      [PROCESS_THREAD_COMM]         = { A_NORMAL, Yellow,  Default },
      [BAR_BORDER]                  = { A_NORMAL, Blue,    Default },
      [BAR_SHADOW]                  = { A_NORMAL, Gray,    Default },
      [SWAP]                        = { A_NORMAL, Red,     Default },
      [GRAPH_1]                     = { A_BOLD,   Cyan,    Default },
      [GRAPH_2]                     = { A_NORMAL, Cyan,    Default },
      [MEMORY_USED]                 = { A_NORMAL, Green,   Default },
      [MEMORY_BUFFERS]              = { A_NORMAL, Cyan,    Default },
      [MEMORY_BUFFERS_TEXT]         = { A_NORMAL, Cyan,    Default },
      [MEMORY_CACHE]                = { A_NORMAL, Yellow,  Default },
      [LOAD_AVERAGE_FIFTEEN]        = { A_NORMAL, Default, Default },
      [LOAD_AVERAGE_FIVE]           = { A_NORMAL, Default, Default },
      [LOAD_AVERAGE_ONE]            = { A_NORMAL, Default, Default },
      [LOAD]                        = { A_NORMAL, White,   Default },
      [HELP_BOLD]                   = { A_NORMAL, Blue,    Default },
      [CLOCK]                       = { A_NORMAL, White,   Default },
      [DATE]                        = { A_NORMAL, White,   Default },
      [DATETIME]                    = { A_NORMAL, White,   Default },
      [CHECK_BOX]                   = { A_NORMAL, Blue,    Default },
      [CHECK_MARK]                  = { A_NORMAL, Default, Default },
      [CHECK_TEXT]                  = { A_NORMAL, Default, Default },
      [HOSTNAME]                    = { A_NORMAL, White,   Default },
      [CPU_NICE]                    = { A_NORMAL, Cyan,    Default },
      [CPU_NICE_TEXT]               = { A_NORMAL, Cyan,    Default },
      [CPU_NORMAL]                  = { A_NORMAL, Green,   Default },
      [CPU_SYSTEM]                  = { A_NORMAL, Red,     Default },
      [CPU_IOWAIT]                  = { A_BOLD,   Default, Default },
      [CPU_IRQ]                     = { A_BOLD,   Blue,    Default },
      [CPU_SOFTIRQ]                 = { A_NORMAL, Blue,    Default },
      [CPU_STEAL]                   = { A_NORMAL, Default, Default },
      [CPU_GUEST]                   = { A_NORMAL, Default, Default },
      [PRESSURE_STALL_THREEHUNDRED] = { A_NORMAL, Default, Default },
      [PRESSURE_STALL_SIXTY]        = { A_NORMAL, Default, Default },
      [PRESSURE_STALL_TEN]          = { A_NORMAL, Default, Default },
      [ZFS_MFU]                     = { A_NORMAL, Cyan,    Default },
      [ZFS_MRU]                     = { A_NORMAL, Yellow,  Default },
      [ZFS_ANON]                    = { A_BOLD,   Magenta, Default },
      [ZFS_HEADER]                  = { A_NORMAL, Default, Default },
      [ZFS_OTHER]                   = { A_BOLD,   Magenta, Default },
      [ZFS_COMPRESSED]              = { A_NORMAL, Cyan,    Default },
      [ZFS_RATIO]                   = { A_BOLD,   Magenta, Default },
      [ZRAM]                        = { A_NORMAL, Yellow,  Default },
   },

   [COLORSCHEME_MIDNIGHT] = {
      [RESET_COLOR]                 = { A_NORMAL, White,   Blue    },
      [DEFAULT_COLOR]               = { A_NORMAL, White,   Blue    },
      [FUNCTION_BAR]                = { A_NORMAL, Black,   Cyan    },
      [FUNCTION_KEY]                = { A_NORMAL, Default, Default },
      [PANEL_HEADER_FOCUS]          = { A_NORMAL, Black,   Cyan    },
      [PANEL_HEADER_UNFOCUS]        = { A_NORMAL, Black,   Cyan    },
      [PANEL_SELECTION_FOCUS]       = { A_NORMAL, Black,   White   },
      [PANEL_SELECTION_FOLLOW]      = { A_NORMAL, Black,   Yellow  },
      [PANEL_SELECTION_UNFOCUS]     = { A_BOLD,   Yellow,  Blue    },
      [FAILED_SEARCH]               = { A_NORMAL, Red,     Cyan    },
      [FAILED_READ]                 = { A_BOLD,   Red,     Blue    },
      [PAUSED]                      = { A_BOLD,   Yellow,  Cyan    },
      [UPTIME]                      = { A_BOLD,   Yellow,  Blue    },
      [BATTERY]                     = { A_BOLD,   Yellow,  Blue    },
      [LARGE_NUMBER]                = { A_BOLD,   Red,     Blue    },
      [METER_TEXT]                  = { A_NORMAL, Cyan,    Blue    },
      [METER_VALUE]                 = { A_BOLD,   Cyan,    Blue    },
      [METER_VALUE_ERROR]           = { A_BOLD,   Red,     Blue    },
      [METER_VALUE_IOREAD]          = { A_NORMAL, Green,   Blue    },
      [METER_VALUE_IOWRITE]         = { A_NORMAL, Black,   Blue    },
      [METER_VALUE_NOTICE]          = { A_BOLD,   White,   Blue    },
      [METER_VALUE_OK]              = { A_NORMAL, Green,   Blue    },
      [METER_VALUE_WARN]            = { A_BOLD,   Yellow,  Default },
      [LED_COLOR]                   = { A_NORMAL, Green,   Blue    },
      [TASKS_RUNNING]               = { A_BOLD,   Green,   Blue    },
      [PROCESS]                     = { A_NORMAL, White,   Blue    },
      [PROCESS_SHADOW]              = { A_BOLD,   Black,   Blue    },
      [PROCESS_TAG]                 = { A_BOLD,   Yellow,  Blue    },
      [PROCESS_MEGABYTES]           = { A_NORMAL, Cyan,    Blue    },
      [PROCESS_GIGABYTES]           = { A_NORMAL, Green,   Blue    },
      [PROCESS_BASENAME]            = { A_BOLD,   Cyan,    Blue    },
      [PROCESS_TREE]                = { A_NORMAL, Cyan,    Blue    },
      [PROCESS_R_STATE]             = { A_NORMAL, Green,   Blue    },
      [PROCESS_D_STATE]             = { A_BOLD,   Red,     Blue    },
      [PROCESS_HIGH_PRIORITY]       = { A_NORMAL, Red,     Blue    },
      [PROCESS_LOW_PRIORITY]        = { A_NORMAL, Green,   Blue    },
      [PROCESS_NEW]                 = { A_NORMAL, Blue,    Green   },
      [PROCESS_TOMB]                = { A_NORMAL, Blue,    Red     },
      [PROCESS_THREAD]              = { A_NORMAL, Green,   Blue    },
      [PROCESS_THREAD_BASENAME]     = { A_BOLD,   Green,   Blue    },
      [PROCESS_COMM]                = { A_NORMAL, Magenta, Blue    },
      [PROCESS_THREAD_COMM]         = { A_NORMAL, Black,   Blue    },
      [BAR_BORDER]                  = { A_BOLD,   Yellow,  Blue    },
      [BAR_SHADOW]                  = { A_NORMAL, Cyan,    Blue    },
      [SWAP]                        = { A_NORMAL, Red,     Blue    },
      [GRAPH_1]                     = { A_BOLD,   Cyan,    Blue    },
      [GRAPH_2]                     = { A_NORMAL, Cyan,    Blue    },
      [MEMORY_USED]                 = { A_BOLD,   Green,   Blue    },
      [MEMORY_BUFFERS]              = { A_BOLD,   Cyan,    Blue    },
      [MEMORY_BUFFERS_TEXT]         = { A_BOLD,   Cyan,    Blue    },
      [MEMORY_CACHE]                = { A_BOLD,   Yellow,  Blue    },
      [LOAD_AVERAGE_FIFTEEN]        = { A_BOLD,   Black,   Blue    },
      [LOAD_AVERAGE_FIVE]           = { A_NORMAL, White,   Blue    },
      [LOAD_AVERAGE_ONE]            = { A_BOLD,   White,   Blue    },
      [LOAD]                        = { A_BOLD,   White,   Blue    },
      [HELP_BOLD]                   = { A_BOLD,   Cyan,    Blue    },
      [CLOCK]                       = { A_NORMAL, White,   Blue    },
      [DATE]                        = { A_NORMAL, White,   Blue    },
      [DATETIME]                    = { A_NORMAL, White,   Blue    },
      [CHECK_BOX]                   = { A_NORMAL, Cyan,    Blue    },
      [CHECK_MARK]                  = { A_BOLD,   White,   Blue    },
      [CHECK_TEXT]                  = { A_NORMAL, White,   Blue    },
      [HOSTNAME]                    = { A_NORMAL, White,   Blue    },
      [CPU_NICE]                    = { A_BOLD,   Cyan,    Blue    },
      [CPU_NICE_TEXT]               = { A_BOLD,   Cyan,    Blue    },
      [CPU_NORMAL]                  = { A_BOLD,   Green,   Blue    },
      [CPU_SYSTEM]                  = { A_BOLD,   Red,     Blue    },
      [CPU_IOWAIT]                  = { A_BOLD,   Black,   Blue    },
      [CPU_IRQ]                     = { A_BOLD,   Black,   Blue    },
      [CPU_SOFTIRQ]                 = { A_NORMAL, Black,   Blue    },
      [CPU_STEAL]                   = { A_NORMAL, White,   Blue    },
      [CPU_GUEST]                   = { A_NORMAL, White,   Blue    },
      [PRESSURE_STALL_THREEHUNDRED] = { A_BOLD,   Black,   Blue    },
      [PRESSURE_STALL_SIXTY]        = { A_NORMAL, White,   Blue    },
      [PRESSURE_STALL_TEN]          = { A_BOLD,   White,   Blue    },
      [ZFS_MFU]                     = { A_BOLD,   White,   Blue    },
      [ZFS_MRU]                     = { A_BOLD,   Yellow,  Blue    },
      [ZFS_ANON]                    = { A_BOLD,   Magenta, Blue    },
      [ZFS_HEADER]                  = { A_BOLD,   Yellow,  Blue    },
      [ZFS_OTHER]                   = { A_BOLD,   Magenta, Blue    },
      [ZFS_COMPRESSED]              = { A_BOLD,   White,   Blue    },
      [ZFS_RATIO]                   = { A_BOLD,   Magenta, Blue    },
      [ZRAM]                        = { A_BOLD,   Yellow,  Blue    },
   },

   [COLORSCHEME_BLACKNIGHT] = {
      [RESET_COLOR]                 = { A_NORMAL,  Cyan,    Black  },
      [DEFAULT_COLOR]               = { A_NORMAL,  Cyan,    Black  },
      [FUNCTION_BAR]                = { A_NORMAL,  Black,   Green  },
      [FUNCTION_KEY]                = { A_NORMAL,  Cyan,    Black  },
      [PANEL_HEADER_FOCUS]          = { A_NORMAL,  Black,   Green  },
      [PANEL_HEADER_UNFOCUS]        = { A_NORMAL,  Black,   Green  },
      [PANEL_SELECTION_FOCUS]       = { A_NORMAL,  Black,   Cyan   },
      [PANEL_SELECTION_FOLLOW]      = { A_NORMAL,  Black,   Yellow },
      [PANEL_SELECTION_UNFOCUS]     = { A_NORMAL,  Black,   White  },
      [FAILED_SEARCH]               = { A_NORMAL,  Red,     Green  },
      [FAILED_READ]                 = { A_BOLD,    Red,     Black  },
      [PAUSED]                      = { A_BOLD,    Yellow,  Green  },
      [UPTIME]                      = { A_NORMAL,  Green,   Black  },
      [BATTERY]                     = { A_NORMAL,  Green,   Black  },
      [LARGE_NUMBER]                = { A_BOLD,    Red,     Black  },
      [METER_TEXT]                  = { A_NORMAL,  Cyan,    Black  },
      [METER_VALUE]                 = { A_NORMAL,  Green,   Black  },
      [METER_VALUE_ERROR]           = { A_BOLD,    Red,     Black  },
      [METER_VALUE_IOREAD]          = { A_NORMAL,  Green,   Black  },
      [METER_VALUE_IOWRITE]         = { A_NORMAL,  Blue,    Black  },
      [METER_VALUE_NOTICE]          = { A_BOLD,    White,   Black  },
      [METER_VALUE_OK]              = { A_NORMAL,  Green,   Black  },
      [METER_VALUE_WARN]            = { A_BOLD,    Yellow,  Black  },
      [LED_COLOR]                   = { A_NORMAL,  Green,   Black  },
      [TASKS_RUNNING]               = { A_BOLD,    Green,   Black  },
      [PROCESS]                     = { A_NORMAL,  Cyan,    Black  },
      [PROCESS_SHADOW]              = { A_BOLD,    Gray,    Black  },
      [PROCESS_TAG]                 = { A_BOLD,    Yellow,  Black  },
      [PROCESS_MEGABYTES]           = { A_BOLD,    Green,   Black  },
      [PROCESS_GIGABYTES]           = { A_BOLD,    Yellow,  Black  },
      [PROCESS_BASENAME]            = { A_BOLD,    Green,   Black  },
      [PROCESS_TREE]                = { A_NORMAL,  Cyan,    Black  },
      [PROCESS_THREAD]              = { A_NORMAL,  Green,   Black  },
      [PROCESS_THREAD_BASENAME]     = { A_BOLD,    Blue,    Black  },
      [PROCESS_COMM]                = { A_NORMAL,  Magenta, Black  },
      [PROCESS_THREAD_COMM]         = { A_NORMAL,  Yellow,  Black  },
      [PROCESS_R_STATE]             = { A_NORMAL,  Green,   Black  },
      [PROCESS_D_STATE]             = { A_BOLD,    Red,     Black  },
      [PROCESS_HIGH_PRIORITY]       = { A_NORMAL,  Red,     Black  },
      [PROCESS_LOW_PRIORITY]        = { A_NORMAL,  Green,   Black  },
      [PROCESS_NEW]                 = { A_NORMAL,  Black,   Green  },
      [PROCESS_TOMB]                = { A_NORMAL,  Black,   Red    },
      [BAR_BORDER]                  = { A_BOLD,    Green,   Black  },
      [BAR_SHADOW]                  = { A_NORMAL,  Cyan,    Black  },
      [SWAP]                        = { A_NORMAL,  Red,     Black  },
      [GRAPH_1]                     = { A_BOLD,    Green,   Black  },
      [GRAPH_2]                     = { A_NORMAL,  Green,   Black  },
      [MEMORY_USED]                 = { A_NORMAL,  Green,   Black  },
      [MEMORY_BUFFERS]              = { A_NORMAL,  Blue,    Black  },
      [MEMORY_BUFFERS_TEXT]         = { A_BOLD,    Blue,    Black  },
      [MEMORY_CACHE]                = { A_NORMAL,  Yellow,  Black  },
      [LOAD_AVERAGE_FIFTEEN]        = { A_NORMAL,  Green,   Black  },
      [LOAD_AVERAGE_FIVE]           = { A_NORMAL,  Green,   Black  },
      [LOAD_AVERAGE_ONE]            = { A_BOLD,    Green,   Black  },
      [LOAD]                        = { A_BOLD,    Default, Black  },
      [HELP_BOLD]                   = { A_BOLD,    Cyan,    Black  },
      [CLOCK]                       = { A_NORMAL,  Green,   Black  },
      [CHECK_BOX]                   = { A_NORMAL,  Green,   Black  },
      [CHECK_MARK]                  = { A_BOLD,    Green,   Black  },
      [CHECK_TEXT]                  = { A_NORMAL,  Cyan,    Black  },
      [HOSTNAME]                    = { A_NORMAL,  Green,   Black  },
      [CPU_NICE]                    = { A_NORMAL,  Blue,    Black  },
      [CPU_NICE_TEXT]               = { A_BOLD,    Blue,    Black  },
      [CPU_NORMAL]                  = { A_NORMAL,  Green,   Black  },
      [CPU_SYSTEM]                  = { A_NORMAL,  Red,     Black  },
      [CPU_IOWAIT]                  = { A_NORMAL,  Yellow,  Black  },
      [CPU_IRQ]                     = { A_BOLD,    Blue,    Black  },
      [CPU_SOFTIRQ]                 = { A_NORMAL,  Blue,    Black  },
      [CPU_STEAL]                   = { A_NORMAL,  Cyan,    Black  },
      [CPU_GUEST]                   = { A_NORMAL,  Cyan,    Black  },
      [PRESSURE_STALL_THREEHUNDRED] = { A_NORMAL,  Green,   Black  },
      [PRESSURE_STALL_SIXTY]        = { A_NORMAL,  Green,   Black  },
      [PRESSURE_STALL_TEN]          = { A_BOLD,    Green,   Black  },
      [ZFS_MFU]                     = { A_NORMAL,  Blue,    Black  },
      [ZFS_MRU]                     = { A_NORMAL,  Yellow,  Black  },
      [ZFS_ANON]                    = { A_NORMAL,  Magenta, Black  },
      [ZFS_HEADER]                  = { A_NORMAL,  Yellow,  Black  },
      [ZFS_OTHER]                   = { A_NORMAL,  Magenta, Black  },
      [ZFS_COMPRESSED]              = { A_NORMAL,  Blue,    Black  },
      [ZFS_RATIO]                   = { A_NORMAL,  Magenta, Black  },
      [ZRAM]                        = { A_NORMAL,  Yellow,  Black  },
   },
};

int CRT_cursorX = 0;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

ColorScheme CRT_colorScheme;

ATTR_NORETURN
static void CRT_handleSIGTERM(int sgn) {
   (void) sgn;
   CRT_done();
   exit(0);
}

#ifdef HAVE_SETUID_ENABLED

static int CRT_euid = -1;

static int CRT_egid = -1;

void CRT_dropPrivileges() {
   CRT_egid = getegid();
   CRT_euid = geteuid();
   if (setegid(getgid()) == -1) {
      CRT_fatalError("Fatal error: failed dropping group privileges");
   }
   if (seteuid(getuid()) == -1) {
      CRT_fatalError("Fatal error: failed dropping user privileges");
   }
}

void CRT_restorePrivileges() {
   if (CRT_egid == -1 || CRT_euid == -1) {
      CRT_fatalError("Fatal error: internal inconsistency");
   }
   if (setegid(CRT_egid) == -1) {
      CRT_fatalError("Fatal error: failed restoring group privileges");
   }
   if (seteuid(CRT_euid) == -1) {
      CRT_fatalError("Fatal error: failed restoring user privileges");
   }
}

#endif /* HAVE_SETUID_ENABLED */

static struct sigaction old_sig_handler[32];

// TODO: pass an instance of Settings instead.

void CRT_init(const int* delay, int colorScheme, bool allowUnicode) {
   initscr();
   noecho();
   CRT_delay = delay;

   for (int i = 0; i < LAST_STYLE; i++) {
      Style style = CRT_colorSchemes[COLORSCHEME_DEFAULT][i];
      CRT_colorSchemes[COLORSCHEME_BROKENGRAY][i] = style.fg == Gray ? (Style){ 0, Default, Default } : style;
   }

   halfdelay(*CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   mouseinterval(0);
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
      define_key("\033[14;2~", KEY_F(15));
      define_key("\033[17;2~", KEY_F(18));
      char sequence[3] = "\033a";
      for (char c = 'a'; c <= 'z'; c++) {
         sequence[1] = c;
         define_key(sequence, KEY_ALT('A' + (c - 'a')));
      }
   }

   struct sigaction act;
   sigemptyset (&act.sa_mask);
   act.sa_flags = (int)SA_RESETHAND | SA_NODEFER;
   act.sa_handler = CRT_handleSIGSEGV;
   sigaction (SIGSEGV, &act, &old_sig_handler[SIGSEGV]);
   sigaction (SIGFPE, &act, &old_sig_handler[SIGFPE]);
   sigaction (SIGILL, &act, &old_sig_handler[SIGILL]);
   sigaction (SIGBUS, &act, &old_sig_handler[SIGBUS]);
   sigaction (SIGPIPE, &act, &old_sig_handler[SIGPIPE]);
   sigaction (SIGSYS, &act, &old_sig_handler[SIGSYS]);
   sigaction (SIGABRT, &act, &old_sig_handler[SIGABRT]);

   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);

   use_default_colors();
   CRT_setColors(has_colors() ? colorScheme : COLORSCHEME_MONOCHROME);

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

#if NCURSES_MOUSE_VERSION > 1
   mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
#else
   mousemask(BUTTON1_RELEASED, NULL);
#endif

   CRT_degreeSign = initDegreeSign();
}

void CRT_done() {
   curs_set(1);
   endwin();
}

void CRT_fatalError(const char* note) {
   const char* sysMsg = strerror(errno);
   CRT_done();
   fprintf(stderr, "%s: %s\n", note, sysMsg);
   exit(2);
}

int CRT_readKey() {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int ret = getch();
   halfdelay(*CRT_delay);
   return ret;
}

void CRT_disableDelay() {
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay() {
   halfdelay(*CRT_delay);
}

void CRT_setColors(int colorScheme) {
   for (int i = 0; i < LAST_STYLE; i++) {
      Style style = CRT_colorSchemes[colorScheme][i];
      if (style.fg == Gray && Gray >= COLORS)
         style.fg = Black;
      init_pair(i + 1, style.fg, style.bg);
   }

   CRT_colorScheme = colorScheme;
}

int CRT_getAttrs(StyleId styleId) {
   return COLOR_PAIR(styleId + 1) | CRT_colorSchemes[CRT_colorScheme][styleId].attrs;
}

void CRT_handleSIGSEGV(int signal) {
   CRT_done();

   fprintf(stderr, "\n\n"
      "FATAL PROGRAM ERROR DETECTED\n"
      "============================\n"
      "Please check at https://htop.dev/issues whether this issue has already been reported.\n"
      "If no similar issue has been reported before, please create a new issue with the following information:\n"
      "\n"
      "- Your htop version (htop --version)\n"
      "- Your OS and kernel version (uname -a)\n"
      "- Your distribution and release (lsb_release -a)\n"
      "- Likely steps to reproduce (How did it happened?)\n"
   );

#ifdef HAVE_EXECINFO_H
   fprintf(stderr, "- Backtrace of the issue (see below)\n");
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

#ifdef HAVE_EXECINFO_H
   fprintf(stderr,
      "Backtrace information:\n"
      "----------------------\n"
      "The following function calls were active when the issue was detected:\n"
      "---\n"
   );

   void *backtraceArray[256];

   size_t size = backtrace(backtraceArray, ARRAYSIZE(backtraceArray));
   backtrace_symbols_fd(backtraceArray, size, 2);
   fprintf(stderr,
      "---\n"
      "\n"
      "To make the above information more practical to work with,\n"
      "you should provide a disassembly of your binary.\n"
      "This can usually be done by running the following command:\n"
      "\n"
   );

#ifdef HTOP_DARWIN
   fprintf(stderr, "   otool -tvV `which htop` > ~/htop.otool\n");
#else
   fprintf(stderr, "   objdump -d -S -w `which htop` > ~/htop.objdump\n");
#endif

   fprintf(stderr,
      "\n"
      "Please include the generated file in your report.\n"
      "\n"
   );
#endif

   fprintf(stderr,
      "Running this program with debug symbols or inside a debugger may provide further insights.\n"
      "\n"
      "Thank you for helping to improve htop!\n"
      "\n"
      "htop " VERSION " aborting.\n"
      "\n"
   );

   /* Call old sigsegv handler; may be default exit or third party one (e.g. ASAN) */
   if (sigaction (signal, &old_sig_handler[signal], NULL) < 0) {
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
