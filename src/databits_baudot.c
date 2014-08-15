/*
 * databits_baudot.c
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
 * Baudot 5-bit data databits decoder/encoder
 */

#include "baudot.h"

/* returns nbytes decoded */
unsigned int
databits_decode_baudot( char *dataout_p, unsigned int dataout_size,
	unsigned long long bits, unsigned int n_databits )
{
    if ( ! dataout_p ) {	// databits processor reset: reset Baudot state
	    baudot_reset();
	    return 0;
    }
    bits &= 0x1F;
    return baudot_decode(dataout_p, bits);
}

