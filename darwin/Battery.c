
#include "BatteryMeter.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

void Battery_getData(double* level, ACPresence* isOnAC) {
   CFTypeRef power_sources = IOPSCopyPowerSourcesInfo();

   *level = -1;
   *isOnAC = AC_ERROR;

   if(NULL == power_sources) {
      return;
   }

   if(power_sources != NULL) {
      CFArrayRef list = IOPSCopyPowerSourcesList(power_sources);
      CFDictionaryRef battery = NULL;
      int len;

      if(NULL == list) {
         CFRelease(power_sources);

         return;
      }

      len = CFArrayGetCount(list);

      /* Get the battery */
      for(int i = 0; i < len && battery == NULL; ++i) {
         CFDictionaryRef candidate = IOPSGetPowerSourceDescription(power_sources,
                                     CFArrayGetValueAtIndex(list, i)); /* GET rule */
         CFStringRef type;

         if(NULL != candidate) {
            type = (CFStringRef) CFDictionaryGetValue(candidate,
                   CFSTR(kIOPSTransportTypeKey)); /* GET rule */

            if(kCFCompareEqualTo == CFStringCompare(type, CFSTR(kIOPSInternalType), 0)) {
               CFRetain(candidate);
               battery = candidate;
            }
         }
      }

      if(NULL != battery) {
         /* Determine the AC state */
         CFStringRef power_state = CFDictionaryGetValue(battery, CFSTR(kIOPSPowerSourceStateKey));

         *isOnAC = (kCFCompareEqualTo == CFStringCompare(power_state, CFSTR(kIOPSACPowerValue), 0))
                 ? AC_PRESENT
                 : AC_ABSENT;

         /* Get the percentage remaining */
         double current;
         double max;

         CFNumberGetValue(CFDictionaryGetValue(battery, CFSTR(kIOPSCurrentCapacityKey)),
                 kCFNumberDoubleType, &current);
         CFNumberGetValue(CFDictionaryGetValue(battery, CFSTR(kIOPSMaxCapacityKey)),
                 kCFNumberDoubleType, &max);

         *level = (current * 100.0) / max;

         CFRelease(battery);
      }

      CFRelease(list);
      CFRelease(power_sources);
   }
}
