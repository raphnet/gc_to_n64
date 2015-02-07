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

#include "n64_mapping.h"

struct mapping_controller_unit g_n64_status[] = {
	[MAP_N64_BTN_A] 		= { .type = TYPE_BTN, }, // A
	[MAP_N64_BTN_B]			= { .type = TYPE_BTN, }, // B
	[MAP_N64_BTN_Z]			= { .type = TYPE_BTN, }, // Z
	[MAP_N64_BTN_START]		= { .type = TYPE_BTN, }, // Start
	[MAP_N64_BTN_L]			= { .type = TYPE_BTN, }, // L
	[MAP_N64_BTN_R]			= { .type = TYPE_BTN, }, // R
	[MAP_N64_BTN_C_UP]		= { .type = TYPE_BTN, }, // C-up
	[MAP_N64_BTN_C_DOWN]	= { .type = TYPE_BTN, }, // C-down
	[MAP_N64_BTN_C_LEFT]	= { .type = TYPE_BTN, }, // C-left
	[MAP_N64_BTN_C_RIGHT]	= { .type = TYPE_BTN, }, // C-right
	[MAP_N64_BTN_DPAD_UP]	= { .type = TYPE_BTN, }, // Dpad-up
	[MAP_N64_BTN_DPAD_DOWN]	= { .type = TYPE_BTN, }, // Dpad-down
	[MAP_N64_BTN_DPAD_LEFT]	= { .type = TYPE_BTN, }, // Dpad-left
	[MAP_N64_BTN_DPAD_RIGHT] = { .type = TYPE_BTN, }, // Dpad-right
	[MAP_N64_AXIS_LEFT_RIGHT] = { .type = TYPE_AXIS }, // Left-right axis
	[MAP_N64_AXIS_UP_DOWN]	= { .type = TYPE_AXIS }, // Up-down axis
	[MAP_N64_AXB_JOY_UP]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_FULL_NEG }, // Joystick up
	[MAP_N64_AXB_JOY_DOWN]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_FULL_POS }, // Joystick down
	[MAP_N64_AXB_JOY_LEFT]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_FULL_NEG }, // Joystick left
	[MAP_N64_AXB_JOY_RIGHT]	= { .type = TYPE_AXIS_TO_BTN, .thres = THRES_FULL_POS }, // Joystick right
	[MAP_N64_NONE] = { .type = TYPE_NONE },
};


