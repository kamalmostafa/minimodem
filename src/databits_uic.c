/*
 * databits_uic.c
 *
 * Copyright (C) 2014 Marcos Vives Del Sol <socram8888@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "databits.h"
#include "uic_codes.h"

/*
 * UIC-751-3 Ground-train decoder
 */

unsigned int
databits_decode_uic(char *output,
	unsigned long long input,
	unsigned int type)
{
	int written;

	if (!output) {
		return 0;
	}

	unsigned int code = (unsigned int) bit_reverse(bit_window(input, 24, 8), 8);
	written = sprintf(output, "Train ID: %X%X%X%X%X%X - Message: %02X (%s)\n",
			(unsigned int) bit_window(input, 0, 4),
			(unsigned int) bit_window(input, 4, 4),
			(unsigned int) bit_window(input, 8, 4),
			(unsigned int) bit_window(input, 12, 4),
			(unsigned int) bit_window(input, 16, 4),
			(unsigned int) bit_window(input, 20, 4),
			code,
			uic_message_meaning(code, type)
	);

	return written;
}

unsigned int
databits_decode_uic_ground(char *output,
	unsigned int outputSize,
	unsigned long long input,
	unsigned int inputSize)
{
	return databits_decode_uic(output,
		input,
		UIC_TYPE_GROUNDTRAIN);
}

unsigned int
databits_decode_uic_train(char *output,
	unsigned int outputSize,
	unsigned long long input,
	unsigned int inputSize)
{
	return databits_decode_uic(output,
		input,
		UIC_TYPE_TRAINGROUND);
}
