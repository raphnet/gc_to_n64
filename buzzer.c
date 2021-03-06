/*  GC to N64 : Gamecube controller to N64 adapter firmware
    Copyright (C) 2011-2017  Raphael Assenat <raph@raphnet.net>

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
#include <util/delay.h>
#include "buzzer.h"

static char invert_mode = 0;

void buzzer_led_invert(char invert)
{
	invert_mode = invert;
}

void buzzer_init(void)
{
	// Active-high output for buzzer on PB1 (OC1A)
	PORTB &= ~(1<<PB1);
	DDRB |= 1<<PB1;
}

void buzz_error(void)
{
#ifdef VISUAL_BUZZER
	blips(10);
	buzzer_led_invert(0);
	buzz(0);
#else
	// did did did dah----
	blips(3);

	buzz(1);
	_delay_ms(700);
	buzz(0);
#endif
}

#ifdef VISUAL_BUZZER
#define ON_DELAY	80
#define OFF_DELAY	80
#else
#define ON_DELAY	120
#define OFF_DELAY	50
#endif

void blips(int n)
{
	do {
		buzz(1);
		_delay_ms(ON_DELAY);
		buzz(0);
		_delay_ms(OFF_DELAY);
	} while (--n);
}

void buzz(int on)
{
#ifndef VISUAL_BUZZER
	unsigned short intensity = 0x1F;

	if (on==0) {
		TCCR1A = 0;
		return;
	}

	//TCCR1A = 1<<COM1A1 | 1<<WGM11;
	TCCR1A = 1<<COM1A1 | 1<<WGM10;
	TCCR1B = 1<<WGM12 | 1<<CS11; // CTC, /1024 prescaler

	OCR1AH = intensity >> 8;
	OCR1AL = intensity & 0xff;
#else
	if (invert_mode) {
		on = !on;
	}
	if (on) {
		PORTB |= 1<<PB1;
	} else {
		PORTB &= ~(1<<PB1);
	}
#endif
}
