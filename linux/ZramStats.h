#ifndef HEADER_ZramStats
#define HEADER_ZramStats

typedef struct ZramStats_ {
   memory_t totalZram;
   memory_t usedZramComp;
   memory_t usedZramOrig;
} ZramStats;

#endif
