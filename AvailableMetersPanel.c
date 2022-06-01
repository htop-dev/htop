/*
htop - AvailableMetersPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "AvailableMetersPanel.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "CPUMeter.h"
#include "DynamicMeter.h"
#include "FunctionBar.h"
#include "Hashtable.h"
#include "Header.h"
#include "ListItem.h"
#include "Macros.h"
#include "Meter.h"
#include "MetersPanel.h"
#include "Object.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "XUtils.h"


static void AvailableMetersPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableMetersPanel* this = (AvailableMetersPanel*) object;
   Panel_done(super);
   free(this->meterPanels);
   free(this);
}

static inline void AvailableMetersPanel_addMeter(Header* header, MetersPanel* panel, const MeterClass* type, unsigned int param, size_t column) {
   const Meter* meter = Header_addMeterByClass(header, type, param, column);
   Panel_add((Panel*)panel, (Object*) Meter_toListItem(meter, false));
   Panel_setSelected((Panel*)panel, Panel_size((Panel*)panel) - 1);
   MetersPanel_setMoving(panel, true);
}

static HandlerResult AvailableMetersPanel_eventHandler(Panel* super, int ch) {
   AvailableMetersPanel* this = (AvailableMetersPanel*) super;
   Header* header = this->header;

   const ListItem* selected = (ListItem*) Panel_getSelected(super);
   if (!selected)
      return IGNORED;

   unsigned int param = selected->key & 0xffff;
   int type = selected->key >> 16;
   HandlerResult result = IGNORED;
   bool update = false;

   switch(ch) {
      case KEY_F(5):
      case 'l':
      case 'L':
      {
         AvailableMetersPanel_addMeter(header, this->meterPanels[0], Platform_meterTypes[type], param, 0);
         result = HANDLED;
         update = true;
         break;
      }
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_F(6):
      case 'r':
      case 'R':
      {
         AvailableMetersPanel_addMeter(header, this->meterPanels[this->columns - 1], Platform_meterTypes[type], param, this->columns - 1);
         result = (KEY_LEFT << 16) | SYNTH_KEY;
         update = true;
         break;
      }
   }
   if (update) {
      this->settings->changed = true;
      this->settings->lastUpdate++;
      Header_calculateHeight(header);
      Header_updateData(header);
      Header_draw(header);
      ScreenManager_resize(this->scr);
   }
   return result;
}

const PanelClass AvailableMetersPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = AvailableMetersPanel_delete
   },
   .eventHandler = AvailableMetersPanel_eventHandler
};

// Handle (&CPUMeter_class) entries in the AvailableMetersPanel
static void AvailableMetersPanel_addCPUMeters(Panel* super, const MeterClass* type, const ProcessList* pl) {
   if (pl->existingCPUs > 1) {
      Panel_add(super, (Object*) ListItem_new("CPU average", 0));
      for (unsigned int i = 1; i <= pl->existingCPUs; i++) {
         char buffer[50];
         xSnprintf(buffer, sizeof(buffer), "%s %d", type->uiName, Settings_cpuId(pl->settings, i - 1));
         Panel_add(super, (Object*) ListItem_new(buffer, i));
      }
   } else {
      Panel_add(super, (Object*) ListItem_new(type->uiName, 1));
   }
}

typedef struct {
   Panel* super;
   unsigned int id;
   unsigned int offset;
} DynamicIterator;

static void AvailableMetersPanel_addDynamicMeter(ATTR_UNUSED ht_key_t key, void* value, void* data) {
   const DynamicMeter* meter = (const DynamicMeter*)value;
   DynamicIterator* iter = (DynamicIterator*)data;
   unsigned int identifier = (iter->offset << 16) | iter->id;
   const char* label = meter->description ? meter->description : meter->caption;
   if (!label)
      label = meter->name; /* last fallback to name, guaranteed set */
   Panel_add(iter->super, (Object*) ListItem_new(label, identifier));
   iter->id++;
}

// Handle (&DynamicMeter_class) entries in the AvailableMetersPanel
static void AvailableMetersPanel_addDynamicMeters(Panel* super, const ProcessList* pl, unsigned int offset) {
   DynamicIterator iter = { .super = super, .id = 1, .offset = offset };
   assert(pl->dynamicMeters != NULL);
   Hashtable_foreach(pl->dynamicMeters, AvailableMetersPanel_addDynamicMeter, &iter);
}

// Handle remaining Platform Meter entries in the AvailableMetersPanel
static void AvailableMetersPanel_addPlatformMeter(Panel* super, const MeterClass* type, unsigned int offset) {
   const char* label = type->description ? type->description : type->uiName;
   Panel_add(super, (Object*) ListItem_new(label, offset << 16));
}

AvailableMetersPanel* AvailableMetersPanel_new(Settings* settings, Header* header, size_t columns, MetersPanel** meterPanels, ScreenManager* scr, const ProcessList* pl) {
   AvailableMetersPanel* this = AllocThis(AvailableMetersPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_newEnterEsc("Add   ", "Done   ");
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->header = header;
   this->columns = columns;
   this->meterPanels = meterPanels;
   this->scr = scr;

   Panel_setHeader(super, "Available meters");
   // Platform_meterTypes[0] should be always (&CPUMeter_class) which we will
   // handle separately in the code below.  Likewise, identifiers for Dynamic
   // Meters are handled separately - similar to CPUs, this allows generation
   // of multiple different Meters (also using 'param' to distinguish them).
   for (unsigned int i = 1; Platform_meterTypes[i]; i++) {
      const MeterClass* type = Platform_meterTypes[i];
      assert(type != &CPUMeter_class);
      if (type == &DynamicMeter_class)
         AvailableMetersPanel_addDynamicMeters(super, pl, i);
      else
         AvailableMetersPanel_addPlatformMeter(super, type, i);
   }
   AvailableMetersPanel_addCPUMeters(super, &CPUMeter_class, pl);

   return this;
}
