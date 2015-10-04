#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include "../compat.h"
#include "n64_isr.h"

#define STATE_IDLE		0
#define STATE_ERASEALL	1
#define STATE_WRITEPAGE	2
#define STATE_START_APP	3

static volatile unsigned char state;
static volatile unsigned char pagebuf[PAGE_SIZE];
static volatile unsigned short pageaddr;

static void boot_eraseall(void)
{
	unsigned char sreg;
	int i;

	sreg = SREG;
	cli();

	for (i=0; i<BOOTSTART / PAGE_SIZE; i++) {
		eeprom_busy_wait();
		boot_spm_busy_wait();
		boot_page_erase(i*PAGE_SIZE);
		boot_spm_busy_wait();
	}

	SREG = sreg;
}

static void boot_writePage(void)
{
	unsigned char sreg;
	int i;

	sreg = SREG;
	cli();

	eeprom_busy_wait();
	boot_spm_busy_wait();

	for (i=0; i<PAGE_SIZE; i+= 2) {
		unsigned short w = pagebuf[i] | (pagebuf[i+1]<<8);
		boot_page_fill(pageaddr + i, w);
	}
	boot_page_write(pageaddr);
	boot_spm_busy_wait();
	boot_rww_enable();

	SREG = sreg;
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
				if (state != STATE_IDLE) {
					g_n64_buf[0] = 0x01; // Busy
				} else {
					state = STATE_ERASEALL;
					g_n64_buf[0] = 0x00; // Ack
				}
				return 1;

			case 0xf1: // Bootloader read flash (32-byte block)
				{
					// Arguments
					// [2][3] : Block id
					unsigned short block_id = (g_n64_buf[2]<<8) | g_n64_buf[3];
					memcpy_PF((void*)g_n64_buf, block_id * 32L, 32);
					return 32;
				}

			case 0xf2: // Upload block
				{
					// Arguments
					// [2][3] : Block id
					// + 32 bytes block
					unsigned long addr;
					unsigned short block_id = (g_n64_buf[2]<<8) | g_n64_buf[3];
					int page_part;

					if (state != STATE_IDLE) {
						g_n64_buf[0] = 0x01; // Busy
						return 1;
					}

					addr = g_n64_buf[3] * 32L;
					page_part = block_id % (PAGE_SIZE/32);

					memcpy((void*)pagebuf+page_part*32, (void*)g_n64_buf+4, 32);
					pageaddr = addr / PAGE_SIZE;
					pageaddr *= PAGE_SIZE;

					if (page_part == (PAGE_SIZE/32)-1) {
						// When receiving the last block, trigger the write. This
						// takes time so it's done in the main loop.
						g_n64_buf[0] = 0x00; // Ack
						g_n64_buf[1] = 0x01; // Need to poll for busy state
						g_n64_buf[2] = pageaddr >> 8;
						g_n64_buf[3] = pageaddr;
						state = STATE_WRITEPAGE;
						return 4;
					}

					g_n64_buf[0] = 0x00; // Ack
					g_n64_buf[1] = 0x00; // No need to poll for busy state.
					addr /= PAGE_SIZE;
					addr *= PAGE_SIZE;
					g_n64_buf[2] = pageaddr >> 8;
					g_n64_buf[3] = pageaddr;
					return 4;
				}
				break;

			case 0xf9: // Get busy
				g_n64_buf[0] = state != STATE_IDLE;
				return 1;

			case 0xfe: // Start application
				if (state != STATE_IDLE) {
					g_n64_buf[0] = 0x01; // Busy / NACK
					return 1;
				}
				g_n64_buf[0] = 0x00; // Will do
				state = STATE_START_APP;
				return 1;
		}
	}

	return 0;
}

static void startApp(void)
{
	cli();

#ifdef AT168_COMPATIBLE
	MCUCR = (1<<IVCE);
	MCUCR = 0;
#else
	GICR = (1<<IVCE);
	GICR = 0;
#endif

	asm volatile(	"ldi r30, 0x00\n"
					"ldi r31, 0x00\n"
					"ijmp\n");
}

int main(void)
{
	// PORTD2 is the N64 data signal.
	// Pullup resistors on all other signals.
	//
	DDRD=0;
	PORTD = ~0x04;

	// portD 0 is debug bit
	DDRD |= 1;

	// temporary
	DDRC=0;
	PORTC=0xff;

#ifdef WITH_LED
	PORTB |= (1<<PB1);
	DDRB |= 1<<PB1;
#endif

#ifdef AT168_COMPATIBLE
	MCUCR = (1<<IVCE);
	MCUCR = (1<<IVSEL);

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

	while(1)
	{
		switch (state)
		{
			default:
				state = STATE_IDLE;
			case STATE_IDLE:
				break;

			case STATE_ERASEALL:
				boot_eraseall();
				state = STATE_IDLE;
				break;

			case STATE_WRITEPAGE:
				boot_writePage();
				state = STATE_IDLE;
				break;

			case STATE_START_APP:
				// Todo : add app validation mechanism
				startApp(); // If all is good, won't return.
				state = STATE_IDLE;
				break;
		}
	}

	return 0;
}
