/*
 * baudot.c
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
 */


#include <assert.h>


static char
baudot_table[32][3] = {
    // letter, U.S. figs, CCITT No.2 figs (Europe)
    { '*', '*', '*' },	// NUL
    { 'E', '3', '3' },
    { 0xA, 0xA, 0xA },	// LF
    { 'A', '-', '-' },
    { ' ', ' ', ' ' },	// SPACE
    { 'S', '*', '\'' },	// BELL or apostrophe
    { 'I', '8', '8' },
    { 'U', '7', '7' },

    { 0xD, 0xD, 0xD },	// CR
    { 'D', '$', '*' },	// '$' or ENQ
    { 'R', '4', '4' },
    { 'J', '\'', '*' },	// apostrophe or BELL
    { 'N', ',', ',' },
    { 'F', '!', '!' },
    { 'C', ':', ':' },
    { 'K', '(', '(' },

    { 'T', '5', '5' },
    { 'Z', '"', '+' },
    { 'L', ')', ')' },
    { 'W', '2', '2' },
    { 'H', '#', '*' },	// '#' or British pounds symbol	// FIXME
    { 'Y', '6', '6' },
    { 'P', '0', '0' },
    { 'Q', '1', '1' },

    { 'O', '9', '9' },
    { 'B', '?', '?' },
    { 'G', '&', '&' },
    { '*', '*', '*' },	// FIGS
    { 'M', '.', '.' },
    { 'X', '/', '/' },
    { 'V', ';', '=' },
    { '*', '*', '*' },	// LTRS
};

#define BAUDOT_LTRS	0x1F
#define BAUDOT_FIGS	0x1B
#define BAUDOT_SPACE	0x04


static int baudot_charset = 0;		// FIXME


void
baudot_reset()
{
    baudot_charset = 0;
}


/*
 * returns nonzero if *char_outp was stuffed with an output character
 */
int
baudot( unsigned char databits, char *char_outp )
{
    /* Baudot (RTTY) */
    assert( (databits & ~0x1F) == 0 );

    int stuff_char = 1;
    if ( databits == BAUDOT_FIGS ) {
	baudot_charset = 1;
	stuff_char = 0;
    } else if ( databits == BAUDOT_LTRS ) {
	baudot_charset = 0;
	stuff_char = 0;
    } else if ( databits == BAUDOT_SPACE ) {
	baudot_charset = 0;
    }
    if ( stuff_char )
	*char_outp = baudot_table[databits][baudot_charset];
    return stuff_char;
}

