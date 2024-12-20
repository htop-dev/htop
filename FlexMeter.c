/*
htop - FlexMeter.c
(C) 2021 Stoyan Bogdanov
(C) 2021 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "FlexMeter.h"
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#include "CRT.h"
#include "Object.h"
//#include "ProcessList.h"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#define FLEX_CFG_FOLDER ".config/htop/FlexMeter"

typedef struct {
   char name[80];
   char command[2048];
   char type[80];
   char caption[80];
   char uiName[80];
   uint8_t done;
}_flex_meter;


_flex_meter meter_list[30];

int meter_list_idx=0;
static int meters_count=0;

static const int DateMeter_attributes[] = {
   FLEX
};

MeterClass * FlexMeter_class = NULL;

int check_for_meters(void);

static int parse_input(char * line)
{
   int l=0;

   if (!strncmp(line,"name=",5))
   {
      strcpy(meter_list[meters_count].uiName,line+5);
      l = strlen(meter_list[meters_count].uiName);
      meter_list[meters_count].uiName[l-1] = '\0';
   }
   else if (!strncmp(line,"command=",7))
   {
      strcpy(meter_list[meters_count].command,line+8);
      l = strlen(meter_list[meters_count].command);
      meter_list[meters_count].command[l-1] = '\0';
   }
   else if (!strncmp(line,"caption=",7))
   {
      strcpy(meter_list[meters_count].caption,line+9);
      l = strlen(meter_list[meters_count].caption);
      meter_list[meters_count].caption[l-1] = '\0';
   }
   else if (!strncmp(line,"type=",5))
   {
      strcpy(meter_list[meters_count].type,line+6);
      l = strlen(meter_list[meters_count].type);
      meter_list[meters_count].type[l-1] = '\0';
   }
   else
   {
      return -1;
   }

   return 0;
}

static int load_config(char * file)
{
   int ret=0;
   char buff[2048];
   memset(buff,0,2048);
   FILE *fp = fopen( file , "r");
   while( NULL != fgets(buff, 2048, fp) )
   {
      parse_input(buff);
      memset(buff,0,2048);
   }

   fclose(fp);
   return ret;
}

int check_for_meters(void)
{
   char path[400];
   DIR *d;
   struct dirent *dir;
   struct passwd *pw = getpwuid(getuid());
   const char *homedir = pw->pw_dir;
   char home[120];
   sprintf(home,"%s/%s",homedir,FLEX_CFG_FOLDER);
   d = opendir(home);
   if (d) {
      while ((dir = readdir(d)) != NULL) {
         if ( dir->d_name[0]!='.' && strcmp(".",dir->d_name) && strcmp("..",dir->d_name))
         {
            strcpy(meter_list[meters_count].name,dir->d_name);
            memset(path,0,80);
            sprintf(path,"%s/%s",home,dir->d_name);
            if( 0 != load_config(path) )
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
      if (0 == mkdir(home,0777))
      {
         char template [4][40] =
         {
            "name=template",
            "command=echo \"`uptime`\"",
            "type=TEXT_METERMODE",
            "caption=\"UPTIME\""
         };

         char  template_filename[500];
         sprintf(template_filename,"%s/%s",home,".Template");
         FILE * fp = fopen(template_filename,"w");
         if (fp != NULL )
         {
            for (int i=0; i<4; i++)
            {
               fprintf(fp,"%s\n",template[i]);
            }
         }
      }
   }
   return meters_count;
}

static void FlexMeter_updateValues(Meter* this)
{
   for (int i =0 ; i < meters_count; i++)
   {
      if (this->m_ptr == &FlexMeter_class[i] )
      {
         char buff[256]={0};
         memset(buff,0,256);
         if ( meter_list[i].command[0] != 0 )
         {
            FILE* fd = popen(meter_list[i].command, "r");
            if (fd)
            {
               if ( fgets(buff, 256, fd) == NULL ) pclose(fd);
               else pclose(fd);
            }
            if( buff[0] != 0 )
            {
               int l = strlen(buff);
               buff[l-1] = '\0';
               xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s",buff);
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
   if (FlexMeter_class == NULL && meters_num!=0)
   {
      FlexMeter_class = (MeterClass*) xCalloc(meters_num,sizeof(MeterClass));
      for (uint8_t i=0 ; i<meters_num;i++)
      {
         memcpy(&FlexMeter_class[i],&FlexMeter_class_template,sizeof(MeterClass));

         char * p = (char * ) xCalloc(1,strlen(meter_list[i].name));
         memcpy(p,meter_list[i].name,strlen(meter_list[i].name));
         FlexMeter_class[i].name = (const char *) p;

         p = (char * ) xCalloc(1,strlen(meter_list[i].uiName));
         memcpy(p,meter_list[i].uiName,strlen(meter_list[i].uiName));
         FlexMeter_class[i].uiName = (const char *) p;

         p = (char * ) xCalloc(1,strlen(meter_list[i].caption));
         memcpy(p,meter_list[i].caption,strlen(meter_list[i].caption));
         FlexMeter_class[i].caption = (const char *) p;
      }
   }
   return (int)meters_num;
}
