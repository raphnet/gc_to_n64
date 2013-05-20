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
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "gamepad.h"
#include "gamecube.h"
#include "boarddef.h"
#include "gcn64_protocol.h"

/*********** prototypes *************/
static void gamecubeInit(void);
static char gamecubeUpdate(char);
static char gamecubeChanged(void);
static void gamecubeBuildReport(unsigned char *reportBuffer);

/* What was most recently read from the controller */
static unsigned char last_built_report[GC_REPORT_SIZE];

/* What was most recently sent to the host */
static unsigned char last_sent_report[GC_REPORT_SIZE];

static void gamecubeInit(void)
{
	unsigned char sreg;
	sreg = SREG;
	cli();

	DDRB |= 0x02; // Bit 1 out
	PORTB &= ~0x02; // 0
	
	// data as input
	GC_DATA_DDR &= ~(GC_DATA_BIT);

	// keep data low. By toggling the direction, we make the
	// pin act as an open-drain output.
	GC_DATA_PORT &= ~GC_DATA_BIT;

	SREG = sreg;

	gamecubeUpdate(GAMECUBE_UPDATE_ORIGIN);
}

void gc_decodeAnswer()
{
	unsigned char tmpdata[8];	
	int i;

	// Note: Checking seems a good idea, adds protection
	// against corruption (if the "constant" bits are invalid, 
	// maybe others are : Drop the packet). 
	//
	// However, I have seen bit 2 in a high state. To be as compatible
	// as possible, I decided NOT to look at these bits since instead
	// of being "constant" bits they might have a meaning I don't know.

	// Check the always 0 and always 1 bits
#if 0
	if (gcn64_workbuf[0] || gcn64_workbuf[1] || gcn64_workbuf[2])
		return 1;

	if (!gcn64_workbuf[8])
		return 1;
#endif
	
/*
	(Source: Nintendo Gamecube Controller Protocol
		updated 8th March 2004, by James.)

	Bit		Function
	0-2		Always 0    << RAPH: Not true, I see 001!
	3		Start
	4		Y
	5		X
	6		B
	7		A
	8		Always 1
	9		L
	10		R
	11		Z
	12-15	Up,Down,Right,Left
	16-23	Joy X
	24-31	Joy Y
	32-39	C Joystick X
	40-47	C Joystick Y
	48-55	Left Btn Val
	56-63	Right Btn Val
 */
	
	/* Convert the one-byte-per-bit data generated
	 * by the assembler mess above to nicely packed
	 * binary data. */	
	memset(tmpdata, 0, sizeof(tmpdata));

	for (i=0; i<8; i++) // X axis
		tmpdata[0] |= gcn64_workbuf[i+16] ? (0x80>>i) : 0;
	
	for (i=0; i<8; i++) // Y axis
		tmpdata[1] |= gcn64_workbuf[i+24] ? (0x80>>i) : 0;
	tmpdata[1] ^= 0xff;

	for (i=0; i<8; i++) // C X axis
		tmpdata[2] |= gcn64_workbuf[i+32] ? (0x80>>i) : 0;

	for (i=0; i<8; i++) // C Y axis
		tmpdata[3] |= gcn64_workbuf[i+40] ? 0 : (0x80>>i);	

	for (i=0; i<8; i++) // Left btn value
		tmpdata[4] |= gcn64_workbuf[i+48] ? (0x80>>i) : 0;
	tmpdata[4] ^= 0xff;
	
	for (i=0; i<8; i++) // Right btn value
		tmpdata[5] |= gcn64_workbuf[i+56] ? (0x80>>i) : 0;	
	tmpdata[5] ^= 0xff;

	for (i=0; i<5; i++) // St Y X B A
		tmpdata[6] |= gcn64_workbuf[i+3] ? (0x01<<i) : 0;
	for (i=0; i<3; i++) // L R Z
		tmpdata[6] |= gcn64_workbuf[i+9] ? (0x20<<i) : 0;
	
	for (i=0; i<4; i++) // Up,Down,Right,Left
		tmpdata[7] |= gcn64_workbuf[i+12] ? (0x01<<i) : 0;

	// analog joysticks
	for (i=0; i<6; i++) {
		last_built_report[i] = tmpdata[i];
	}
	
	// buttons
	last_built_report[6] = tmpdata[6];
	last_built_report[7] = tmpdata[7];
}


static char gamecubeUpdate(char origin)
{
	unsigned char tmp=0;
	unsigned char tmpdata[8];
	unsigned char count;


	/* The GetID command. This is required for the Nintendo Wavebird to work... */
	tmp = GC_GETID;
	count = gcn64_transaction(&tmp, 1);
	if (count != GC_GETID_REPLY_LENGTH) {
		return 1;
	}

	/* 
	 * The wavebird needs time. It does not answer the 
	 * folowwing get status command if we don't wait here.
	 *
	 * A good 2:1 safety margin has been chosen.
	 *
	 */
	// 10 : does not work
	// 20 : does not work
	// 25 : works
	// 30 : works
	_delay_us(50);

	// 2013-05-19 RA: 
	//
	// I discovered that in the goldeneye menu, the crosshair moves down 
	// very slowly when the controller is idle. This means that making
	// the zero with the 0x420302 does not work properly. Revert
	// to using the first value returned by GETSTATUS.
	origin = 0; 

	if (origin)	{
		// Ok... at first I was trying a so-called 'get origin command' (0x41) to work,
		// but it did nothing on the wavebird. But this command seems to work. The document
		// Yet another gamecube documentation says 'calibrate ?'. 
		//
		// Wavebird: Fixed data
		// Normal controller: Varies
		//
		// The answer is 80 bits with the first 64 bits have the same as with 
		// the GET_STATUS command.
		tmpdata[0] = 0x42;
		tmpdata[1] = 0x03;
		tmpdata[2] = 0x02;
		
		count = gcn64_transaction(tmpdata, 3);
		if (count != 80) {
			return 1;
		}
	} else {
		tmpdata[0] = GC_GETSTATUS1;
		tmpdata[1] = GC_GETSTATUS2;
		tmpdata[2] = GC_GETSTATUS3(0);
		
		count = gcn64_transaction(tmpdata, 3);
		if (count != GC_GETSTATUS_REPLY_LENGTH) {
			return 1;
		}
	}

	gc_decodeAnswer();

	return 0;
}


static char gamecubeProbe(void)
{
	if (gamecubeUpdate(GAMECUBE_UPDATE_ORIGIN)) {
		return 0;
	}

	return 1;
}

static char gamecubeChanged(void)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }
	
	return memcmp(last_built_report, last_sent_report, GC_REPORT_SIZE);
}

static void gamecubeBuildReport(unsigned char *reportBuffer)
{
	if (reportBuffer != NULL)
		memcpy(reportBuffer, last_built_report, GC_REPORT_SIZE);
	
	memcpy(last_sent_report, last_built_report, GC_REPORT_SIZE);	
}

Gamepad GamecubeGamepad = {
	.report_size			= GC_REPORT_SIZE,
	.init					= gamecubeInit,
	.update					= gamecubeUpdate,
	.changed				= gamecubeChanged,
	.buildReport			= gamecubeBuildReport,
	.probe					= gamecubeProbe,
};

Gamepad *gamecubeGetGamepad(void)
{
	return &GamecubeGamepad;
}

