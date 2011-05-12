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
#include "support.h"
#include "boarddef.h"


/*********** prototypes *************/
static void gamecubeInit(void);
static char gamecubeUpdate(void);
static char gamecubeChanged(void);
static void gamecubeBuildReport(unsigned char *reportBuffer);


/* What was most recently read from the controller */
static unsigned char last_built_report[GC_REPORT_SIZE];

/* What was most recently sent to the host */
static unsigned char last_sent_report[GC_REPORT_SIZE];

/* For probe */
static char last_failed;

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

	gamecubeUpdate();

}


static char gamecubeUpdate(void)
{
	int i;
	unsigned char tmp=0;
	unsigned char tmpdata[8];	
	unsigned char volatile results[65];
	unsigned char count;

	/* Use .c functionality to send a 8 bit
	 * command. In this case, it is the GetID 
	 * command. This is required for the Nintendo 
	 * Wavebird to work... */
	_n64Update(0x00);
	_delay_us(100);	
	tmp = 0x00; // get status
	/*
	 * z: Points to current source byte
	 * r16: Holds the bit to be AND'ed with current byte
	 * r17: Holds the current byte
	 * r18: Holds the number of bits left to send.
	 * r19: Loop counter for delays
	 *
	 * Edit sbi/cbi/andi instructions to use the right bit!
	 * 
	 * 3 us == 36 cycles
	 * 1 us == 12 cycles
	 *
	 * Note: I know the code below is not very clean... but it works.
	 */
	asm volatile(
"			push r30				\n"
"			push r31				\n"

"			ldi r16, 0x80			\n" // 1
"nextBit:							\n" 
"			ldi r17, 0x40				\n" // 2		// BITS 0-7 (MSB sent first)
"			and r17, r16			\n" // 1
"			breq send0				\n" // 2
"			nop						\n"

"send1:								\n"
"			sbi %1, 3				\n" // 2
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\n			\n" // 3
"			cbi %1, 3				\n" // 2
"				ldi r19, 12			\n"	// 1 
"lp1:			dec r19				\n"	// 1 
"				brne lp1			\n"	// 2
"				nop\nnop			\n" // 2
"			lsr r16					\n" // 1
"			breq done				\n" // 1
"			rjmp nextBit			\n" // 2
		
"send0:		sbi %1, 3				\n"	// 2
"				ldi r19, 15			\n"	// 1
"lp0:			dec r19				\n"	// 1
"				brne lp0			\n"	// 2
"				nop					\n" // 1

"          	cbi %1,3				\n" // 2
"				nop\nnop\n				\n" // 2
"				nop\nnop\nnop\nnop	\n" // 4

"			lsr r16					\n" // 1
"			breq done				\n" // 1
"			rjmp nextBit			\n" // 2

"done:								\n"
"			cbi %1, 3				\n"


///////////////////////////////////////////// BITS 8-15

"			ldi r16, 0x80			\n" // 1
"nextBitA:							\n" 
"			ldi r17, 0x03			\n" // 2			// BITS 8-15
"			and r17, r16			\n" // 1
"			breq send0A				\n" // 2
"			nop						\n"

//"send1:								\n"
"			sbi %1, 3				\n" // 2
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\n			\n" // 3
"			cbi %1, 3				\n" // 2
"				ldi r19, 12			\n"	// 1 
"lp1A:			dec r19				\n"	// 1 
"				brne lp1A			\n"	// 2
"				nop\nnop			\n" // 2
"			lsr r16					\n" // 1
"			breq doneA				\n" // 1
"			rjmp nextBitA			\n" // 2
		
"send0A:	sbi %1, 3				\n"	// 2
"				ldi r19, 15			\n"	// 1
"lp0A:			dec r19				\n"	// 1
"				brne lp0A			\n"	// 2
"				nop					\n" // 1

"          	cbi %1,3				\n" // 2
"				nop\nnop\n				\n" // 2
"				nop\nnop\nnop\nnop	\n" // 4

"			lsr r16					\n" // 1
"			breq doneA				\n" // 1
"			rjmp nextBitA			\n" // 2

"doneA:								\n"
"			cbi %1, 3				\n"

////////////////////////// BITS 16-23
"			ldi r16, 0x80			\n" // 1
"nextBitB:							\n" 
"			mov r17, %2				\n" // 2 		// BITS 16-23
"			and r17, r16			\n" // 1
"			breq send0B				\n" // 2
"			nop						\n"

//"send1B:								\n"
"			sbi %1, 3				\n" // 2
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\n			\n" // 3
"			cbi %1, 3				\n" // 2
"				ldi r19, 12			\n"	// 1 
"lp1B:			dec r19				\n"	// 1 
"				brne lp1B			\n"	// 2
"				nop\nnop			\n" // 2
"			lsr r16					\n" // 1
"			breq doneB				\n" // 1
"			rjmp nextBitB			\n" // 2
		
"send0B:	sbi %1, 3				\n"	// 2
"				ldi r19, 15			\n"	// 1
"lp0B:			dec r19				\n"	// 1
"				brne lp0B			\n"	// 2
"				nop					\n" // 1

"          	cbi %1,3				\n" // 2
"				nop\nnop\n				\n" // 2
"				nop\nnop\nnop\nnop	\n" // 4

"			lsr r16					\n" // 1
"			breq doneB				\n" // 1
"			rjmp nextBitB			\n" // 2

"doneB:								\n"
"			cbi %1, 3				\n"
"			nop\nnop\nnop\nnop		\n"	// 4


// Stop bit (1us low, 3us high)
"          	sbi %1,3				\n" // 2
"			nop\nnop\nnop\nnop		\n" // 4
"			nop\nnop\nnop\nnop		\n" // 4
"			nop\nnop\nnop\nnop		\n" // 4
"			nop\nnop\n		\n" // 5
"			cbi %1,3				\n" 
			
"			nop\nnop\nnop\nnop		\n" // 4
"			nop\nnop\nnop\nnop		\n" // 4


"anti_slow_rise:\n"
"			in r17, %4\n"
"           andi r17, 0x20\n"
"           breq anti_slow_rise\n"

			// Response reading part //
			// r16: loop Counter
			// r17: Reference state
			// r18: Current state
			// r19: bit counter
		
"			ldi r19, 64\n			" // 1  We will receive 32 bits

"st:\n								"
"			ldi r16, 0x3f			\n" // setup a timeout
"waitFall:							\n" 
"			dec r16					\n" // 1
"			breq timeout			\n" // 1 
"			in r17, %4				\n" // 1  read the input port
"			andi r17, 0x08			\n" // 1  isolate the input bit
"			brne waitFall			\n" // 2  if still high, loop

// OK, so there is now a 0 on the wire.
// Worst case, we are at the 9th cycle.
// Best case, we are at the 4th cycle.
// 	Middle: cycle 6
// 
// cycle: 1-12 12-24 24-36 36-48
//  high:  0     1     1     1
//	 low:  0     0     0     1
//
// I check the pin on the 24th cycle which is the safest place.

"			nop\nnop\nnop\nnop\n	" // 4
"			nop\nnop\nnop\nnop\n	" // 4
"			nop\nnop\nnop\nnop\n	" // 4
"			nop\nnop\nnop\nnop\n	" // 4
"			nop\nnop\nnop\nnop\n	" // 4
"			nop\nnop\n	" // 2
			
"			sbi %5, 0\n"				// DEBUG
			// We are now more or less aligned on the 24th cycle.			
"			in r18, %4\n			" // 1  Read from the port
"			cbi %5, 0\n"				// DEBUG
"			andi r18, 0x08\n		" // 1  Isolate our bit
"			st z+, r18\n			" // 2  store the value

"			dec r19\n				" // 1 decrement the bit counter
"			breq ok\n				" // 1

"			ldi r16, 0x3f\n			" // setup a timeout
"waitHigh:\n						"
"			dec r16\n				" // decrement timeout
"			breq timeout\n			" // handle timeout condition
"			in r18, %4\n			" // Read the port
"			andi r18, 0x08\n		" // Isolate our bit
"			brne st\n				" // Continue if set
"			rjmp waitHigh\n			" // loop otherwise..

"ok:\n"
"			clr %0\n				"
"			rjmp end\n				"

"error:\n"
"			sbi %5, 0\n"
"			mov %0, r19				\n" // report how many bits were read
"			rjmp end				\n"

"timeout:							\n"
"			clr %0					\n"
"			com %0					\n" // 255 is timeout

"end:\n"
"			pop r31\n"
"			pop r30\n"
"			cbi %5, 0\n"
			: "=r" (count)
			: "I" (_SFR_IO_ADDR(GC_DATA_DDR)), "r"(tmp), 
				"z"(results), "I" (_SFR_IO_ADDR(GC_DATA_PIN)),
				"I" (_SFR_IO_ADDR(PORTD)) // DEBUG
			: "r16","r17","r18","r19"
			);

	if (count == 255) {
		last_failed = 1;
		return 1; // failure
	}

	
/*
	(Source: Nintendo Gamecube Controller Protocol
		updated 8th March 2004, by James.)

	Bit		Function
	0-2		Always 0 
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
		tmpdata[0] |= results[i+16] ? (0x80>>i) : 0;
	
	for (i=0; i<8; i++) // Y axis
		tmpdata[1] |= results[i+24] ? (0x80>>i) : 0;
	tmpdata[1] ^= 0xff;

	for (i=0; i<8; i++) // C X axis
		tmpdata[2] |= results[i+32] ? (0x80>>i) : 0;


	// Raph: October 2007. C stick vertical orientation 
	// was wrong. But no one complained...
#if INVERTED_VERTICAL_C_STICK
	for (i=0; i<8; i++) // C Y axis
		tmpdata[3] |= results[i+40] ? (0x80>>i) : 0;	
#else
	for (i=0; i<8; i++) // C Y axis
		tmpdata[3] |= results[i+40] ? 0 : (0x80>>i);	
#endif

	for (i=0; i<8; i++) // Left btn value
		tmpdata[4] |= results[i+48] ? (0x80>>i) : 0;
	tmpdata[4] ^= 0xff;
	
	for (i=0; i<8; i++) // Right btn value
		tmpdata[5] |= results[i+56] ? (0x80>>i) : 0;	
	tmpdata[5] ^= 0xff;

	for (i=0; i<5; i++) // St Y X B A
		tmpdata[6] |= results[i+3] ? (0x01<<i) : 0;
	for (i=0; i<3; i++) // L R Z
		tmpdata[6] |= results[i+9] ? (0x20<<i) : 0;
	
	for (i=0; i<4; i++) // Up,Down,Right,Left
		tmpdata[7] |= results[i+12] ? (0x01<<i) : 0;

	// analog joysticks
	for (i=0; i<6; i++) {
		last_built_report[i] = tmpdata[i];
	}
	
	// buttons
	last_built_report[6] = tmpdata[6];
	last_built_report[7] = tmpdata[7];

	return 0; // success
}

static char gamecubeProbe(void)
{
	last_failed = 0;
	gamecubeUpdate();

	if (!last_failed)
		return 1;

	return 0;
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

