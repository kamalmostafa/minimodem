/*
 * databits.h
 *
 * Copyright (C) 2012 Kamal Mostafa <kamal@whence.com>
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

// Reverses the ordering of the bits on an integer
static inline unsigned long long
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
static inline unsigned long long
bit_window(unsigned long long value,
	unsigned int offset,
	unsigned int bits)
{
	unsigned long long mask = (1ULL << bits) - 1;
	value = (value >> offset) & mask;
	return value;
}

typedef int (databits_encoder)(
	unsigned int *databits_outp, char char_out );

typedef unsigned int (databits_decoder)(
	char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits );


int
databits_encode_ascii8( unsigned int *databits_outp, char char_out );

unsigned int
databits_decode_ascii8( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits );


#include "baudot.h"
#define databits_encode_baudot baudot_encode // from baudot.h
//int
//databits_encode_baudot( unsigned int *databits_outp, char char_out );

unsigned int
databits_decode_baudot( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits );


int
databits_encode_binary( unsigned int *databits_outp, char char_out );

unsigned int
databits_decode_binary( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits );


unsigned int
databits_decode_callerid( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits );

unsigned int
databits_decode_uic_ground( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits );

unsigned int
databits_decode_uic_train( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits );
