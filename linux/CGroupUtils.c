/*
htop - CGroupUtils.h
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "linux/CGroupUtils.h"

#include "XUtils.h"


typedef struct StrBuf_state {
   char *buf;
   size_t size;
   size_t pos;
} StrBuf_state;

typedef bool (*StrBuf_putc_t)(StrBuf_state* p, char c);

static bool StrBuf_putc_count(StrBuf_state* p, ATTR_UNUSED char c) {
   p->pos++;
   return true;
}

static bool StrBuf_putc_write(StrBuf_state* p, char c) {
   if (p->pos >= p->size)
      return false;

   p->buf[p->pos] = c;
   p->pos++;
   return true;
}

static bool StrBuf_putsn(StrBuf_state* p, StrBuf_putc_t w, const char* s, size_t count) {
   for (; count; count--)
      if (!w(p, *s++))
         return false;

   return true;
}

static bool StrBuf_putsz(StrBuf_state* p, StrBuf_putc_t w, const char* s) {
   while (*s)
      if (!w(p, *s++))
         return false;

   return true;
}

static bool Label_checkEqual(const char* labelStart, size_t labelLen, const char* expected) {
   return labelLen == strlen(expected) && String_startsWith(labelStart, expected);
}

static bool Label_checkPrefix(const char* labelStart, size_t labelLen, const char* expected) {
   return labelLen > strlen(expected) && String_startsWith(labelStart, expected);
}

static bool Label_checkSuffix(const char* labelStart, size_t labelLen, const char* expected) {
   return labelLen > strlen(expected) && String_startsWith(labelStart + labelLen - strlen(expected), expected);
}

static bool CGroup_filterName_internal(const char *cgroup, StrBuf_state* s, StrBuf_putc_t w) {
   const char* str_slice_suffix = ".slice";
   const char* str_system_slice = "system.slice";
   const char* str_user_slice = "user.slice";
   const char* str_machine_slice = "machine.slice";
   const char* str_user_slice_prefix = "/user-";
   const char* str_system_slice_prefix = "/system-";

   const char* str_lxc_monitor_legacy = "lxc.monitor";
   const char* str_lxc_payload_legacy = "lxc.payload";
   const char* str_lxc_monitor_prefix = "lxc.monitor.";
   const char* str_lxc_payload_prefix = "lxc.payload.";

   const char* str_nspawn_scope_prefix = "machine-";
   const char* str_nspawn_monitor_label = "/supervisor";
   const char* str_nspawn_payload_label = "/payload";

   const char* str_snap_scope_prefix = "snap.";

   const char* str_service_suffix = ".service";
   const char* str_scope_suffix = ".scope";

   while (*cgroup) {
      if ('/' == *cgroup) {
         while ('/' == *cgroup)
            cgroup++;

         if (!w(s, '/'))
            return false;

         continue;
      }

      const char* labelStart = cgroup;
      const char* nextSlash = strchrnul(labelStart, '/');
      const size_t labelLen = nextSlash - labelStart;

      if (Label_checkEqual(labelStart, labelLen, str_system_slice)) {
         cgroup = nextSlash;

         if (!StrBuf_putsz(s, w, "[S]"))
            return false;

         if (String_startsWith(cgroup, str_system_slice_prefix)) {
            cgroup = strchrnul(cgroup + 1, '/');
            continue;
         }

         continue;
      }

      if (Label_checkEqual(labelStart, labelLen, str_machine_slice)) {
         cgroup = nextSlash;

         if (!StrBuf_putsz(s, w, "[M]"))
            return false;

         continue;
      }

      if (Label_checkEqual(labelStart, labelLen, str_user_slice)) {
         cgroup = nextSlash;

         if (!StrBuf_putsz(s, w, "[U]"))
            return false;

         if (!String_startsWith(cgroup, str_user_slice_prefix))
            continue;

         const char* userSliceSlash = strchrnul(cgroup + strlen(str_user_slice_prefix), '/');
         const char* sliceSpec = userSliceSlash - strlen(str_slice_suffix);

         if (!String_startsWith(sliceSpec, str_slice_suffix))
            continue;

         const size_t sliceNameLen = sliceSpec - (cgroup + strlen(str_user_slice_prefix));

         s->pos--;
         if (!w(s, ':'))
            return false;

         if (!StrBuf_putsn(s, w, cgroup + strlen(str_user_slice_prefix), sliceNameLen))
            return false;

         if (!w(s, ']'))
            return false;

         cgroup = userSliceSlash;

         continue;
      }

      if (Label_checkSuffix(labelStart, labelLen, str_slice_suffix)) {
         const size_t sliceNameLen = labelLen - strlen(str_slice_suffix);

         if (!w(s, '['))
            return false;

         if (!StrBuf_putsn(s, w, cgroup, sliceNameLen))
            return false;

         if (!w(s, ']'))
            return false;

         cgroup = nextSlash;

         continue;
      }

      if (Label_checkPrefix(labelStart, labelLen, str_lxc_payload_prefix)) {
         const size_t cgroupNameLen = labelLen - strlen(str_lxc_payload_prefix);

         if (!StrBuf_putsz(s, w, "[lxc:"))
            return false;

         if (!StrBuf_putsn(s, w, cgroup + strlen(str_lxc_payload_prefix), cgroupNameLen))
            return false;

         if (!w(s, ']'))
            return false;

         cgroup = nextSlash;

         continue;
      }

      if (Label_checkPrefix(labelStart, labelLen, str_lxc_monitor_prefix)) {
         const size_t cgroupNameLen = labelLen - strlen(str_lxc_monitor_prefix);

         if (!StrBuf_putsz(s, w, "[LXC:"))
            return false;

         if (!StrBuf_putsn(s, w, cgroup + strlen(str_lxc_monitor_prefix), cgroupNameLen))
            return false;

         if (!w(s, ']'))
            return false;

         cgroup = nextSlash;

         continue;
      }

      // LXC legacy cgroup naming
      if (Label_checkEqual(labelStart, labelLen, str_lxc_monitor_legacy) ||
         Label_checkEqual(labelStart, labelLen, str_lxc_payload_legacy)) {
         bool isMonitor = Label_checkEqual(labelStart, labelLen, str_lxc_monitor_legacy);

         labelStart = nextSlash;
         while (*labelStart == '/')
            labelStart++;

         nextSlash = strchrnul(labelStart, '/');
         if (nextSlash - labelStart > 0) {
            if (!StrBuf_putsz(s, w, isMonitor ? "[LXC:" : "[lxc:"))
               return false;

            if (!StrBuf_putsn(s, w, labelStart, nextSlash - labelStart))
               return false;

            if (!w(s, ']'))
               return false;

            cgroup = nextSlash;
            continue;
         }

         labelStart = cgroup;
         nextSlash = labelStart + labelLen;
      }

      if (Label_checkSuffix(labelStart, labelLen, str_service_suffix)) {
         const size_t serviceNameLen = labelLen - strlen(str_service_suffix);

         if (String_startsWith(cgroup, "user@")) {
            cgroup = nextSlash;

            while(*cgroup == '/')
               cgroup++;

            continue;
         }

         if (!StrBuf_putsn(s, w, cgroup, serviceNameLen))
            return false;

         cgroup = nextSlash;

         continue;
      }

      if (Label_checkSuffix(labelStart, labelLen, str_scope_suffix)) {
         const size_t scopeNameLen = labelLen - strlen(str_scope_suffix);

         if (Label_checkPrefix(labelStart, scopeNameLen, str_nspawn_scope_prefix)) {
            const size_t machineScopeNameLen = scopeNameLen - strlen(str_nspawn_scope_prefix);

            const bool is_monitor = String_startsWith(nextSlash, str_nspawn_monitor_label);

            if (!StrBuf_putsz(s, w, is_monitor ? "[SNC:" : "[snc:"))
               return false;

            if (!StrBuf_putsn(s, w, cgroup + strlen(str_nspawn_scope_prefix), machineScopeNameLen))
               return false;

            if (!w(s, ']'))
               return false;

            cgroup = nextSlash;
            if (String_startsWith(nextSlash, str_nspawn_monitor_label))
               cgroup += strlen(str_nspawn_monitor_label);
            else if (String_startsWith(nextSlash, str_nspawn_payload_label))
               cgroup += strlen(str_nspawn_payload_label);

            continue;
         } else if(Label_checkPrefix(labelStart, scopeNameLen, str_snap_scope_prefix)) {
            const char* nextDot = strchrnul(labelStart + strlen(str_snap_scope_prefix), '.');

            if (!StrBuf_putsz(s, w, "!snap:"))
               return false;

            if (nextDot >= labelStart + scopeNameLen) {
               nextDot = labelStart + scopeNameLen;
            }

            if (!StrBuf_putsn(s, w, labelStart + strlen(str_snap_scope_prefix), nextDot - (labelStart + strlen(str_snap_scope_prefix))))
               return false;

            cgroup = nextSlash;

            continue;
         }

         if (!w(s, '!'))
            return false;

         if (!StrBuf_putsn(s, w, cgroup, scopeNameLen))
            return false;

         cgroup = nextSlash;

         continue;
      }

      // Default behavior: Copy the full label
      cgroup = labelStart;

      if (!StrBuf_putsn(s, w, cgroup, labelLen))
         return false;

      cgroup = nextSlash;
   }

   return true;
}

char* CGroup_filterName(const char *cgroup) {
   StrBuf_state s = {
      .buf = NULL,
      .size = 0,
      .pos = 0,
   };

   if (!CGroup_filterName_internal(cgroup, &s, StrBuf_putc_count)) {
      return NULL;
   }

   s.buf = xCalloc(s.pos + 1, sizeof(char));
   s.size = s.pos;
   s.pos = 0;

   if (!CGroup_filterName_internal(cgroup, &s, StrBuf_putc_write)) {
      free(s.buf);
      return NULL;
   }

   s.buf[s.size] = '\0';
   return s.buf;
}
