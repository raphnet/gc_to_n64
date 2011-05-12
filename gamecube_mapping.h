#ifndef _gamecube_mapping_h__
#define _gamecube_mapping_h__

#include "mapper.h"

extern struct mapping_controller_unit g_gamecube_status[];

#define MAP_GC_BTN_A			0
#define MAP_GC_BTN_B			1
#define MAP_GC_BTN_Z			2
#define MAP_GC_BTN_START		3
#define MAP_GC_BTN_L			4
#define MAP_GC_BTN_R			5

#define MAP_GC_AXB_C_UP			6
#define MAP_GC_AXB_C_DOWN		7
#define MAP_GC_AXB_C_LEFT		8
#define MAP_GC_AXB_C_RIGHT		9

#define MAP_GC_BTN_DPAD_UP		10
#define MAP_GC_BTN_DPAD_DOWN	11
#define MAP_GC_BTN_DPAD_LEFT	12
#define MAP_GC_BTN_DPAD_RIGHT	13

#define MAP_GC_AXIS_LEFT_RIGHT	14
#define MAP_GC_AXIS_UP_DOWN		15
#define MAP_GC_BTN_X			16
#define MAP_GC_BTN_Y			17

#define MAP_GC_AXB_JOY_UP		18
#define MAP_GC_AXB_JOY_DOWN		19
#define MAP_GC_AXB_JOY_LEFT		20
#define MAP_GC_AXB_JOY_RIGHT	21

#define MAP_GC_AXB_L_SLIDER		22
#define MAP_GC_AXB_R_SLIDER		23

#define MAP_GC_AXIS_C_LEFT_RIGHT	24
#define MAP_GC_AXIS_C_UP_DOWN		25

#define MAP_GC_NONE				26

#endif // _gamecube_mapping_h__

