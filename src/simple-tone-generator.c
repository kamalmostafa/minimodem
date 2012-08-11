/*
 * simple-tone-generator.c
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


#include <math.h>
#include <strings.h>
#include <malloc.h>
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
    float *buf = malloc(nsamples_dur * sizeof(float));
    assert(buf);

    if ( tone_freq != 0 ) {

	float wave_nsamples = simpleaudio_get_rate(sa_out) / tone_freq;

	size_t i;
	for ( i=0; i<nsamples_dur; i++ )
	    buf[i] = sinf( M_PI*2.0*( i/wave_nsamples + sa_tone_cphase) );

	sa_tone_cphase
	    = fmodf(sa_tone_cphase + (float)nsamples_dur/wave_nsamples, 1.0);

    } else {

	bzero(buf, nsamples_dur*sizeof(float));
	sa_tone_cphase = 0.0;

    }

    assert ( simpleaudio_write(sa_out, buf, nsamples_dur) > 0 );

    free(buf);
}

