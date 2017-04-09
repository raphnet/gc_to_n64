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
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "sync.h"
#include "eeprom.h"
#include "menu.h"
#include "buzzer.h"
#include "gamecube.h"
#include "mapper.h"
#include "gamecube_mapping.h"
#include "main.h"

#define EV_BTN_A		0x001
#define EV_BTN_B		0x002
#define EV_BTN_X		0x004
#define EV_BTN_Y		0x008
#define EV_BTN_Z		0x010
#define EV_BTN_L		0x020
#define EV_BTN_R		0x040
#define EV_BTN_D_UP		0x080
#define EV_BTN_D_DOWN	0x100
#define EV_BTN_D_LEFT	0x200
#define EV_BTN_D_RIGHT	0x400
#define EV_BTN_START	0x800

#define CODE_BASE	4

static void waitStartRelease(void)
{
	while(1)
	{
		g_gcpad->update(GAMECUBE_UPDATE_NORMAL);
		g_gcpad->buildReport(gc_report);
		gc_report_to_mapping(gc_report, g_gamecube_status);

		if (!g_gamecube_status[MAP_GC_BTN_START].value)
			break;

		_delay_ms(16);
	}
}

static int getEvent(void)
{
	static int status = 0;
	int b;
	while(1)
	{
		_delay_ms(16);
		g_gcpad->update(GAMECUBE_UPDATE_NORMAL);
		if (g_gcpad->changed()) {
			int now = 0;
			// Read the gamepad
			g_gcpad->buildReport(gc_report);

			// Convert the data we got from the gamepad reader
			// to a mapping structure.
			gc_report_to_mapping(gc_report, g_gamecube_status);

			if (g_gamecube_status[MAP_GC_BTN_A].value)
				now |= EV_BTN_A;
			if (g_gamecube_status[MAP_GC_BTN_B].value)
				now |= EV_BTN_B;
			if (g_gamecube_status[MAP_GC_BTN_X].value)
				now |= EV_BTN_X;
			if (g_gamecube_status[MAP_GC_BTN_Y].value)
				now |= EV_BTN_Y;
			if (g_gamecube_status[MAP_GC_BTN_Z].value)
				now |= EV_BTN_Z;
			if (g_gamecube_status[MAP_GC_BTN_L].value)
				now |= EV_BTN_L;
			if (g_gamecube_status[MAP_GC_BTN_R].value)
				now |= EV_BTN_R;
			if (g_gamecube_status[MAP_GC_BTN_DPAD_UP].value)
				now |= EV_BTN_D_UP;
			if (g_gamecube_status[MAP_GC_BTN_DPAD_DOWN].value)
				now |= EV_BTN_D_DOWN;
			if (g_gamecube_status[MAP_GC_BTN_DPAD_LEFT].value)
				now |= EV_BTN_D_LEFT;
			if (g_gamecube_status[MAP_GC_BTN_DPAD_RIGHT].value)
				now |= EV_BTN_D_RIGHT;
			if (g_gamecube_status[MAP_GC_BTN_START].value)
				now |= EV_BTN_START;

			for (b=1; b<=EV_BTN_START; b<<=1) {
				if ((now & b) != (status & b)) {
					if (now & b) {
						status = now;
						return b;
					}
				}
			}
			status = now;
		}
	}
}

int receivePair(int initial_event, int *input, int *output)
{
	int ev = initial_event;
	int *target = input;
	unsigned char value = 0x80;

	// Code [ABXY] Z [ABXY] L
	//      input    output
	//
	// Z : Input/output separator
	// L : Pair end.
	//

	if (initial_event==EV_BTN_L) {
		if (*input != -1) {
			// assign next trigger to same output
			(*input)++;
			return 0;
		}
		return -1;
	}

	goto first_ev;

	while(1)
	{
		ev = getEvent();
first_ev:

		switch (ev)
		{
			case EV_BTN_Z:
				if (value != 0x80) {
					*target = value;
				} else {
					if (*input != -1) {
						(*input)++;
					} else {
						return -1; // Prior explicit input id required
					}
				}
				target = output;
				value = 0x80;
				break;

			case EV_BTN_A:
				value *= CODE_BASE;
				break;
			case EV_BTN_B:
				value *= CODE_BASE;
				value += 1;
				break;
			case EV_BTN_X:
				value *= CODE_BASE;
				value += 2;
				break;
			case EV_BTN_Y:
				value *= CODE_BASE;
				value += 3;
				break;

			case EV_BTN_L:
				if (target == input) {
					// should have got at least a 'Z'
					return -1;
				}
				if (value != 0x80) {
					*target = value;
					return 0;
				}
				// target omitted, left as-is
				return 0;

			case EV_BTN_START:
				return -1; // abort
		}

	}

	return 0;
}

int set_default_menu()
{
	int ev;

	blips(1);
	ev = getEvent();

	switch(ev)
	{
		case EV_BTN_D_UP:
			setDefaultMapping(1);
			return 0;

		case EV_BTN_D_DOWN:
			setDefaultMapping(2);
			return 0;

		case EV_BTN_D_LEFT:
			setDefaultMapping(3);
			return 0;

		case EV_BTN_D_RIGHT:
			setDefaultMapping(4);
			return 0;

		case EV_BTN_START:
			setDefaultMapping(0);
			return 0;

		default:
			return -1;
	}

	return 0;

}

// R pressed. Next steps:
//
// Dpad direction : Save to corresponding slots and exit.
// Start : Load default mapping and exit.
int rmenu_do()
{
	int ev;

	blips(1);
	ev = getEvent();

	switch(ev)
	{
		case EV_BTN_D_UP:
			saveCurrentMappingTo(1);
			break;

		case EV_BTN_D_DOWN:
			saveCurrentMappingTo(2);
			break;

		case EV_BTN_D_LEFT:
			saveCurrentMappingTo(3);
			// load stored config id X
			break;

		case EV_BTN_D_RIGHT:
			saveCurrentMappingTo(4);
			break;

		case EV_BTN_START:
			loadMappingId(0);
			break;

		case EV_BTN_Z:
			toggleDeadzone();
			break;

		case EV_BTN_L:
			return set_default_menu();

		case EV_BTN_B:
			cycle_conversion_mode();
			break;

		case EV_BTN_X:
			eeprom_writeDefaults();
			loadMappingId(0);
			break;

		default:
			return -1;
	}

	return 0;
}

void menumain()
{
	int ev, res;
	unsigned char sreg;
	int input=-1, output=-1;
	int firstMappingPairInSession = 1;
	sreg = SREG;
	cli();

	// re-init buzzer (sync uses the same timer)
	buzzer_init();
#ifndef VISUAL_BUZZER
	blips(5);
#else
	buzzer_led_invert(1);
	buzz(0);
#endif

	waitStartRelease();

	while(1)
	{

		ev = getEvent();

		switch(ev)
		{
			case EV_BTN_D_UP:
				res = loadMappingId(1);
				if (res==-1)
					goto error;

				goto menu_done;

			case EV_BTN_D_DOWN:
				res = loadMappingId(2);
				if (res==-1)
					goto error;

				goto menu_done;

			case EV_BTN_D_LEFT:
				res = loadMappingId(3);
				if (res==-1)
					goto error;

				goto menu_done;

			case EV_BTN_D_RIGHT:
				res = loadMappingId(4);
				if (res==-1)
					goto error;

				goto menu_done;

			case EV_BTN_START:
				goto menu_done;

			case EV_BTN_R:
				res = rmenu_do();
				if (res==-1)
					goto error;

				goto menu_done;

			case EV_BTN_Z:
			case EV_BTN_A:
			case EV_BTN_B:
			case EV_BTN_X:
			case EV_BTN_Y:
			case EV_BTN_L:
				res = receivePair(ev, &input, &output);
				if (res==-1) {
					goto error;
				}

				if (input == -1 || output == -1) {
					goto error;
				}

				if (firstMappingPairInSession) {
					// When a new code is entered, we must start
					// from a clean copy of the default mapping. Otherwise
					// the changes are cumulative.
					loadMappingId(0);
					firstMappingPairInSession = 0;
				}

				mapper_change_mapping_entry(current_mapping, input, output);
				blips(1);
				break;
		}
	}

error:
	buzz_error();

	// re-init sync. Buzzer uses the same timer...
	sync_init();

	SREG = sreg;
	return;

menu_done:
	buzzer_led_invert(0);
	blips(3);

	// re-init sync. Buzzer uses the same timer...
	sync_init();


	SREG = sreg;
}
