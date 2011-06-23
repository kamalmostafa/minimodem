/*
 * baudot.h
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
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


void
baudot_reset();

/*
 * Returns 1 if *char_outp was stuffed with an output character
 * or 0 if no output character was stuffed (in other words, returns
 * the count of characters decoded and stuffed).
 */
int
baudot_decode( char *char_outp, unsigned char databits );

/*
 * Returns the number of 5-bit datawords stuffed into *databits_outp (1 or 2)
 */
int
baudot_encode( unsigned int *databits_outp, char char_out );
