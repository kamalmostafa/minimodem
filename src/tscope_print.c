/*
 * tscope_print.c
 *
 * Author: Kamal Mostafa <kamal@whence.com>
 *
 * Unpublished work, not licensed for any purpose.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <alloca.h>
#include <math.h>

#include <fftw3.h>

#include "tscope_print.h"


static inline
float
band_mag( fftwf_complex * const cplx, unsigned int band, float scalar )
{
    float re = cplx[band][0];
    float im = cplx[band][1];
    float mag = hypot(re, im) * scalar;
    return mag;
}


void
tscope_print( fftwf_complex * const fftout, int nbands, float magscalar,
	int one_line_mode, int show_maxmag )
{
    char *buf = alloca(nbands+1);
    char magchars[] = " .-=+#^";
    if ( one_line_mode )
	magchars[0] = '_';
    float maxmag = 0;
    int i;
    for ( i=0; i<nbands; i++ ) {
	char c;
	float mag = band_mag(fftout, i, magscalar);
	if ( maxmag < mag )
	     maxmag = mag;
	     if ( mag <= 0.05 ) c = magchars[0];
	else if ( mag <= 0.10 ) c = magchars[1];
	else if ( mag <= 0.25 ) c = magchars[2];
	else if ( mag <= 0.50 ) c = magchars[3];
	else if ( mag <= 0.95 ) c = magchars[4];
	else if ( mag <= 1.00 ) c = magchars[5];
	else c = magchars[6];
	buf[i] = c;
    }
    buf[i] = 0;
    if ( show_maxmag )
	printf(" %.2f", maxmag);
    printf("|%s|", buf);
}

