;
;	n64_to_wii: AVR Microcontroller firmware for converting N64 controller
;	          protocol to Nintendo Wii/Gamecube controller protocol.
;    Copyright (C) 2007-2015  Raphael Assenat <raph@raphnet.net>
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
; 2015-02-07: Updated interrupt name for compilation with recent avr-libc
;
; 2015-09-29: Rewrote (unrolled) reception code.
; for specific values is left in r16. Thanks to this, the firmware now answers requests
; as fast as an official N64 controller.
;
; Also, corrected transmission timing to match official N64 controllers (4us cycles)
;
; 2015-09-30: Improved reception code to support multi-byte commands. This
; adds a delay because we wait for a timeout at the 9th bit, but it should be possible
; to test the value in r16 right away and jump out of the rx code to save time when
; a supported 8bit command is received.
;
; Also rewrote the transmission function. Now one bit occupies one bit (no more
; one byte per bit waste). Code tweaked for clean 1us/3us timing even across the byte boundary.
;

#include <avr/io.h>

#include "n64_isr.h"
#include "../compat.h"

.lcomm g_n64_buf, N64_BUF_SIZE

.text
.global INT0_vect
.global g_n64_buf
.global n64_rx_callback

#define IO_SREG	_SFR_IO_ADDR(SREG)
#define IO_PORTD _SFR_IO_ADDR(PORTD)
#define IO_DDRD _SFR_IO_ADDR(DDRD)
#define IO_PIND	_SFR_IO_ADDR(PIND)
#define IO_EIFR	_SFR_IO_ADDR(EIFR)

#if 0
#define DEBUG_PORTD_BIT 0x00	/* In PORTD */
#define DEBUG_ON 	sbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG
#define DEBUG_OFF 	cbi IO_PORTD, DEBUG_PORTD_BIT	; DEBUG
#else
#define DEBUG_ON
#define DEBUG_OFF
#endif

/* Note: Changing only this is not enough. The number of LSL and LSR
	operations in the code also needs an update. */
#define DATA_BIT	0x02	/* in PORTD */
#define DATA_MASK	0x04

#define ALIGN_DELAY				6
#define ALIGN_DELAY_NEXT_BYTE	5
#define STOP_ALIGN_DELAY	3

#define DATA_FALL_TIMEOUT	0x0A


; Useful
#define xl	r26
#define xh	r27
#define yl	r28
#define yh	r29
#define zl	r30
#define zh	r31



	; R16 : Byte buiding
	; R17 : Counter for delays
	; R20 : Holder for value from port

INT0_vect:
	push r1
	in r1, IO_SREG
	push r20
	push r16
	push r17
	push zl
	push zh
	push r23
	push xl
	push xh
	ldi xl, lo8(g_n64_buf)
	ldi xh, hi8(g_n64_buf)

next_byte:
	clr r16
	ldi zl, lo8(pm(bit_timed_out))
	ldi zh, hi8(pm(bit_timed_out))

	; All the above instruction provide the correct delay
	; for the first sample!

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04


	; Now stash the bit in r16 at the first position
	lsl r20 ; 0x08
	lsl r20 ; 0x10
	lsl r20 ; 0x20
	lsl r20 ; 0x40
	lsl r20 ; 0x80
	or r16, r20

waitHigh_BIT7:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_BIT7

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 6
	ldi r17, DATA_FALL_TIMEOUT
bit6_wait_fall:
	dec r17
	breq bit6_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit6_wait_fall
	rjmp bit6_align

bit6_fall_timeout:
	ijmp ; To Z

bit6_align:
	ldi r17, ALIGN_DELAY
bit6_align_lp:
	dec r17
		brne bit6_align_lp

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04
	lsl r20 ; 0x08
	lsl r20 ; 0x10
	lsl r20 ; 0x20
	lsl r20 ; 0x40
	or r16, r20

waitHigh_BIT6:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_BIT6


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 5
	ldi r17, DATA_FALL_TIMEOUT
bit5_wait_fall:
	dec r17
	breq bit5_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit5_wait_fall
	rjmp bit5_align

bit5_fall_timeout:
	ijmp ; To Z 

bit5_align:
	ldi r17, ALIGN_DELAY
bit5_align_lp:
	dec r17
		brne bit5_align_lp

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04
	lsl r20 ; 0x08
	lsl r20 ; 0x10
	lsl r20 ; 0x20
	or r16, r20

waitHigh_bit5:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_bit5


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 4
	ldi r17, DATA_FALL_TIMEOUT
bit4_wait_fall:
	dec r17
	breq bit4_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit4_wait_fall
	rjmp bit4_align

bit4_fall_timeout:
	ijmp ; To Z

bit4_align:
	ldi r17, ALIGN_DELAY
bit4_align_lp:
	dec r17
		brne bit4_align_lp

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04
	lsl r20 ; 0x08
	lsl r20 ; 0x10
	or r16, r20

waitHigh_bit4:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_bit4


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 3
	ldi r17, DATA_FALL_TIMEOUT
bit3_wait_fall:
	dec r17
	breq bit3_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit3_wait_fall
	rjmp bit3_align

bit3_fall_timeout:
	ijmp ; To Z

bit3_align:
	ldi r17, ALIGN_DELAY
bit3_align_lp:
	dec r17
		brne bit3_align_lp

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04
	lsl r20 ; 0x08
	or r16, r20

waitHigh_bit3:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_bit3


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 2
	ldi r17, DATA_FALL_TIMEOUT
bit2_wait_fall:
	dec r17
	breq bit2_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit2_wait_fall
	rjmp bit2_align

bit2_fall_timeout:
	ijmp ; To Z

bit2_align:
	ldi r17, ALIGN_DELAY
bit2_align_lp:
	dec r17
		brne bit2_align_lp

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04
	or r16, r20

waitHigh_bit2:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_bit2


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 1
	ldi r17, DATA_FALL_TIMEOUT
bit1_wait_fall:
	dec r17
	breq bit1_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit1_wait_fall
	rjmp bit1_align

bit1_fall_timeout:
	ijmp ; To Z

bit1_align:
	ldi r17, ALIGN_DELAY
bit1_align_lp:
	dec r17
		brne bit1_align_lp

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04
	lsr r20 ; 0x02
	or r16, r20

waitHigh_bit1:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_bit1


	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 0
	ldi r17, DATA_FALL_TIMEOUT
bit0_wait_fall:
	dec r17
	breq bit0_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit0_wait_fall
	rjmp bit0_align

bit0_fall_timeout:
	ijmp ; To Z

bit0_align:
	ldi r17, ALIGN_DELAY
bit0_align_lp:
	dec r17
		brne bit0_align_lp

	DEBUG_ON
	in r20, IO_PIND
	DEBUG_OFF
	andi r20, DATA_MASK ; 0x04
	lsr r20 ; 0x02
	lsr r20 ; 0x01
	or r16, r20

waitHigh_bit0:
	in r20, IO_PIND
	andi r20, DATA_MASK
		breq waitHigh_bit0

	st x+, r16

	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; BIT 7 (next byte)
	ldi r17, DATA_FALL_TIMEOUT
bit7_wait_fall:
	dec r17
	breq bit7_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne bit7_wait_fall
	rjmp bit7_align

bit7_fall_timeout:
	ijmp ; To Z

bit7_align:
	ldi r17, ALIGN_DELAY_NEXT_BYTE
bit7_align_lp:
	dec r17
		brne bit7_align_lp

	rjmp next_byte


bit_timed_out:
	; Each _complete_ byte we receive is stored in x with post increment.
	; X is initially pointed to g_n64_buf.
	; Calculate how many bytes we received using the current value of X.
	ldi r16, lo8(g_n64_buf)
	sub xl, r16 ; Bug/limitation: 8-bit arithmetic. Only up to 255 bytes.
	breq interrupt_done_poll ; Nothing received / incomplete single byte. Abort.

	;;;;;; CALLING A C FUNCTION
	push r0
	push r1
	push r18
	push r19
	push r20
	push r21
	push r22
	push r23
	push r24
	push r25
	push r26
	push r27
	push r31

	mov r24, xl	; Argument 1 (len)
	clr r1

	ldi zl, lo8(pm(n64_rx_callback))
	ldi zh, hi8(pm(n64_rx_callback))
	icall
;	clr r24

	mov r16, r24 ; Return value

	pop r31
	pop r27
	pop r26
	pop r25
	pop r24
	pop r23
	pop r22
	pop r21
	pop r20
	pop r19
	pop r18
	pop r1
	pop r0

	; r16 is the length to transmit
	rcall sendMisc

	rjmp interrupt_done_poll


interrupt_done_poll:

; Clear the interrupt flag
#ifndef AT168_COMPATIBLE
	ldi r20, 0x40 ; INTF0
	sts GIFR, r20
#else
	sbi IO_EIFR, 0 ; Clear INTF0
#endif

;;;;;; INTERRUPT RETURN CODE
	pop xh
	pop xl
	pop r23
	pop zh
	pop zl
	pop r17
	pop r16
	pop r20
	out IO_SREG, r1
	pop r1
	reti

	/* Arg (length) in r16 */
sendMisc:
	ldi zl, lo8(g_n64_buf)
	ldi zh, hi8(g_n64_buf)
	mov r23, r16
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
	* Z must point to first byte of data.
	*
	* A stop bit is added at thy end of the packet.
	*
	* Note: registers available are r16, r17, r20 and r23
	************************************************/
sendGCData:
	push r19

	tst r23
	breq done_send

send_next_byte:
	; Check if this is the last byte.
	tst r23
	breq send_stop
	dec r23
	ld r16, z+
	ldi r17, 0x80 ; mask

send_next_bit:
	mov r19, r16
	and r19, r17
	brne send1
	nop

send0:
	sbi IO_DDRD, DATA_BIT   ; Pull bus to 0

	ldi r20, 15
lp_send0_3us:
	dec r20
	brne lp_send0_3us
	nop

	cbi IO_DDRD, DATA_BIT	; Release bus to 1


	lsr r17
	breq send_next_byte
	nop
	nop
	nop
	nop
	nop
	nop
	rjmp send_next_bit

send1:
	sbi IO_DDRD, DATA_BIT	; Pull bus to 0

	ldi r20, 4
lp_send1_1us:
	dec r20
	brne lp_send1_1us
	nop
	nop

	cbi IO_DDRD, DATA_BIT	; Release bus to 1

	ldi r20, 10
lp_send1_3us:
	dec r20
	brne lp_send1_3us
	nop
	nop

	lsr r17
	breq send_next_byte
	nop
	nop
	nop
	nop
	nop
	nop
	rjmp send_next_bit

send_stop:
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	; STOP BIT
	sbi IO_DDRD, DATA_BIT ; Pull low for stop bit
	ldi r20, 4
stbdly0:
	dec r20
	brne stbdly0
	nop
	cbi IO_DDRD, DATA_BIT ;	Release

done_send:
	pop r19
	ret


