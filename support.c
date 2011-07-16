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
#include <avr/io.h>
#include <util/delay.h>

#include "support.h"
#include "boarddef.h"

// used to send a 8 bit command..
int _n64Update(unsigned char tmp)
{
	int i;
	unsigned char tmpdata[4];	
	unsigned char volatile results[65];
	unsigned char count;

	
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
	 */
	asm volatile(
"			push r30				\n"
"			push r31				\n"
			
"			ldi r16, 0x80			\n" // 1
"nextBit:							\n" 
"			mov r17, %2				\n" // 2
//"			ldi r17, 0x0f			\n" // 2
"			and r17, r16			\n" // 1
"			breq send0				\n" // 2
"			nop						\n"

"send1:								\n"
"			sbi %1, 3				\n" // 2
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop\nnop\nnop	\n" // 4
"				nop\nnop	\n" // 4
"			cbi %1, 3				\n" // 2
"				ldi r19, 12			\n"	// 1 
"lp1:			dec r19				\n"	// 1 
"				brne lp1			\n"	// 2
"				nop\nnop			\n" // 2
"			lsr r16					\n" // 1
"			breq done				\n" // 1
"			rjmp nextBit			\n" // 2
		
/* Send a 0: 3us Low, 1us High */
"send0:		sbi %1, 3				\n"	// 2
"				ldi r19, 15			\n"	// 1
"lp0:			dec r19				\n"	// 1
"				brne lp0			\n"	// 2
"				nop					\n" // 1

"          	cbi %1,3				\n" // 2
"				nop\nnop\nnop\nnop\nnop\nnop				\n" // 2

"			lsr r16					\n" // 1
"			breq done				\n" // 1
"			rjmp nextBit			\n" // 2

"done:								\n"
"			cbi %1, 3				\n"
"			nop\nnop\nnop\nnop		\n"	// 4
"			nop\nnop\nnop\nnop		\n"	// 2


// Stop bit (1us low, 3us high)
"          	sbi %1,3				\n" // 2
"			nop\nnop\nnop\nnop		\n" // 4
"			nop\nnop\nnop\nnop		\n" // 4
"			nop\nnop\nnop\nnop		\n" // 4
"			nop\nnop		\n" // 4
"			cbi %1,3				\n" 
			

			// Response reading part //
			// r16: loop Counter
			// r17: Reference state
			// r18: Current state
			// r19: bit counter
		
"			ldi r19, 32\n			" // 1  We will receive 32 bits

"st:\n								"
"			ldi r16, 0xff			\n" // setup a timeout
"waitFall:							\n" 
"			dec r16					\n" // 1
"			breq timeout			\n" // 1 
"			in r17, %4				\n" // 1  read the input port
"			andi r17, 0x20			\n" // 1  isolate the input bit
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

"			cbi %5, 1\n"				// DEBUG
"			nop\nnop\n	" // 2
"			nop\nnop\nnop\nnop\n	" // 4
"			nop\nnop\nnop\nnop\n	" // 4
"			nop\nnop\nnop\nnop\n	" // 4
"			nop\n"
			
			// We are now more or less aligned on the 24th cycle.			
"			in r18, %4\n			" // 1  Read from the port
"			sbi %5, 1\n"				// DEBUG
"			andi r18, 0x08\n		" // 1  Isolate our bit
"			st z+, r18\n			" // 2  store the value

"			dec r19\n				" // 1 decrement the bit counter
"			breq ok\n				" // 1

"			ldi r16, 0xff\n			" // setup a timeout
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
"			sbi %5, 1\n"
"			mov %0, r19				\n" // report how many bits were read
"			rjmp end				\n"

"timeout:							\n"
"			clr %0					\n"
"			com %0					\n" // 255 is timeout

"end:\n"
"			pop r31\n"
"			pop r30\n"
"			cbi %5, 1\n"
			: "=r" (count)
			: "I" (_SFR_IO_ADDR(GC_DATA_DDR)), "r"(tmp), 
				"z"(results), "I" (_SFR_IO_ADDR(GC_DATA_PIN)),
				"I" (_SFR_IO_ADDR(PORTB))
			: "r16","r17","r18","r19"
			);

	if (count == 255) {
		return -1; // failure
	}
	
	return 0;
}


