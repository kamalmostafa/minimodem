/*
 * baudot.c
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


#include <assert.h>
#include <ctype.h>
#include <stdio.h>

//#define BAUDOT_DEBUG
#ifdef BAUDOT_DEBUG
# define debug_log(format, args...)  fprintf(stderr, format, ## args)
#else
# define debug_log(format, args...)
#endif


static char
baudot_decode_table[32][3] = {
    // letter, U.S. figs, CCITT No.2 figs (Europe)
    { '_', '^', '^' },	// NUL (underscore and caret marks for debugging)
    { 'E', '3', '3' },
    { 0xA, 0xA, 0xA },	// LF
    { 'A', '-', '-' },
    { ' ', ' ', ' ' },	// SPACE
    { 'S', 0x7, '\'' },	// BELL or apostrophe
    { 'I', '8', '8' },
    { 'U', '7', '7' },

    { 0xD, 0xD, 0xD },	// CR
    { 'D', '$', '^' },	// '$' or ENQ
    { 'R', '4', '4' },
    { 'J', '\'', 0x7 },	// apostrophe or BELL
    { 'N', ',', ',' },
    { 'F', '!', '!' },
    { 'C', ':', ':' },
    { 'K', '(', '(' },

    { 'T', '5', '5' },
    { 'Z', '"', '+' },
    { 'L', ')', ')' },
    { 'W', '2', '2' },
    { 'H', '#', '%' },	// '#' or British pounds symbol	// FIXME
    { 'Y', '6', '6' },
    { 'P', '0', '0' },
    { 'Q', '1', '1' },

    { 'O', '9', '9' },
    { 'B', '?', '?' },
    { 'G', '&', '&' },
    { '%', '%', '%' },	// FIGS (symbol % for debug; won't be printed)
    { 'M', '.', '.' },
    { 'X', '/', '/' },
    { 'V', ';', '=' },
    { '%', '%', '%' },	// LTRS (symbol % for debug; won't be printed)
};

static char
baudot_encode_table[0x60][2] = {
    // index: ascii char; values: bits, ltrs_or_figs_or_neither_or_both

  /* 0x00 */
    /* NUL */	{ 0x00, 3 },	// NUL
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* BEL */	{ 0x05, 2 },	// BELL (or CCITT2 apostrophe)
    /* BS */	{ 0, 0 },	// non-encodable (FIXME???)
    /* xxx */	{ 0, 0 },	// non-encodable
    /* LF */	{ 0x02, 3 },	// LF
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* 0xD */	{ 0x08, 3 },	// CR
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable

  /* 0x10 */
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable
    /* xxx */	{ 0, 0 },	// non-encodable

  /* 0x20 */
    /*   */	{ 0x04, 3 },	// SPACE
    /* ! */	{ 0x0d, 2 },	//
    /* " */	{ 0x11, 2 },	//
    /* # */	{ 0x14, 2 },	// '#' (or CCITT2 British pounds symbol)
    /* $ */	{ 0x09, 2 },	// '$' (or CCITT2 ENQ)
    /* % */	{ 0, 0 },	// non-encodable
    /* & */	{ 0x1a, 2 },	//
    /* ' */	{ 0x0b, 2 },	// apostrophe (or CCITT2 BELL)
    /* ( */	{ 0x0f, 2 },	//
    /* ) */	{ 0x12, 2 },	//
    /* * */	{ 0, 0 },	// non-encodable
    /* + */	{ 0x12, 2 },	//
    /* , */	{ 0x0c, 2 },	//
    /* - */	{ 0x03, 2 },	//
    /* . */	{ 0x1c, 2 },	//
    /* / */	{ 0x1d, 2 },	//

  /* 0x30 */
    /* 0 */	{ 0x16, 2 },	//
    /* 1 */	{ 0x17, 2 },	//
    /* 2 */	{ 0x13, 2 },	//
    /* 3 */	{ 0x01, 2 },	//
    /* 4 */	{ 0x0a, 2 },	//
    /* 5 */	{ 0x10, 2 },	//
    /* 6 */	{ 0x15, 2 },	//
    /* 7 */	{ 0x07, 2 },	//
    /* 8 */	{ 0x06, 2 },	//
    /* 9 */	{ 0x18, 2 },	//
    /* : */	{ 0x0e, 2 },	//
    /* ; */	{ 0x1e, 2 },	//
    /* < */	{ 0, 0 },	// non-encodable
    /* = */	{ 0, 0 },	// non-encodable
    /* > */	{ 0, 0 },	// non-encodable
    /* ? */	{ 0x19, 2 },	//

  /* 0x40 */
    /* @ */	{ 0, 0 },	// non-encodable
    /* A */	{ 0x03, 1 },	//
    /* B */	{ 0x19, 1 },	//
    /* C */	{ 0x0e, 1 },	//
    /* D */	{ 0x09, 1 },	//
    /* E */	{ 0x01, 1 },	//
    /* F */	{ 0x0d, 1 },	//
    /* G */	{ 0x1a, 1 },	//
    /* H */	{ 0x14, 1 },	//
    /* I */	{ 0x06, 1 },	//
    /* J */	{ 0x0b, 1 },	//
    /* K */	{ 0x0f, 1 },	//
    /* L */	{ 0x12, 1 },	//
    /* M */	{ 0x1c, 1 },	//
    /* N */	{ 0x0c, 1 },	//
    /* O */	{ 0x18, 1 },	//

  /* 0x50 */
    /* P */	{ 0x16, 1 },	//
    /* Q */	{ 0x17, 1 },	//
    /* R */	{ 0x0a, 1 },	//
    /* S */	{ 0x05, 1 },	//
    /* T */	{ 0x10, 1 },	//
    /* U */	{ 0x07, 1 },	//
    /* V */	{ 0x1e, 1 },	//
    /* W */	{ 0x13, 1 },	//
    /* X */	{ 0x1d, 1 },	//
    /* Y */	{ 0x15, 1 },	//
    /* Z */	{ 0x11, 1 },	//
    /* [ */	{ 0, 0 },	// non-encodable
    /* \\ */	{ 0, 0 },	// non-encodable
    /* ] */	{ 0, 0 },	// non-encodable
    /* ^ */	{ 0, 0 },	// non-encodable
    /* _ */	{ 0, 0 },	// non-encodable

};

#define BAUDOT_LTRS	0x1F
#define BAUDOT_FIGS	0x1B
#define BAUDOT_SPACE	0x04


/*
 * 0 unknown state
 * 1 LTRS state
 * 2 FIGS state
 */
static unsigned int baudot_charset = 0;		// FIXME


void
baudot_reset()
{
    baudot_charset = 1;
}


/*
 * Returns 1 if *char_outp was stuffed with an output character
 * or 0 if no output character was stuffed (in other words, returns
 * the count of characters decoded and stuffed).
 */
int
baudot_decode( char *char_outp, unsigned char databits )
{
    /* Baudot (RTTY) */
    assert( (databits & ~0x1F) == 0 );

    int stuff_char = 1;
    if ( databits == BAUDOT_FIGS ) {
	baudot_charset = 2;
	stuff_char = 0;
    } else if ( databits == BAUDOT_LTRS ) {
	baudot_charset = 1;
	stuff_char = 0;
    } else if ( databits == BAUDOT_SPACE ) {	/* RX un-shift on space */
	baudot_charset = 1;
    }
    if ( stuff_char ) {
	int t;
	if ( baudot_charset == 1 )
	    t = 0;
	else
	    t = 1;	// U.S. figs
	    // t = 2;	// CCITT figs
	*char_outp = baudot_decode_table[databits][t];
    }
    return stuff_char;
}


static void
baudot_skip_warning( char char_out )
{
    unsigned char byte = char_out;
    fprintf(stderr, "W: baudot skipping non-encodable character '%c' 0x%02x\n",
		char_out, byte);
}

/*
 * Returns the number of 5-bit data words stuffed into *databits_outp (1 or 2)
 */
int
baudot_encode( unsigned int *databits_outp, char char_out )
{

    char_out = toupper(char_out);
    if( char_out >= 0x60 || char_out < 0 ) {
	baudot_skip_warning(char_out);
	return 0;
    }

    unsigned char ind = char_out;

    int n = 0;

    unsigned char charset_mask = baudot_encode_table[ind][1];

    debug_log("I: (baudot_charset==%u)   input character '%c' 0x%02x charset_mask=%u\n", baudot_charset, char_out, char_out, charset_mask);

    if ( (baudot_charset & charset_mask ) == 0 ) {
	if ( charset_mask == 0 ) {
	    baudot_skip_warning(char_out);
	    return 0;
	}

	if ( baudot_charset == 0 )
	    baudot_charset = 1;

	if ( charset_mask != 3 )
	    baudot_charset = charset_mask;

	if ( baudot_charset == 1 )
	    databits_outp[n++] = BAUDOT_LTRS;
	else if ( baudot_charset == 2 )
	    databits_outp[n++] = BAUDOT_FIGS;
	else
	    assert(0);

	debug_log("I: emit charset select 0x%02X\n", databits_outp[n-1]);
    }

    if ( !( baudot_charset == 1 || baudot_charset == 2 ) ) {
	fprintf(stderr, "E: baudot input character failed '%c' 0x%02x\n",
		char_out, char_out);
	fprintf(stderr, "E: baudot_charset==%u\n", baudot_charset);
	assert(0);
    }

    databits_outp[n++] = baudot_encode_table[ind][0];

    /* TX un-shift on space */
    if ( char_out == ' ' )
	baudot_charset = 1;

    return n;
}
