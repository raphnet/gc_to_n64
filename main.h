#ifndef _main_h__
#define _main_h__

#include "gamepad.h"
#include "gamecube.h"
#include "gamecube_mapping.h"

extern unsigned char gc_report[GC_REPORT_SIZE];
extern Gamepad *g_gcpad;
extern struct mapping_entry current_mapping[MAP_GC_NONE + 2];

int loadMappingId(int id);
void saveCurrentMappingTo(int id);
int calb(char orig, unsigned char val);

#endif // _main_h__
