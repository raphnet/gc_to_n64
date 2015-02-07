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
#include <avr/io.h>
#include <avr/interrupt.h>

static unsigned char t0_count;

ISR(TIMER0_OVF_vect)
{
	if (t0_count<60)
		t0_count++;
}

void timerIntOff()
{
	TIMSK &= ~(1<<TOIE0);
}

void timerIntOn()
{
	TIMSK |= 1<<TOIE0;
}

void timerStart()
{
	// 1024 prescaler
	TCCR0 = 1<<CS02 | 1<<CS00;
	TCNT0 = 0;
	TIMSK |= 1<<TOIE0;

	// 16000000 / 1024 / 256 = approx 61hz
	t0_count = 0;
}

char timerIsOver(void)
{
	if (t0_count >= 60) {
		t0_count = 0;
		return 1;
	}

	return 0;
}
