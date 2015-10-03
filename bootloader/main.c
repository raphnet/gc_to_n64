#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include "../compat.h"
#include "n64_isr.h"


static void boot_eraseall(void)
{
	int i;

	// Pages are 64 bytes
	for (i=0; i<BOOTSTART / PAGE_SIZE; i++) {
		eeprom_busy_wait();
		boot_page_erase(i);
		boot_spm_busy_wait();
	}
}

unsigned char n64_rx_callback(unsigned char len)
{
	if (len == 1)
	{
		switch (g_n64_buf[0])
		{
			case 0xff: // Reset
			case 0x00: // Get caps
				g_n64_buf[0] = 0x05;
				g_n64_buf[1] = 0x00;
				g_n64_buf[2] = 0x02;
				return 3;

			case 0x01: // Get status
				g_n64_buf[0] = 0;
				g_n64_buf[1] = 0;
				g_n64_buf[2] = 0;
				g_n64_buf[3] = 0;
				return 4;
		}
		return 0;
	}

	// All raphnet specific commands start by 'R'
	if ((len >= 2) && (g_n64_buf[0]=='R')) {
		switch (g_n64_buf[1]) {
			case 0x00: // echo
				return len;

			case 0x01: // Get info
				memset((void*)g_n64_buf, 0, sizeof(g_n64_buf));
				g_n64_buf[0] = 1; // IsBootloader? Yes.
				g_n64_buf[1] = PAGE_SIZE;
				g_n64_buf[2] = ((BOOTSTART) & 0xff00) >> 8;
				g_n64_buf[3] = (BOOTSTART) & 0xFF;
				strcpy((void*)(g_n64_buf+10), VERSION_STR);
				return 10 + strlen(VERSION_STR) + 1;

			case 0xf0: // Bootloader erase app
				boot_eraseall();
				g_n64_buf[2] = 0x00;
				return 3;

			case 0xf1: // Bootloader read flash (32-byte block)
				// Arguments
				// [2][3] : Block id
				memcpy_PF((void*)g_n64_buf, g_n64_buf[3] * 32L, 32);
				return 32;


		}
	}

	return 0;
}

int main(void)
{
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
	// Move interrupt vector table to bootloader
asm volatile(
	"push r16		\n"
	"ldi r16, 0x01	\n"
	"out 0x3B, r16	\n"
	"ldi r16, 0x02	\n"
	"out 0x3B, r16	\n"
	"pop r16\n"
	::);

	// configure external interrupt 1 to trigger on falling edge.
	MCUCR |= 1<<ISC11;
	MCUCR &= ~(1<<ISC10);

	GICR |= (1<<INT0);
#endif

	sei();

	while(1) {
	}

	return 0;
}
