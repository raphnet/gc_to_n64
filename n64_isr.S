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
; 2015-09-29: Rewrote (unrolled) reception code. A complete byte, ready for testing
; for specific values is left in r16. Thanks to this, the firmware now answers requests
; as fast as an official N64 controller.
;
; Also, corrected transmission timing to match official N64 controllers (4us cycles)
;

#include <avr/io.h>

#include "n64_isr.h"
#include "compat.h"

#define N64_RX_BUF_SIZE	33

.lcomm n64_rx_buf, N64_RX_BUF_SIZE
.lcomm n64_tx_buf0, N64_TX_BUF_SIZE	; reply is 32 bits + 1 stop bit
.lcomm n64_tx_buf1, N64_TX_BUF_SIZE	;
.lcomm n64_tx_id_reply 24;
.lcomm n64_got_command 1;
.lcomm n64_use_buf1 1;

.text
.global INT0_vect
.global n64_rx_buf
.global n64_tx_buf0
.global n64_tx_buf1
.global n64_tx_id_reply
.global n64_got_command
.global n64_use_buf1

;.global sendGCData

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

#define ALIGN_DELAY			6
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

	clr r16
	ldi zl, lo8(pm(interrupt_done_poll))
	ldi zh, hi8(pm(interrupt_done_poll))

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




	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; STOP BIT
	ldi r17, DATA_FALL_TIMEOUT
stop1_wait_fall:
	dec r17
	breq stop1_fall_timeout
	in r20, IO_PIND
	andi r20, DATA_MASK
	brne stop1_wait_fall
	rjmp stop1_align

stop1_fall_timeout:
	ijmp ; To Z

	; Waste the right amount of cycles to make sure we
	; answer as fast as real N64 controller, and not faster.
stop1_align:
	ldi r17, STOP_ALIGN_DELAY
stop1_align_lp:
	dec r17
		brne stop1_align_lp



got_8bit_command:
	; Commands:
	; 0x00 : Get capability
	; 0x01 : Get button status
	; 0xff : Reset

	; Unsupported commands 0x02 0x03 (multi-byte)

	cpi r16, 0x01	; GET STATUS
	brne check_other_commands
	rcall sendStatus
	rjmp interrupt_done_poll

check_other_commands:
	cpi r16, 0x00	; GET CAPABILITIES
	breq cap

	cpi r16, 0xff	; RESET (Same answer as get caps 0x00)
	breq cap

	; ignore unknwn
	rjmp interrupt_done_poll

cap:
	rcall sendCapabilityWord
	rjmp interrupt_done_poll


interrupt_done_poll:

; Notify the main loop about the poll
	ldi r20, 1
	ldi	zl, lo8(n64_got_command)
	ldi zh, hi8(n64_got_command)
	st z, r20

; Clear the interrupt flag
#ifndef AT168_COMPATIBLE
	ldi r20, 0x40 ; INTF0
	sts GIFR, r20
#else
	sbi IO_EIFR, 0 ; Clear INTF0
#endif

;;;;;; INTERRUPT RETURN CODE
	pop r23
	pop zh
	pop zl
	pop r17
	pop r16
	pop r20
	out IO_SREG, r1
	pop r1
	reti

	/******************************************
	Point Z to the 'capabilities' contant
	and sent it.
	******************************************/
sendCapabilityWord:
	ldi zl, lo8(n64_tx_id_reply)
	ldi zh, hi8(n64_tx_id_reply)
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
	sbi IO_DDRD, DATA_BIT	; Pull bus to 0

	ldi r20, 15
lp_send0_3us:
	dec r20
	brne lp_send0_3us
	nop

	cbi IO_DDRD, DATA_BIT	; Release bus to 1

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
	sbi IO_DDRD, DATA_BIT ; Pull low for stop bit
	ldi r20, 4
stbdly0:
	dec r20
	brne stbdly0
	nop
	cbi IO_DDRD, DATA_BIT ;	Release

	pop r23
	pop r20
	pop r19
	ret

send1:
	sbi IO_DDRD, DATA_BIT	; Pull bus to 0

	ldi r20, 4
lp_send1_1us:
	dec r20
	brne lp_send1_1us
	nop
	nop

	cbi IO_DDRD, DATA_BIT	; Release bus to 1

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
	sbi IO_DDRD, DATA_BIT ; Pull low for stop bit
	ldi r20,4
stbdly1:
	dec r20
	brne stbdly1
	nop
	cbi IO_DDRD, DATA_BIT	; Release

waithigh:
	ldi r20, 0xff
waithigh_lp:
	dec r20
	breq waithigh_done

	sbic IO_PIND, DATA_BIT
	rjmp waithigh_done
	nop
	nop
	nop
	nop
	rjmp waithigh_lp

waithigh_done:

	pop r23
	pop r20
	pop r19

	ret

