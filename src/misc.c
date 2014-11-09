/*
 * misc.c
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

#include "misc.h"

// Reverses the ordering of the bits on an integer
unsigned long long
bit_reverse(unsigned long long value,
	unsigned int bits)
{
	unsigned int out = 0;

	while (bits--) {
		out = (out << 1) | (value & 1);
		value >>= 1;
	}

	return out;
}

// Gets "bits" bits from "value" starting "offset" bits from the start
unsigned long long
bit_window(unsigned long long value,
	unsigned int offset,
	unsigned int bits)
{
	unsigned long long mask = (1ULL << bits) - 1;
	value = (value >> offset) & mask;
	return value;
}
