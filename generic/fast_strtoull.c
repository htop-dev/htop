/*
htop - generic/fast_strtoull.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "generic/fast_strtoull.h"

#include <stdint.h>


uint64_t fast_strtoull_dec(char** str, int maxlen) {
   register uint64_t result = 0;

   if (!maxlen)
      --maxlen;

   while (maxlen-- && **str >= '0' && **str <= '9') {
      result *= 10;
      result += **str - '0';
      (*str)++;
   }

   return result;
}

uint64_t fast_strtoull_hex(char** str, int maxlen) {
   register uint64_t result = 0;
   register int nibble, letter;
   const long valid_mask = 0x03FF007E;

   if (!maxlen)
      --maxlen;

   while (maxlen--) {
      nibble = (unsigned char)**str;
      if (!(valid_mask & (1 << (nibble & 0x1F))))
         break;
      if ((nibble < '0') || (nibble & ~0x20) > 'F')
         break;
      letter = (nibble & 0x40) ? 'A' - '9' - 1 : 0;
      nibble &=~0x20; // to upper
      nibble ^= 0x10; // switch letters and digits
      nibble -= letter;
      nibble &= 0x0f;
      result <<= 4;
      result += (uint64_t)nibble;
      (*str)++;
   }

   return result;
}
