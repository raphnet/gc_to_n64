/*  GC to N64 : Gamecube controller to N64 adapter firmware
    Copyright (C) 2011  Raphael Assenat <raph@raphnet.net>

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
#include <stdio.h>
#include <string.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "buzzer.h"
#include "timer0.h"

#include "gamecube.h"

//#include "gc_offsets.h"
//#include "n64_offsets.h"

#include "n64_isr.h"

#include "mapper.h"
#include "gamecube_mapping.h"
#include "n64_mapping.h"

#include "eeprom.h"

Gamepad *gcpad;
unsigned char gc_report[GC_REPORT_SIZE];

struct mapping_entry current_mapping[MAP_GC_NONE + 2];

struct mapping_entry gamecube_to_n64_default_mapping[MAP_GC_NONE + 2] = {
	{ MAP_GC_BTN_A,				MAP_N64_BTN_A },
	{ MAP_GC_BTN_B,				MAP_N64_BTN_B },
	{ MAP_GC_BTN_Z,				MAP_N64_BTN_Z },
	{ MAP_GC_BTN_START,			MAP_N64_BTN_START },
	{ MAP_GC_BTN_L,				MAP_N64_BTN_L },
	{ MAP_GC_BTN_R,				MAP_N64_BTN_R },
	{ MAP_GC_AXB_C_UP,			MAP_N64_BTN_C_UP },
	{ MAP_GC_AXB_C_DOWN,		MAP_N64_BTN_C_DOWN },
	{ MAP_GC_AXB_C_LEFT,		MAP_N64_BTN_C_LEFT },
	{ MAP_GC_AXB_C_RIGHT,		MAP_N64_BTN_C_RIGHT },
	{ MAP_GC_BTN_DPAD_UP,		MAP_N64_BTN_DPAD_UP },
	{ MAP_GC_BTN_DPAD_DOWN,		MAP_N64_BTN_DPAD_DOWN },
	{ MAP_GC_BTN_DPAD_LEFT,		MAP_N64_BTN_DPAD_LEFT },
	{ MAP_GC_BTN_DPAD_RIGHT,	MAP_N64_BTN_DPAD_RIGHT },
	{ MAP_GC_AXIS_LEFT_RIGHT,	MAP_N64_AXIS_LEFT_RIGHT },
	{ MAP_GC_AXIS_UP_DOWN,		MAP_N64_AXIS_UP_DOWN },
	{ MAP_GC_BTN_X,				MAP_N64_NONE },
	{ MAP_GC_BTN_Y,				MAP_N64_NONE },
	{ MAP_GC_AXB_JOY_UP,		MAP_N64_NONE },
	{ MAP_GC_AXB_JOY_DOWN,		MAP_N64_NONE },
	{ MAP_GC_AXB_JOY_LEFT,		MAP_N64_NONE },
	{ MAP_GC_AXB_JOY_RIGHT,		MAP_N64_NONE },
	{ MAP_GC_AXB_L_SLIDER,		MAP_N64_BTN_L },
	{ MAP_GC_AXB_R_SLIDER,		MAP_N64_BTN_R },
	{ MAP_GC_AXIS_C_LEFT_RIGHT,	MAP_N64_NONE },
	{ MAP_GC_AXIS_C_UP_DOWN,	MAP_N64_NONE },
	{ MAP_GC_NONE,				MAP_N64_NONE },
	{ -1, -1 }, // terminator
};

void saveCurrentMappingTo(int id)
{
	if (id >= 1 && id <= 4) {
		memcpy( g_eeprom_data.appdata + (sizeof(current_mapping) * (id-1)), current_mapping, sizeof(current_mapping));
		eeprom_commit();
	}
}

int loadMappingId(int id)
{
	unsigned char *mapping_pointer;

	// Always start from default mapping
	memcpy(current_mapping, gamecube_to_n64_default_mapping, sizeof(current_mapping));

	switch(id)
	{
		default:
		case 0:
			// default mapping used.
			break;
		
		case 1:
		case 2:
		case 3:
		case 4:
			mapping_pointer = g_eeprom_data.appdata + (sizeof(current_mapping) * (id-1));
			if (mapping_pointer[0] == 0xff) {
				// Terminator at the beginning? Empty slot!
				return -1;
			}
			memcpy(current_mapping, mapping_pointer, sizeof(current_mapping));
	}
	return 0;
}


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

static void gc_report_to_mapping(const unsigned char gcr[GC_REPORT_SIZE], struct mapping_controller_unit *gcs);

static void waitStartRelease(void)
{
	while(1)
	{

		gcpad->update();
		gcpad->buildReport(gc_report);
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
		gcpad->update();
		if (gcpad->changed()) {
			int now = 0;
			// Read the gamepad	
			gcpad->buildReport(gc_report);				
				
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
	sreg = SREG;
	cli();

	blips(5);

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
				mapper_change_mapping_entry(current_mapping, input, output);
				blips(1);

				

				break;
		}
	}

error:
	blips(3);

	buzz(1);
	_delay_ms(700);
	buzz(0);

	SREG = sreg;
	return;

menu_done:
	blips(3);

	SREG = sreg;
}

int domenu(struct mapping_controller_unit *gcs)
{
	static int start_seconds = 0;

	if (gcs[MAP_GC_BTN_START].value) 
	{
		if (timerIsOver()) {
			start_seconds++;
			if (start_seconds > 5) {
				menumain();
				return 1;
			}
		} 
	}
	else {
		start_seconds = 0;
	}
	return 0;
}
void byteTo8Bytes(unsigned char val, unsigned char *dst)
{
	unsigned char c = 0x80;

	do {
		*dst = val & c;
		dst++;
		c >>= 1;
	} while(c);
}

unsigned char scaleValue(unsigned char raw)
{
	return ((char)raw) * 24000L / 32767L;
}



static void gc_report_to_mapping(const unsigned char gcr[GC_REPORT_SIZE], struct mapping_controller_unit *gcs)
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

	gcs[MAP_GC_AXIS_LEFT_RIGHT].value = scaleValue(gcr[0]^0x80);
	gcs[MAP_GC_AXIS_UP_DOWN].value = scaleValue(gcr[1]^0x80);
	
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

#define IS_ACTIVE(a) ((a).value != (a).def)

void n64_status_to_output(struct mapping_controller_unit *n64s, unsigned char volatile dstbuf[32])
{
	char x,y;

	dstbuf[0] = IS_ACTIVE(n64s[MAP_N64_BTN_A]);
	dstbuf[1] = IS_ACTIVE(n64s[MAP_N64_BTN_B]);
	dstbuf[2] = IS_ACTIVE(n64s[MAP_N64_BTN_Z]);
	dstbuf[3] = IS_ACTIVE(n64s[MAP_N64_BTN_START]);
	dstbuf[4] = IS_ACTIVE(n64s[MAP_N64_BTN_DPAD_UP]);
	dstbuf[5] = IS_ACTIVE(n64s[MAP_N64_BTN_DPAD_DOWN]);
	dstbuf[6] = IS_ACTIVE(n64s[MAP_N64_BTN_DPAD_LEFT]);
	dstbuf[7] = IS_ACTIVE(n64s[MAP_N64_BTN_DPAD_RIGHT]);
	dstbuf[8] = 0;
	dstbuf[9] = 0;
	dstbuf[10] = IS_ACTIVE(n64s[MAP_N64_BTN_L]);
	dstbuf[11] = IS_ACTIVE(n64s[MAP_N64_BTN_R]);
	dstbuf[12] = IS_ACTIVE(n64s[MAP_N64_BTN_C_UP]);
	dstbuf[13] = IS_ACTIVE(n64s[MAP_N64_BTN_C_DOWN]);
	dstbuf[14] = IS_ACTIVE(n64s[MAP_N64_BTN_C_LEFT]);
	dstbuf[15] = IS_ACTIVE(n64s[MAP_N64_BTN_C_RIGHT]);

	x = n64s[MAP_N64_AXIS_LEFT_RIGHT].value;
	y = n64s[MAP_N64_AXIS_UP_DOWN].value;

	if (IS_ACTIVE(n64s[MAP_N64_AXB_JOY_UP]))
		y = n64s[MAP_N64_AXB_JOY_UP].value;
	if (IS_ACTIVE(n64s[MAP_N64_AXB_JOY_DOWN]))
		y = n64s[MAP_N64_AXB_JOY_DOWN].value;
	if (IS_ACTIVE(n64s[MAP_N64_AXB_JOY_LEFT]))
		x = n64s[MAP_N64_AXB_JOY_LEFT].value;
	if (IS_ACTIVE(n64s[MAP_N64_AXB_JOY_RIGHT]))
		x = n64s[MAP_N64_AXB_JOY_RIGHT].value;

	byteTo8Bytes(x, dstbuf + 16);
	byteTo8Bytes((y)^0xff, dstbuf + 24);

}


int main(void)
{
	
	gcpad = gamecubeGetGamepad();


	// PORTD2 is the N64 data signal.
	// Pullup resistors on all other signals.
	//
	DDRD=0;
	PORTD |= ~0x04;

	// portD 0 is debug bit
	DDRD |= 1;

	// temporary
	DDRC=0;
	PORTC=0xff;

	// configure external interrupt 1 to trigger on falling edge.
	MCUCR |= 1<<ISC11;
	MCUCR &= ~(1<<ISC10);

	GICR |= (1<<INT0);

	memset((void*)n64_tx_buf0, 0, sizeof(n64_tx_buf0));
	memset((void*)n64_tx_buf1, 0, sizeof(n64_tx_buf1));

	n64_tx_id_reply[0] = 0x05;
	n64_tx_id_reply[1] = 0x00;
	n64_tx_id_reply[2] = 0x02;

	// Got command 0xff with mario 64, controller replies similarly	
	n64_tx_id2_reply[0] = 0x05;
	n64_tx_id2_reply[1] = 0x00; 
	n64_tx_id2_reply[2] = 0x00;
	
	sei();

	eeprom_init();

	loadMappingId(g_eeprom_data.defmap);	

	
	buzzer_init();
	timerStart();

	gcpad->init();

	_delay_ms(500);

	/* Read from Gamecube controller */
	gcpad->update();
	gcpad->buildReport(gc_report);
	gc_report_to_mapping(gc_report, g_gamecube_status);

	if (g_gamecube_status[MAP_GC_BTN_START].value) {
		menumain(g_gamecube_status);
	}


	while(1)
	{
		if (n64_got_command) {
			n64_got_command = 0;
			PORTD ^= 1;
	
			timerIntOff();		
			gcpad->update();
			timerIntOn();
			if (gcpad->changed()) {
				// Read the gamepad	
				gcpad->buildReport(gc_report);				
				
				// Convert the data we got from the gamepad reader
				// to a mapping structure.
				gc_report_to_mapping(gc_report, g_gamecube_status);

				// Maybe enter configuration menu mode. If we did,
				// 1 is returned.
				if (domenu(g_gamecube_status)) {
					continue;
				}
				
				// Perform mapping (conversion), copying data at appropriate
				// places in the output structure (n64 status)
				mapper_copy(current_mapping, g_gamecube_status, g_n64_status);

				// Convert the N64 status mapping structure to a raw buffer we can send
				// to the console in interrupt context.
				n64_status_to_output(g_n64_status, n64_use_buf1 ? n64_tx_buf0 : n64_tx_buf1);

				n64_use_buf1 = !n64_use_buf1;
			}
		}

	}

}


