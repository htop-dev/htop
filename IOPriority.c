/*
htop - IOPriority.c
(C) 2004-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.

Based on ionice, 
Copyright (C) 2005 Jens Axboe <jens@axboe.dk>
Released under the terms of the GNU General Public License version 2
*/

#include "IOPriority.h"

/*{

enum {
   IOPRIO_CLASS_NONE,
   IOPRIO_CLASS_RT,
   IOPRIO_CLASS_BE,
   IOPRIO_CLASS_IDLE,
};

#define IOPRIO_WHO_PROCESS 1

#define IOPRIO_CLASS_SHIFT (13)
#define IOPRIO_PRIO_MASK ((1UL << IOPRIO_CLASS_SHIFT) - 1)

#define IOPriority_class(ioprio_) ((int) ((ioprio_) >> IOPRIO_CLASS_SHIFT) )
#define IOPriority_data(ioprio_) ((int) ((ioprio_) & IOPRIO_PRIO_MASK) )

typedef int IOPriority;

#define IOPriority_tuple(class_, data_) (((class_) << IOPRIO_CLASS_SHIFT) | data_)

#define IOPriority_error 0xffffffff

#define IOPriority_None IOPriority_tuple(IOPRIO_CLASS_NONE, 0)
#define IOPriority_Idle IOPriority_tuple(IOPRIO_CLASS_IDLE, 0)

}*/

