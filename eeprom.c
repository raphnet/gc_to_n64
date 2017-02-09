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
#include <avr/eeprom.h>
#include <string.h>
#include "eeprom.h"

struct eeprom_data_struct g_eeprom_data;

void eeprom_commit(void)
{
	eeprom_write_block(&g_eeprom_data, (void*)0x00, sizeof(struct eeprom_data_struct));
}

static char magic[EEPROM_MAGIC_SIZE] = { 'G','C','2','N','6','4','v','7' };

void eeprom_writeDefaults(void)
{
	memcpy(g_eeprom_data.magic, magic, EEPROM_MAGIC_SIZE);

	g_eeprom_data.defmap = 0;
	g_eeprom_data.deadzone_enabled = 0;
	g_eeprom_data.old_v1_5_conversion = 0;

	// This fill lets default mappings be empty (-1,-1 being the terminator)
	memset(g_eeprom_data.appdata, 0xff, EEPROM_APPDATA_SIZE);

	eeprom_commit();
}

int eeprom_init(void)
{
	eeprom_read_block(&g_eeprom_data, (void*)0x00, sizeof(struct eeprom_data_struct));

	/* Check for magic number */
	if (memcmp(g_eeprom_data.magic, magic, EEPROM_MAGIC_SIZE)) {
		eeprom_writeDefaults();
		return 1;
	}

	return 0;
}

void toggle_old_v1_5_conversion(void)
{
	g_eeprom_data.old_v1_5_conversion = !g_eeprom_data.old_v1_5_conversion;
	eeprom_commit();
}

void toggleDeadzone(void)
{
	g_eeprom_data.deadzone_enabled = !g_eeprom_data.deadzone_enabled;
	eeprom_commit();
}

void setDefaultMapping(int id)
{
	g_eeprom_data.defmap = id;
	eeprom_commit();
}

