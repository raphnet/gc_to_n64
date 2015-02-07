/*  GC to N64 : Gamecube controller to N64 adapter firmware
    Copyright (C) 2011-2015  Raphael Assenat <raph@raphnet.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gamecube_mapping.h"

// input
struct mapping_controller_unit g_gamecube_status[] = {
	[MAP_GC_BTN_A]			= { .type = TYPE_BTN, },
	[MAP_GC_BTN_B]			= { .type = TYPE_BTN, },
	[MAP_GC_BTN_Z]			= { .type = TYPE_BTN, },
	[MAP_GC_BTN_START]		= { .type = TYPE_BTN, },
	[MAP_GC_BTN_L]			= { .type = TYPE_BTN, },
	[MAP_GC_BTN_R]			= { .type = TYPE_BTN, },
	[MAP_GC_AXB_C_UP]		= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_NEG, },
	[MAP_GC_AXB_C_DOWN]		= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_POS, },
	[MAP_GC_AXB_C_LEFT]		= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_NEG, },
	[MAP_GC_AXB_C_RIGHT]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_POS, },
	[MAP_GC_BTN_DPAD_UP]	= { .type = TYPE_BTN, },
	[MAP_GC_BTN_DPAD_DOWN]	= { .type = TYPE_BTN, },
	[MAP_GC_BTN_DPAD_LEFT]	= { .type = TYPE_BTN, },
	[MAP_GC_BTN_DPAD_RIGHT] = { .type = TYPE_BTN, },
	[MAP_GC_AXIS_LEFT_RIGHT]= { .type = TYPE_AXIS },
	[MAP_GC_AXIS_UP_DOWN]	= { .type = TYPE_AXIS },
	[MAP_GC_BTN_X]			= { .type = TYPE_BTN, },
	[MAP_GC_BTN_Y]			= { .type = TYPE_BTN, },
	[MAP_GC_AXB_JOY_UP]		= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_NEG, },
	[MAP_GC_AXB_JOY_DOWN]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_POS, },
	[MAP_GC_AXB_JOY_LEFT]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_NEG, },
	[MAP_GC_AXB_JOY_RIGHT]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_25_POS, },
	[MAP_GC_AXB_L_SLIDER]	= { .type = TYPE_NEG_SLD_TO_BTN, .thres = THRES_25_POS, },
	[MAP_GC_AXB_R_SLIDER]	= { .type = TYPE_NEG_SLD_TO_BTN, .thres = THRES_25_POS, },
	[MAP_GC_AXIS_C_LEFT_RIGHT] = { .type = TYPE_AXIS },
	[MAP_GC_AXIS_C_UP_DOWN]	= { .type = TYPE_AXIS },
	[MAP_GC_NONE] = { .type = TYPE_NONE },
};


