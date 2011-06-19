/*
 * baudot.h
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
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
