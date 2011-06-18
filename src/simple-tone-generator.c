/*
 * simple-tone-generator.c
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
 */


#include <math.h>
#include <strings.h>
#include <assert.h>

#include "simpleaudio.h"


/* "current" phase state of the tone generator */
static float sa_tone_cphase = 0.0;

void
simpleaudio_tone_reset()
{
    sa_tone_cphase = 0.0;
}

void
simpleaudio_tone(simpleaudio *sa_out, float tone_freq, size_t nsamples_dur)
{
#define    TONE_GEN_BUFSIZE	(10*48000)	// FIXME
    float buf[TONE_GEN_BUFSIZE];

    assert ( nsamples_dur <= TONE_GEN_BUFSIZE );

    if ( tone_freq != 0 ) {

	float wave_nsamples = 48000.0 / tone_freq;	// FIXME rate

	size_t i;
	for ( i=0; i<nsamples_dur; i++ ) {
	    buf[i] = sinf( M_PI*2.0*( i/wave_nsamples + sa_tone_cphase) );
	    buf[i] /= 2.0;	// normalize (for pulseaudio?) */
	}

	sa_tone_cphase
	    = fmodf(sa_tone_cphase + (float)nsamples_dur/wave_nsamples, 1.0);

    } else {

	bzero(buf, nsamples_dur*sizeof(float));
	sa_tone_cphase = 0.0;

    }

    assert ( simpleaudio_write(sa_out, buf, nsamples_dur) > 0 );
}

