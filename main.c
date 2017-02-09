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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "buzzer.h"
#include "timer0.h"
#include "lut.h"
#include "menu.h"

#include "gamecube.h"

//#include "gc_offsets.h"
//#include "n64_offsets.h"

#include "n64_isr.h"
#include "sync.h"
#include "mapper.h"
#include "gamecube_mapping.h"
#include "n64_mapping.h"
#include "compat.h"
#include "eeprom.h"

/* After this many read failures in a row, consider the controller
 * gone and wait for a new one. (whose first read will be considered
 * the origin) */
#define READ_FAIL_LIMIT		20

#define DEBUG_LOW()		do { PORTD &= ~(1); } while(0)
#define DEBUG_HIGH()	do { PORTD |= 1; } while(0)

#ifdef AT168_COMPATIBLE
/* This is just a constant that will be in flash. The update tool will search
 * the hexfile for this sequence. In not found, update won't take place. The
 * goal is to prevent temporary bricking due to users using the wrong .hex.   */
const char signature[] PROGMEM = "41d938a8-6f8a-11e5-a45e-001bfca3c593";

/* This marker is used by the bootloader to protect against partial programming.
 * Since programming is done with increasing addresses, if it is interrupted, this
 * sequence at the end will be missing if interrupted. */
const char endmarker[4] __attribute__((section(".endmarker")))  = { 0x12, 0x34, 0x56, 0x78 };
#endif

void enter_bootloader(void);

Gamepad *g_gcpad;
unsigned char gc_report[GC_REPORT_SIZE];

// When non-zero, triggers saving the current mapping to specified slot
// in the main loop.
static volatile unsigned char g_todo_save_mapping;

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
	{ MAP_GC_AXB_L_SLIDER,		MAP_N64_NONE },
	{ MAP_GC_AXB_R_SLIDER,		MAP_N64_NONE },
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
void byteTo8Bytes(unsigned char val, unsigned char volatile *dst)
{
	unsigned char c = 0x80;

	do {
		*dst = val & c;
		dst++;
		c >>= 1;
	} while(c);
}


#define IS_ACTIVE(a) ((a).value != (a).def)

unsigned char long_command_handler(unsigned char len)
{
	if (len >= 2)
	{
		if (g_n64_buf[0] == 'R') {
			switch (g_n64_buf[1]) {
				case 0x00: // echo
					return len;

				case 0x01: // Get info
					memset((void*)g_n64_buf, 0, sizeof(g_n64_buf));
					g_n64_buf[0] = 0; // Isbootloader? No.
					g_n64_buf[1] = g_eeprom_data.defmap;
					g_n64_buf[2] = g_eeprom_data.deadzone_enabled;
					g_n64_buf[3] = g_eeprom_data.old_v1_5_conversion;
					g_n64_buf[4] = g_eeprom_data.wide_conversion;
#ifdef AT168_COMPATIBLE
					g_n64_buf[9] = 1; // Upgradable
#endif
					strcpy((void*)(g_n64_buf+10), VERSION_STR);
					return 10 + strlen(VERSION_STR) + 1;

				case 0x02: // Get mapping
					if (len >= 4) {
						int id = g_n64_buf[2];
						int chunk = g_n64_buf[3];
						unsigned short mapping_size = sizeof(current_mapping);

						if (id >= 0 && id <= 4) {
							unsigned char *mapping_pointer;

							if (id) {
								mapping_pointer = g_eeprom_data.appdata + (sizeof(current_mapping) * (id-1));
							} else {
								mapping_pointer = (unsigned char*)&current_mapping[0];
							}

							if (chunk == 0) {
								g_n64_buf[0] = mapping_size & 0xff;
								return 1;
							} else {
								if (chunk == 1) {
									memcpy((void*)g_n64_buf, mapping_pointer, 32);
									return 32;
								} else if (chunk == 2) {
									memcpy((void*)g_n64_buf, mapping_pointer + 32, sizeof(current_mapping) - 32);
									return sizeof(current_mapping) - 32;
								}
							}

							return 0;
						}
					}
					break;

				case 0x03: // Set mapping
					if (len > 3) {
						int chunk = g_n64_buf[2];
						unsigned char *mapping_pointer;

						mapping_pointer = (unsigned char*)&current_mapping[0];
						if (chunk == 1) {
							mapping_pointer += 32;
						} else {
							memset(mapping_pointer, 0xff, sizeof(current_mapping));
						}

						memcpy(	mapping_pointer, (void*)(g_n64_buf+3), len-4 );
						return 1;
					}
					break;

				case 0x04: // Save current mapping
					if (len > 2) {
						int id = g_n64_buf[2];

						if (id < 1 || id > 4) {
							g_n64_buf[0] = 0x01; // NACK (out of range)
							return 1;
						}

						// This need to be done in the main loop. It takes
						// too much time and the host may timeout waiting
						// for an answer.
						//
						// Setting this to non-zero triggers a write. It gets
						// cleared back to 0 once it's done.
						//
						// The caller should poll the adapter for business
						// before sending this.
						g_todo_save_mapping = id;
						g_n64_buf[0] = 0x00;
						return 1; // ACK
					}
					break;

				case 0xf9:
					if (g_todo_save_mapping) {
						g_n64_buf[0] = 1;
					} else {
						g_n64_buf[0] = 0;
					}
					return 1;

#ifdef AT168_COMPATIBLE
				case 0xff:
					enter_bootloader();
					break;
#endif
			}
		}
	}
	return 0;
}

void packbytes(unsigned char *input, unsigned char volatile *output, int bytes)
{
	int i;
	unsigned char b;
	unsigned char tmp;

	for (i=0; i<bytes; i++) {
		for (tmp=0, b=0x80; b; b >>= 1) {
			if (*input) {
				tmp |= b;
			}
			input++;
		}
		*output = tmp;
		output++;
	}
}

void n64_status_to_output(struct mapping_controller_unit *n64s, unsigned char volatile output[4])
{
	char x,y;
	unsigned char dstbuf[32];

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

	packbytes(dstbuf, output, 4);
}

int main(void)
{
	char res, read_fail_count = 0;

	g_gcpad = gamecubeGetGamepad();


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

#ifdef AT168_COMPATIBLE
	EICRA = 0x02;
	EIMSK = 0x01;
#else
	// configure external interrupt 1 to trigger on falling edge.
	MCUCR |= 1<<ISC11;
	MCUCR &= ~(1<<ISC10);

	GICR |= (1<<INT0);
#endif

	memset((void*)n64_tx_buf0, 0, sizeof(n64_tx_buf0));
	memset((void*)n64_tx_buf1, 0, sizeof(n64_tx_buf1));

	// Set answer to 0x05 0x00 0x02
	n64_tx_id_reply[0] = 0x05;
	n64_tx_id_reply[1] = 0x00;
	n64_tx_id_reply[2] = 0x02;

	buzzer_init();
	blips(1);

	if (eeprom_init()) {
		/* This is to test the buzzer right
		 * after programming completes. The next time
		 * the board powers up the buzzer will be
		 * silent, unless the eeprom got corrupted. */
		blips(2);
	}


	sei();

	loadMappingId(g_eeprom_data.defmap);

	timerStart();

	g_gcpad->init();

	DEBUG_HIGH();
	_delay_ms(500);

	/* Read from Gamecube controller. As long as this fails,
	 * keep trying. We need the initial read to use as origin
	 * position. */
wait_for_controller:
	while (0 != (res = g_gcpad->update(GAMECUBE_UPDATE_ORIGIN))) {
		_delay_ms(16);
	}
	read_fail_count = 0;

	g_gcpad->buildReport(gc_report);

	// Learn the joystick origin to use
	_delay_ms(16);
	setOriginsFromReport(gc_report);

	gc_report_to_mapping(gc_report, g_gamecube_status);


	if (g_gamecube_status[MAP_GC_BTN_START].value) {
		menumain(g_gamecube_status);
	}
	
	DEBUG_LOW();

	sync_init();

	while(1)
	{
		if (n64_got_command) {
			n64_got_command = 0;
			sync_master_polled_us();
		}

		if (g_todo_save_mapping) {
			saveCurrentMappingTo(g_todo_save_mapping);
			g_todo_save_mapping = 0;
		}

		if (sync_may_poll()) {	
			DEBUG_HIGH();
			timerIntOff();		
			res = g_gcpad->update(GAMECUBE_UPDATE_NORMAL);
			timerIntOn();
			DEBUG_LOW();

			if (res) {
				read_fail_count++;
				if (read_fail_count > READ_FAIL_LIMIT) {
					blips(1);
					goto wait_for_controller;
				}
			} else {
				read_fail_count = 0;
			}

			if (g_gcpad->changed()) {
				DEBUG_HIGH();
				// Read the gamepad	
				g_gcpad->buildReport(gc_report);				
				
				// Convert the data we got from the gamepad reader
				// to a mapping structure.
				gc_report_to_mapping(gc_report, g_gamecube_status);

				// Maybe enter configuration menu mode. If we did,
				// 1 is returned.
							
				// Perform mapping (conversion), copying data at appropriate
				// places in the output structure (n64 status)
				mapper_copy(current_mapping, g_gamecube_status, g_n64_status);

				// Convert the N64 status mapping structure to a raw buffer we can send
				// to the console in interrupt context.
				n64_status_to_output(g_n64_status, n64_use_buf1 ? n64_tx_buf0 : n64_tx_buf1);

				n64_use_buf1 = !n64_use_buf1;
				DEBUG_LOW();
			}

			domenu(g_gamecube_status);
	
		}

	}

}

void enter_bootloader(void)
{
	cli();
	// Make sure timer interrupt is not kept
	timerIntOff();
	asm volatile("ijmp" :: "z" (0x1C00));
}
