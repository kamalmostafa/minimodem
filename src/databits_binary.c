/*
 * databits_binary.c
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

#include "databits.h"

/*
 * Rawbits N-bit binary data decoder/encoder
 */

#include <assert.h>

// returns nbytes decoded
unsigned int
databits_decode_binary( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits )
{
    if ( ! dataout_p )	// databits processor reset: noop
	return 0;
    assert( dataout_size >= n_databits + 1 );
    int j;
    for ( j=0; j<n_databits; j++ )
	dataout_p[j] = (bits>>j & 1) + '0';
    dataout_p[j] = '\n';
    return n_databits + 1;
}

