/*
htop - FlexMeter.c
(C) 2024 Stoyan Bogdanov
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include "FlexMeter.h"
#include "Object.h"
#include "config.h"
#include "CRT.h"

#define FLEX_CFG_FOLDER ".config/htop/FlexMeter"

typedef struct {
   char* name;
   char* command;
   char* type;
   char* caption;
   char* uiName;
} _flex_meter;

_flex_meter meter_list[30];

int meter_list_idx = 0;
static int meters_count = 0;

static const int DateMeter_attributes[] = {
   FLEX
};

MeterClass* FlexMeter_class = NULL;

int check_for_meters(void);

static int parse_input(char* line)
{
   if (!strncmp(line, "name=", 5))
   {
      meter_list[meters_count].uiName = xStrdup(line + 5);
   }
   else if (!strncmp(line, "command=", 7))
   {
      meter_list[meters_count].command = xStrdup(line + 8);
   }
   else if (!strncmp(line, "caption=", 7))
   {
      meter_list[meters_count].caption = xStrdup(line + 8);
   }
   else if (!strncmp(line, "type=", 5))
   {
      meter_list[meters_count].type = xStrdup(line + 6);
   }
   else
   {
      return -1;
   }

   return 0;
}

static int load_config(char* file)
{
   int ret = -1;
   char* buff;
   FILE* fp = fopen( file, "r");

   if (fp != NULL) {
      while (1)
      {
         buff = String_readLine(fp);
         if (buff != NULL)
            parse_input(buff);
         else
            break;
      }

      fclose(fp);
      ret = 0;
   }

   return ret;
}

int check_for_meters(void)
{
   char path[400]; // full path
   DIR* d;
   struct dirent* dir;
   struct passwd* pw = getpwuid(getuid());
   const char* homedir = pw->pw_dir;

   // path to home folder 1 for zero 1 for slash
   char* home = (char* ) xCalloc(1, (strlen(homedir) + strlen(FLEX_CFG_FOLDER) + 2));

   xSnprintf(home, (strlen(homedir) + strlen(FLEX_CFG_FOLDER) + 2), "%s/%s", homedir, FLEX_CFG_FOLDER);

   d = opendir(home);
   if (d) {
      while ((dir = readdir(d)) != NULL) {
         if ( dir->d_name[0] != '.')
         {
            // We are ignoring all files starting with . like ".Template" and "." ".." directories
            meter_list[meters_count].name = xStrdup(dir->d_name);
            memset(path, 0, 80);
            xSnprintf(path, 400, "%s/%s", home, dir->d_name);
            if ( 0 != load_config(path) )
            {
               break;
            }

            if ( meters_count < MAX_METERS_COUNT )
            {
               meters_count++;
            }
            else
            {
               break; // go out we reach the limit
            }
         }
      }
      closedir(d);
   }
   else
   {
      mkdir(home, 0700);
   }

   free(home);
   return meters_count;
}

static void FlexMeter_updateValues(Meter* this)
{
   for (int i = 0 ; i < meters_count; i++)
   {
      if (this->m_ptr == &FlexMeter_class[i] )
      {
         char buff[256] = {0};
         int ret = -1;
         memset(buff, 0, 256);
         if ( meter_list[i].command[0] != 0 )
         {
            FILE* fd = popen(meter_list[i].command, "r");
            if (fd)
            {
               if (fgets(buff, 256, fd) == NULL)
                  ret = pclose(fd);
               else
                  ret = pclose(fd);
            }

            if ( (buff[0] != 0) && (ret == 0) )
            {
               int l = strlen(buff);
               buff[l - 1] = '\0';
               xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", buff);
            }
            else
            {
               xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", "Read CMD ERR");
            }
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
   uint8_t meters_num = check_for_meters();
   if (FlexMeter_class == NULL && meters_num != 0)
   {
      FlexMeter_class = (MeterClass*) xCalloc(meters_num, sizeof(MeterClass));
      for (uint8_t i = 0 ; i < meters_num; i++)
      {
         memcpy(&FlexMeter_class[i], &FlexMeter_class_template, sizeof(MeterClass));

         FlexMeter_class[i].name = (const char*) xStrdup(meter_list[i].name);
         FlexMeter_class[i].uiName = (const char*) xStrdup(meter_list[i].uiName);
         FlexMeter_class[i].caption = (const char*) xStrdup(meter_list[i].caption);
      }
   }
   return (int)meters_num;
}
