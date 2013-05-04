;
;	n64_to_wii: AVR Microcontroller firmware for converting N64 controller
;	          protocol to Nintendo Wii/Gamecube controller protocol.
;    Copyright (C) 2007-2008  Raphael Assenat <raph@raphnet.net>
;
;    This program is free software; you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation; either version 2 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License along
;    with this program; if not, write to the Free Software Foundation, Inc.,
;    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
;
;	------------------------------------------------------------------------   
;

#include <avr/io.h>

#include "n64_isr.h"

#define R7_BIT_UNSWAP_L_Z	0
#define R7_BIT_SIN			1
#define R7_BIT_NOSCALE		2
#define R7_BIT_SCALE_1_5	3
#define R7_BIT_ALT_1		4
#define R7_BIT_ALT_2		5
#define R7_BIT_ALT_3		6
#define R7_BIT_ALT_4		7

#define N64_RX_BUF_SIZE	33

.lcomm n64_rx_buf, N64_RX_BUF_SIZE
.lcomm n64_tx_buf0, N64_TX_BUF_SIZE	; reply is 32 bits + 1 stop bit
.lcomm n64_tx_buf1, N64_TX_BUF_SIZE	;
.lcomm n64_tx_id_reply 3;
.lcomm n64_tx_id2_reply 3;
.lcomm n64_got_command 1;
.lcomm n64_use_buf1 1;

.text
.global SIG_INTERRUPT0
.global n64_rx_buf
.global n64_tx_buf0
.global n64_tx_buf1
.global n64_tx_id_reply
.global n64_tx_id2_reply
.global n64_got_command
.global n64_use_buf1

;.global sendGCData

#define IO_SREG	_SFR_IO_ADDR(SREG)
#define IO_PORTD _SFR_IO_ADDR(PORTD)
#define IO_DDRD _SFR_IO_ADDR(DDRD)
#define IO_PIND	_SFR_IO_ADDR(PIND)
#define IO_PORTC _SFR_IO_ADDR(PORTC)
#define IO_DDRC _SFR_IO_ADDR(DDRC)
#define IO_PINC _SFR_IO_ADDR(PINC)
#define IO_PORTB _SFR_IO_ADDR(PORTB)
#define IO_DDRB _SFR_IO_ADDR(DDRB)
#define IO_PINB	_SFR_IO_ADDR(PINB)
#define IO_MCUCR _SFR_IO_ADDR(MCUCR)
#define IO_GICR	_SFR_IO_ADDR(GICR)

#define CLK_H_LATCH_H	0x30
#define CLK_H_LATCH_L	0x20
#define CLK_L_LATCH_L	0x00

#define DEBUG_PORTD_BIT 0x00	/* In PORTD */

#define DEBUG_ON2 	sbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG
#define DEBUG_OFF2 	cbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG
#if 0
#define DEBUG_ON 	sbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG
#define DEBUG_OFF 	cbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG
#else
#define DEBUG_ON	nop
#define DEBUG_OFF	nop
#endif

#define DEBUG2_ON 	sbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG
#define DEBUG2_OFF 	cbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG

#define GC_DATA_BIT		0x02	/* in PORTD */
#define GC_DATA_MASK	0x04	

; 0x06 here combined with the fast command selection code
; yields a response time of 8.4uS. Much better than
; the 18.80uS from previous releases.
;
; 0x0A : 9.8 uS (eventually fails in F-Zero X)
;
#define DATA_FALL_TIMEOUT	0x06
#define DATA_RAISE_TIMEOUT	0x15 

; Useful
#define xl	r26
#define xh	r27
#define yl	r28
#define yh	r29
#define zl	r30
#define zh	r31



SIG_INTERRUPT0:
	push r1	
	in r1, IO_SREG
	push zl
	push zh
	push r20
	push r16
	push r17
	push r23
	ldi	zl, lo8(n64_rx_buf)
	ldi zh, hi8(n64_rx_buf)
	ldi r16, N64_RX_BUF_SIZE				; We can accept up to 32 bits
	rjmp fell


readNextBit:
	ldi r17, DATA_FALL_TIMEOUT	; setup a timeout
waitFall:				
	dec r17						; 1
	breq rx_complete_f			; 1
	in r20, IO_PIND				; 1 : read the input port
	andi r20, GC_DATA_MASK		; 1 : isolate the input bit
	brne waitFall				; 2 : if still high, loop

	; When we first jump to 'fell' (when the interrupt
	; occurs), we have already wasted many cycles. Those
	; delays are there to compensate and make sure we
	; always sample the data line where we want.
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
fell:

	; Ok, so there is now a 0 on the wire. 
	;  Worst case, we are at the 9th cycle.
	;  Best case, we are at the 3rd cycle.
	;  Let's assume we are at cycle 6.

	;  cycle: 1-16 16-32 32-48 48-64
	;   high:  0     1     1     1
	;   low:  0     0     0     1

	;  I check the pin on the 32th cycle which is 
	;  the safest place. Assuming we are on the 6th cycle,
	;  we need to waste 26 cycles. This is done by
	;  adjusting the align_delay subroutine.

	rcall align_delay				; waste enough cycles to be aligned on cycle 32
	DEBUG_ON
	in r20, IO_PIND					; read the input port
	DEBUG_OFF
	andi r20, GC_DATA_MASK			; isolate the data bit
	st z+, r20						; store the value


	dec r16							; One less bit available...
	breq rx_overflow

	ldi r17, DATA_RAISE_TIMEOUT
waitHigh:
	dec r17
	breq rx_complete_h
	in r20, IO_PIND
	andi r20, GC_DATA_MASK
	breq waitHigh					; Still high? Keep looping

	rjmp readNextBit

	; At this point, we have read a full 8bit command. 
	
rx_overflow:

	rjmp interrupt_done_poll

rx_complete_f:
;	DEBUG_ON2
;	DEBUG_OFF2
rx_complete_h:
	
	; r16 counts down from 	N64_RX_BUF_SIZE
	
	ldi r20, N64_RX_BUF_SIZE
	sub r20, r16   ;  r20 = N64_RX_BUF - r16

	; Longer commands for rumble or memory card ignored (for now).
	; So jump to interrupt_done_poll in that case.
	cpi r20, 9
	brne interrupt_done_poll

got_8bit_command:

	; Commands:
	; 0x00 : Get capability
	; 0x01 : Get button status
	

	; repoint Z to origin of reception buffer	
	ldi	zl, lo8(n64_rx_buf)
	ldi zh, hi8(n64_rx_buf)

#ifdef SLOW_VERSION
	; Convert from 8 bytes to one byte, reult in r20
	rcall rx_buf_to_byte

	tst r20
	breq cap

	cpi r20, 0x01
	breq stat

	cpi r20, 0xff
	breq cap2
#else
	; Detect 0xff by looking at the most significant bit
	ld r20, z
	tst r20
	brne cap2

	; Detect 0x00 or 0x01 by looking at the least significant bit
	ldd r20, z+8
	tst r20
	breq cap

	rcall sendStatus

	; Notify the main loop about the poll
	ldi r20, 1
	ldi	zl, lo8(n64_got_command)
	ldi zh, hi8(n64_got_command)
	st z, r20

	rjmp interrupt_done_poll

#endif

	DEBUG2_ON
	nop
	nop
	DEBUG2_OFF
	; ignore unknwn
	rjmp interrupt_done_poll

interrupt_done_poll:
	pop r23
	pop r17
	pop r16	
	pop r20
	pop zh
	pop zl
	out IO_SREG, r1
	pop r1
	reti


cap2:
	rcall sendCapabilityWord2
	rjmp interrupt_done_poll
	
cap:
	rcall sendCapabilityWord
	rjmp interrupt_done_poll
	
		
;stat:
;		rcall sendStatus
;	rjmp interrupt_done_poll

	/*********************** DELAY *************/
align_delay:
	ldi r17, 2
lp634534:
	dec r17
	brne lp634534
	ret




	/***************
	Convert 8 bytes from the receive buffer
	to one byte (repack)

	receive buffer in Z, Z increased by 8 side effect
	8 bit value in r20

	First byte will be MSB
	****************/
rx_buf_to_byte:
	push r16
	push r17

	clr r20
	ldi r16, 0x80

nbb:
	ld r17, z+ ; load byte

	tst r17		; if clear, skip bit setting
	breq noset

	or r20, r16

noset:
	lsr r16		; shift for next bit
	brne nbb	; loop until 0

rxb_ret:
	pop r17
	pop r16
	ret


	/**********
	unpack bits to several bytes
	source in r20.
	Writes to Z. Increments Z.
	Byte MSB out first.
	************/
byte_to_rxbuf:
	push r16
	push r17

	ldi r16, 0x80
nbrrr:
	mov r17, r20
	and r17, r16
	st z+, r17
	lsr r16
	brne nbrrr

	pop r17
	pop r16	
	ret
	
	

sendCapabilityWord:
	push xl
	push xh

	ldi xl, lo8(n64_tx_id_reply)
	ldi xh, hi8(n64_tx_id_reply)
	ldi zl, lo8(n64_rx_buf)
	ldi zh, hi8(n64_rx_buf)

	ld r20, x+
	rcall byte_to_rxbuf
	ld r20, x+
	rcall byte_to_rxbuf
	ld r20, x+
	rcall byte_to_rxbuf

	pop xh
	pop xl

	ldi zl, lo8(n64_rx_buf)
	ldi zh, hi8(n64_rx_buf)
	ldi r23, 24
	rcall sendGCData
	ret

sendCapabilityWord2:
	push xl
	push xh

	ldi xl, lo8(n64_tx_id2_reply)
	ldi xh, hi8(n64_tx_id2_reply)
	ldi zl, lo8(n64_rx_buf)
	ldi zh, hi8(n64_rx_buf)

	ld r20, x+
	rcall byte_to_rxbuf
	ld r20, x+
	rcall byte_to_rxbuf
	ld r20, x+
	rcall byte_to_rxbuf

	pop xh
	pop xl



	ldi zl, lo8(n64_rx_buf)
	ldi zh, hi8(n64_rx_buf)
	ldi r23, 24
	rcall sendGCData
	ret


	/******************************************
	Load the latest buffer address in Z and
	send it.
	******************************************/
sendStatus:
	ldi	zl, lo8(n64_use_buf1)
	ldi zh, hi8(n64_use_buf1)
	ld r23, z
	tst r23
	brne useBuf1

	; Select the buffer
	ldi	zl, lo8(n64_tx_buf0)
	ldi zh, hi8(n64_tx_buf0)
	rjmp bufferSelected
useBuf1:
	ldi	zl, lo8(n64_tx_buf1)
	ldi zh, hi8(n64_tx_buf1)	

bufferSelected:
	; Send the packet
	ldi r23, 32
dosendGCData:
	rcall sendGCData
	ret


	/************************************************
	* Send data using the N64/GC serial protocol which
	* is as follows:
	*   0          1
	*     __        _____
	* ____|       __|
	* ^   ^ ^     ^ ^   ^
	*  3us 1us    1us 3us
	*
	* To send a 1, the pin direction is set to input.
	* To send a 0, the pin direction is set to output. 
	*	(of course, it's value is preset to zero)
	* 
	* At 16 mhz, a 1us period is 16 cycles. Thus a 3us period
	* is 48 cycles.
	*
	* Number of bits to send is passed in r23
	* Z must point to first byte of data. Every byte
	* represents one bit (programming laziness..). A logic
	* 1 is sent when a byte is non-zero.
	*
	* A stop bit is added at thy end of the packet.
	*
	* Used registers: r19, r20
	************************************************/
sendGCData:
	push r19
	push r20
	push r23

;	pop r23
;	pop r20
;	pop r19

lp_sendGCData_bits:
	ld r19, z+
	tst r19
	brne send1
	nop
send0:
	sbi IO_DDRD, GC_DATA_BIT	; Pull bus to 0 
	
	ldi r20, 15
lp_send0_3us:
	dec r20
	brne lp_send0_3us
	nop

	cbi IO_DDRD, GC_DATA_BIT	; Release bus to 1

	ldi r20, 2
lp_send0_1us:
	dec r20
	brne lp_send0_1us

	dec r23
	brne lp_sendGCData_bits

	nop
	nop
	nop
	nop
	nop
	nop
	; STOP BIT
	sbi IO_DDRD, GC_DATA_BIT ; Pull low for stop bit
	ldi r20, 4
stbdly0:
	dec r20
	brne stbdly0
	nop
	cbi IO_DDRD, GC_DATA_BIT ;	Release

	pop r23
	pop r20
	pop r19
	ret

send1:
	sbi IO_DDRD, GC_DATA_BIT	; Pull bus to 0

	ldi r20, 4
lp_send1_1us:
	dec r20
	brne lp_send1_1us
	nop
	nop
	
	cbi IO_DDRD, GC_DATA_BIT	; Release bus to 1

	ldi r20, 12
lp_send1_3us:
	dec r20
	brne lp_send1_3us
	nop
	nop

	dec r23
	brne lp_sendGCData_bits

	nop
	nop
	nop
	nop
	nop
	nop

	; STOP BIT
	sbi IO_DDRD, GC_DATA_BIT ; Pull low for stop bit
	ldi r20,4
stbdly1:
	dec r20
	brne stbdly1
	nop
	cbi IO_DDRD, GC_DATA_BIT	; Release
	
	pop r23
	pop r20
	pop r19
	
	ret

