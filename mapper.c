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
#include "mapper.h"

void mapper_change_mapping_entry(struct mapping_entry *map, int input, int output)
{
	while (map->input != -1) {
		if (map->input == input) {
			map->output = output;
			break;
		}
		map++;
	}
}

char mapper_getAxis(struct mapping_controller_unit *input)
{
	switch (input->type)
	{
		default:
		case TYPE_NONE:
			break;
		case TYPE_AXIS:
			return input->value;
			break;
		case TYPE_BTN:
			break;
	}
	return 0;
}

char mapper_getButton(struct mapping_controller_unit *input)
{
	switch (input->type)
	{
		default:
		case TYPE_AXIS:
		case TYPE_NONE:
			return 0;
		case TYPE_BTN:
			return input->value;
		case TYPE_AXIS_TO_BTN:
			if (input->thres < 0) {
				if (input->value <= input->thres)
					return 1;
			} else {
				if (input->value >= input->thres)
					return 1;
			}
			break;
		case TYPE_NEG_SLD_TO_BTN:
			if (input->value <= input->thres)
				return 1;
			break;
	}
	return 0;
}

static void allDefault(struct mapping_controller_unit *units)
{
	while(units->type != TYPE_NONE)
	{
		units->value = units->def;
		units++;
	}
}

void mapper_copy(struct mapping_entry *map, struct mapping_controller_unit *input, struct mapping_controller_unit *output)
{
	int idx_in, idx_out;

	// Prepare output from default state	
	allDefault(output);

	while (map->input != -1)
	{
		idx_in = map->input;
		idx_out = map->output;
	
		switch(output[idx_out].type)
		{
			default:
			case TYPE_NONE:
				break;

			case TYPE_BTN:
				// "OR" buttons together.
				output[idx_out].value |= mapper_getButton(&input[idx_in]);
				break;

			case TYPE_AXIS:
				if (input[idx_in].type == TYPE_AXIS) {
					output[idx_out].value = input[idx_in].value;
				}
				break;
				
			case TYPE_AXIS_TO_BTN:
				if (mapper_getButton(&input[idx_in])) {
					output[idx_out].value = output[idx_out].thres;
				}
				break;
		}

		map++;
	}
}



