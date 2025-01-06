/*
htop - FlexMeter.c
(C) 2024 Stoyan Bogdanov
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <dirent.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "CRT.h"
#include "config.h"
#include "FlexMeter.h"
#include "Object.h"


#define FLEX_CFG_FOLDER ".config/htop/FlexMeter"

typedef struct {
   char* name;
   char* command;
   char* type;
   char* caption;
   char* uiName;
} _flex_meter;

_flex_meter meter_list[METERS_LIST_SIZE];

static int meters_count = 0;

static const int DateMeter_attributes[] = {
   FLEX
};

MeterClass* FlexMeter_class = NULL;

static int check_for_meters(void);

static bool parse_input(_flex_meter *meter, char* line)
{
   switch(line[0])
   {
      case 'n':
         if (String_startsWith(line, "name=")) {
            xAsprintf(&meter->uiName, "Flex: %s", line + 5);
         }
         break;
      case 'c':
         if (String_startsWith(line, "command=")) {
            meter->command = xStrdup(line + 8);
         } else if (String_startsWith(line, "caption=")) {
            meter->caption = xStrdup(line + 8);
         }
         break;
      case 't':
         if (String_startsWith(line, "type=")) {
            meter->type = xStrdup(line + 6);
         }
         break;
      default:
         return false;
   }

   return true;
}

static bool load_config(_flex_meter *meter, char* file)
{
   bool ret = false;
   FILE* fp = fopen(file, "r");

   if (fp != NULL) {
      char* buff;
      while ((buff = String_readLine(fp)) != NULL) {
         ret = parse_input(meter, buff);
         if (!ret) {
            break;
         }
      }
      free(buff);
      buff = NULL;
   }

   fclose(fp);
   return ret;
}

static int check_for_meters(void)
{
   char* path;
   struct dirent* dir;
   struct passwd* pw = getpwuid(getuid());
   const char* homedir = pw->pw_dir;
   char* home = NULL;
   bool ret;

   const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
   const char* homedirEnv = getenv("HOME");

   if (xdgConfigHome) {
      xAsprintf(&home, "%s/%s", xdgConfigHome, "htop/FlexMeter");
   } else if (homedirEnv) {
      xAsprintf(&home, "%s/%s", homedirEnv, FLEX_CFG_FOLDER);
   } else {
      xAsprintf(&home, "%s/%s", homedir, FLEX_CFG_FOLDER);
   }

   struct stat fileStat;

   if (stat(home, &fileStat) < 0) {
      return -1;
   }

   uint32_t uid = getuid();

   if ((fileStat.st_uid == uid) && S_ISDIR(fileStat.st_mode) &&
       ((fileStat.st_mode & 0777) == 0700)) {
      DIR* d = opendir(home);
      if (d) {
         while ((dir = readdir(d)) != NULL) {
            if ( dir->d_name[0] == '.') {
               /* We are ignoring all files starting with . like ".Template"
                * and "." ".." directories
                */
               continue;
            }

            meter_list[meters_count].name = xStrdup(dir->d_name);
            xAsprintf(&path, "%s/%s", home, dir->d_name);

            if (stat(path, &fileStat) < 0) {
               return -1;
            }

            if ((fileStat.st_uid == uid) && ((fileStat.st_mode & 0777) == 0700)) {
               ret = load_config(&meter_list[meters_count], path);

               if (ret && (meters_count < MAX_METERS_COUNT)) {
                  meters_count++;
               }
            }

            free(path);
            path=NULL;

         }
         closedir(d);
      }
   }

   free(home);
   home = NULL;

   return meters_count;
}

static void FlexMeter_updateValues(Meter* this)
{
   for (size_t i = 0 ; i < (size_t)meters_count; i++) {
      if (this->m_ptr == &FlexMeter_class[i] ) {
         char* buff = NULL;
         int ret = -1;
         if (meter_list[i].command) {
            FILE* fd = popen(meter_list[i].command, "r");
            if (fd) {
               buff = String_readLine(fd);
               ret = pclose(fd);
            }
         }

         if (buff && !ret) {
            xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", buff);
         } else {
            // Once fail, free command pointer and every time print Error message
            if (meter_list[i].command != NULL) {
               free(meter_list[i].command);
               meter_list[i].command = NULL;
            }
            xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", "[ERR] Check command");
         }
      }
   }
}

const MeterClass FlexMeter_class_template = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = FlexMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100,
   .attributes = DateMeter_attributes,
   .name = NULL,
   .uiName = NULL,
   .caption = NULL,
};

int load_flex_modules(void)
{
   size_t meters_num = check_for_meters();
   if (!FlexMeter_class && meters_num > 0) {
      FlexMeter_class = (MeterClass*) xCalloc(meters_num, sizeof(MeterClass));
      for (size_t i = 0 ; i < meters_num; i++) {
         memcpy(&FlexMeter_class[i], &FlexMeter_class_template, sizeof(MeterClass));
         FlexMeter_class[i].name = (const char*) xStrdup(meter_list[i].name);
         FlexMeter_class[i].uiName = (const char*) xStrdup(meter_list[i].uiName);
         FlexMeter_class[i].caption = (const char*) xStrdup(meter_list[i].caption);
      }
   }
   return meters_num;
}
