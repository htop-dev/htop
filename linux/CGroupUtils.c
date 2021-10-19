/*
htop - CGroupUtils.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/CGroupUtils.h"

#include "XUtils.h"


static bool CGroup_filterName_internal(const char *cgroup, char* buf, size_t bufsize) {
   const char* str_slice_suffix = ".slice";
   const char* str_system_slice = "system.slice";
   const char* str_user_slice = "user.slice";
   const char* str_machine_slice = "machine.slice";
   const char* str_user_slice_prefix = "/user-";

   const char* str_lxc_monitor_prefix = "lxc.monitor.";
   const char* str_lxc_payload_prefix = "lxc.payload.";

   const char* str_nspawn_scope_prefix = "machine-";
   const char* str_nspawn_monitor_label = "/supervisor";

   const char* str_service_suffix = ".service";
   const char* str_scope_suffix = ".scope";

   while(*cgroup) {
      if ('/' == *cgroup) {
         while ('/' == *cgroup)
            cgroup++;

         if (!bufsize)
            return false;

         *buf++ = '/';
         bufsize--;

         continue;
      }

      const char* labelStart = cgroup;
      const char* nextSlash = strchrnul(labelStart, '/');
      const size_t labelLen = nextSlash - labelStart;

      if (String_startsWith(cgroup, str_system_slice)) {
         cgroup += strlen(str_system_slice);

         if (*cgroup && *cgroup != '/')
            goto handle_default;

         if (bufsize < 3)
            return false;

         *buf++ = '[';
         *buf++ = 'S';
         *buf++ = ']';
         bufsize -= 3;

         continue;
      }

      if (String_startsWith(cgroup, str_machine_slice)) {
         cgroup += strlen(str_machine_slice);

         if (*cgroup && *cgroup != '/')
            goto handle_default;

         if (bufsize < 3)
            return false;

         *buf++ = '[';
         *buf++ = 'M';
         *buf++ = ']';
         bufsize -= 3;

         continue;
      }

      if (String_startsWith(cgroup, str_user_slice)) {
         cgroup += strlen(str_user_slice);

         if (*cgroup && *cgroup != '/')
            goto handle_default;

         if (bufsize < 3)
            return false;

         *buf++ = '[';
         *buf++ = 'U';
         *buf++ = ']';
         bufsize -= 3;

         if (!String_startsWith(cgroup, str_user_slice_prefix))
            continue;

         const char* userSliceSlash = strchrnul(cgroup + strlen(str_user_slice_prefix), '/');
         const char* sliceSpec = userSliceSlash - strlen(str_slice_suffix);

         if (!String_startsWith(sliceSpec, str_slice_suffix))
            continue;

         const size_t sliceNameLen = sliceSpec - (cgroup + strlen(str_user_slice_prefix));

         if (bufsize < sliceNameLen + 1)
            return false;

         buf[-1] = ':';

         cgroup += strlen(str_user_slice_prefix);
         while(cgroup < sliceSpec) {
            *buf++ = *cgroup++;
            bufsize--;
         }
         cgroup = userSliceSlash;

         *buf++ = ']';
         bufsize--;

         continue;
      }

      if (labelLen > strlen(str_slice_suffix) && String_startsWith(nextSlash - strlen(str_slice_suffix), str_slice_suffix)) {
         const size_t sliceNameLen = labelLen - strlen(str_slice_suffix);
         if (bufsize < 2 + sliceNameLen)
            return false;

         *buf++ = '[';
         bufsize--;

         for(size_t i = sliceNameLen; i; i--) {
            *buf++ = *cgroup++;
            bufsize--;
         }

         *buf++ = ']';
         bufsize--;

         cgroup = nextSlash;

         continue;
      }

      if (String_startsWith(cgroup, str_lxc_payload_prefix)) {
         const size_t cgroupNameLen = labelLen - strlen(str_lxc_payload_prefix);
         if (bufsize < 6 + cgroupNameLen)
            return false;

         *buf++ = '[';
         *buf++ = 'l';
         *buf++ = 'x';
         *buf++ = 'c';
         *buf++ = ':';
         bufsize -= 5;

         cgroup += strlen(str_lxc_payload_prefix);
         while(cgroup < nextSlash) {
            *buf++ = *cgroup++;
            bufsize--;
         }

         *buf++ = ']';
         bufsize--;

         continue;
      }

      if (String_startsWith(cgroup, str_lxc_monitor_prefix)) {
         const size_t cgroupNameLen = labelLen - strlen(str_lxc_monitor_prefix);
         if (bufsize < 6 + cgroupNameLen)
            return false;

         *buf++ = '[';
         *buf++ = 'L';
         *buf++ = 'X';
         *buf++ = 'C';
         *buf++ = ':';
         bufsize -= 5;

         cgroup += strlen(str_lxc_monitor_prefix);
         while(cgroup < nextSlash) {
            *buf++ = *cgroup++;
            bufsize--;
         }

         *buf++ = ']';
         bufsize--;

         continue;
      }

      if (labelLen > strlen(str_service_suffix) && String_startsWith(nextSlash - strlen(str_service_suffix), str_service_suffix)) {
         const size_t serviceNameLen = labelLen - strlen(str_service_suffix);

         if (String_startsWith(cgroup, "user@")) {
            cgroup = nextSlash;

            while(*cgroup == '/')
               cgroup++;

            continue;
         }

         if (bufsize < serviceNameLen)
            return false;

         for(size_t i = serviceNameLen; i; i--) {
            *buf++ = *cgroup++;
            bufsize--;
         }

         cgroup = nextSlash;

         continue;
      }

      if (labelLen > strlen(str_scope_suffix) && String_startsWith(nextSlash - strlen(str_scope_suffix), str_scope_suffix)) {
         const size_t scopeNameLen = labelLen - strlen(str_scope_suffix);

         if (String_startsWith(cgroup, str_nspawn_scope_prefix)) {
            const size_t machineScopeNameLen = scopeNameLen - strlen(str_nspawn_scope_prefix);
            if (bufsize < 6 + machineScopeNameLen)
               return false;

            const bool is_monitor = String_startsWith(nextSlash, str_nspawn_monitor_label);

            *buf++ = '[';
            *buf++ = is_monitor ? 'S' : 's';
            *buf++ = is_monitor ? 'N' : 'n';
            *buf++ = is_monitor ? 'C' : 'c';
            *buf++ = ':';
            bufsize -= 5;

            cgroup += strlen(str_nspawn_scope_prefix);
            for(size_t i = machineScopeNameLen; i; i--) {
               *buf++ = *cgroup++;
               bufsize--;
            }

            *buf++ = ']';
            bufsize--;

            cgroup = nextSlash;

            continue;
         }

         if (bufsize < 1 + scopeNameLen)
            return false;

         *buf++ = '!';
         bufsize--;

         for(size_t i = scopeNameLen; i; i--) {
            *buf++ = *cgroup++;
            bufsize--;
         }

         cgroup = nextSlash;

         continue;
      }

handle_default:
      // Default behavior: Copy the full label
      cgroup = labelStart;

      if (bufsize < (size_t)(nextSlash - cgroup))
         return false;

      while(cgroup < nextSlash) {
         *buf++ = *cgroup++;
         bufsize--;
      }
   }

   return true;
}

bool CGroup_filterName(const char *cgroup, char* buf, size_t bufsize) {
   memset(buf, 0, bufsize);

   return CGroup_filterName_internal(cgroup, buf, bufsize - 1);
}
