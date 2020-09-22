#ifndef HEADER_ZramStats
#define HEADER_ZramStats

typedef struct ZramStats_ {
   unsigned long long int totalZram;
   unsigned long long int usedZramComp;
   unsigned long long int usedZramOrig;
} ZramStats;

#endif
