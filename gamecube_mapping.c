/*  GC to N64 : Gamecube controller to N64 adapter firmware
    Copyright (C) 2011-2017  Raphael Assenat <raph@raphnet.net>

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
#include <stdlib.h>
#include "gamecube_mapping.h"
#include "eeprom.h"
#include "main.h"

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

static char gc_x_origin = 0;
static char gc_y_origin = 0;

void setOriginsFromReport(const unsigned char gcr[GC_REPORT_SIZE])
{
	// Signed origin
	gc_x_origin = gcr[0]^0x80;
	gc_y_origin = gcr[1]^0x80;
}
/*
 * \return N64 axis data (unsigned)
 * */
int calb(char orig, unsigned char val)
{
	short tmp;
	long mult = 26000; // V1.6
	char dz=0;

	if (!g_eeprom_data.wide_conversion) {
		if (g_eeprom_data.old_v1_5_conversion) {
			mult = 25000;
		}
	}

	if (g_eeprom_data.deadzone_enabled) {
		dz = 12;
		mult = 30000; // V1.6
		if (g_eeprom_data.old_v1_5_conversion) {
			mult = 29000;
		}
	}

	tmp = (signed char)(val^0x80) - orig;

	if (dz) {
		if (tmp > 0) {
			if (tmp < dz) {
				tmp = 0;
			} else {
				tmp -= dz;
			}
		}
		else if (tmp < 0) {
			if (tmp > -dz) {
				tmp = 0;
			} else {
				tmp += dz;
			}
		}
	}

	if (!g_eeprom_data.wide_conversion) {
	//tmp = tmp * 31000L / 32000L;
	tmp = tmp * mult / 32000L;
	}

	if (tmp<=-127)
		tmp = -127;

	if (tmp>127)
		tmp = 127;
/*
	if (tmp<0) {
		tmp = -((char)(pgm_read_byte(&correction_lut[tmp*-2])/2));
	} else {
		tmp = (char)(pgm_read_byte(&correction_lut[tmp*2])/2);
	}
*/
	//
	//   Real N64 x axis
	// -68 0 63
	// -79 0 78
	// -68 0 66
	//

	return tmp; // ((unsigned char)tmp ^ 0x80);
}

void gamecubeXYtoN64(unsigned char x, unsigned char y, char *dst_x, char *dst_y)
{
	unsigned char abs_y, abs_x;
	long sig_x, sig_y;
	long sx, sy;
	int n64_maxval = 80; // Version 1.6

	// The lower, the stronger the correction. 32768 means null correction
	//long l = 1700;
	//long l = 16000;
	long l = 256; // Version 1.6

	if (g_eeprom_data.wide_conversion) {
		sig_x = calb(gc_x_origin, x);
		sig_y = calb(gc_y_origin, y);

		*dst_x = sig_x;
		*dst_y = sig_y;
		return;
	}

	if (g_eeprom_data.old_v1_5_conversion) {
		// Provide a way to use the old parameters, in case
		// it turns out the new values are not good.
		l = 512;
		n64_maxval = 127;
	}

	sig_x = calb(gc_x_origin, x);
	sig_y = calb(gc_y_origin, y);

	abs_y = abs(sig_y);
	abs_x = abs(sig_x);

	if (1) {
		sx = sig_x + sig_x * abs_y / l;
		sy = sig_y + sig_y * abs_x / l;
	} else {
		// Direct conversion (for testing. Not good for use, corners do not
		// reach maximum values)
		sx = sig_x;
		sy = sig_y;
	}

	if (sx<=-n64_maxval)
		sx = -n64_maxval;
	if (sx>n64_maxval)
		sx = n64_maxval;
	if (sy<=-n64_maxval)
		sy = -n64_maxval;
	if (sy>n64_maxval)
		sy = n64_maxval;

	*dst_x = sx;
	*dst_y = sy;
}
/*
static unsigned char gamecubeXtoN64(unsigned char raw)
{
	return calb(gc_x_origin, raw);
	//return ((char)raw) * 24000L / 32767L;
}

static unsigned char gamecubeYtoN64(unsigned char raw)
{
	return calb(gc_y_origin, raw);
	//return ((char)raw) * 24000L / 32767L;
}
*/

void gc_report_to_mapping(const unsigned char gcr[GC_REPORT_SIZE], struct mapping_controller_unit *gcs)
{
	gcs[MAP_GC_BTN_A].value = gcr[6] & 0x10;
	gcs[MAP_GC_BTN_B].value = gcr[6] & 0x08;
	gcs[MAP_GC_BTN_Z].value = gcr[6] & 0x80;
	gcs[MAP_GC_BTN_START].value = gcr[6] & 0x01;
	gcs[MAP_GC_BTN_L].value = gcr[6] & 0x20;
	gcs[MAP_GC_BTN_R].value = gcr[6] & 0x40;

	gcs[MAP_GC_AXB_C_UP].value = gcr[3] ^ 0x80;
	gcs[MAP_GC_AXB_C_DOWN].value = gcr[3] ^ 0x80;
	gcs[MAP_GC_AXB_C_LEFT].value = gcr[2] ^ 0x80;
	gcs[MAP_GC_AXB_C_RIGHT].value = gcr[2] ^ 0x80;

	gcs[MAP_GC_BTN_DPAD_UP].value = gcr[7] & 0x01;
	gcs[MAP_GC_BTN_DPAD_DOWN].value = gcr[7] & 0x02;
	gcs[MAP_GC_BTN_DPAD_LEFT].value = gcr[7] & 0x08;
	gcs[MAP_GC_BTN_DPAD_RIGHT].value = gcr[7] & 0x04;

	gamecubeXYtoN64(gcr[0], gcr[1], &gcs[MAP_GC_AXIS_LEFT_RIGHT].value, &gcs[MAP_GC_AXIS_UP_DOWN].value);
//	gcs[MAP_GC_AXIS_LEFT_RIGHT].value = gamecubeXtoN64(gcr[0]);
//	gcs[MAP_GC_AXIS_UP_DOWN].value = gamecubeYtoN64(gcr[1]);

	gcs[MAP_GC_BTN_X].value = gcr[6] & 0x04;
	gcs[MAP_GC_BTN_Y].value = gcr[6] & 0x02;

	gcs[MAP_GC_AXB_JOY_UP].value = gcs[MAP_GC_AXIS_UP_DOWN].value;
	gcs[MAP_GC_AXB_JOY_DOWN].value = gcs[MAP_GC_AXIS_UP_DOWN].value;
	gcs[MAP_GC_AXB_JOY_LEFT].value = gcs[MAP_GC_AXIS_LEFT_RIGHT].value;
	gcs[MAP_GC_AXB_JOY_RIGHT].value = gcs[MAP_GC_AXIS_LEFT_RIGHT].value;

	gcs[MAP_GC_AXB_L_SLIDER].value = gcr[4] ^ 0x80;
	gcs[MAP_GC_AXB_R_SLIDER].value = gcr[5] ^ 0x80;

	gcs[MAP_GC_AXIS_C_LEFT_RIGHT].value = gcs[MAP_GC_AXB_C_LEFT].value;
	gcs[MAP_GC_AXIS_C_UP_DOWN].value = gcs[MAP_GC_AXB_C_UP].value;
}
