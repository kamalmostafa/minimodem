/*
 * simpleaudio-stdfile.c
 *
 * Copyright (C) 2011-2020 Kamal Mostafa <kamal@whence.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if USE_STDIO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sndfile.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * stdio backend for simpleaudio
 */


static ssize_t
sa_stdio_read( simpleaudio *sa, void *buf, size_t nframes )
{
    SNDFILE *s = (SNDFILE *)sa->backend_handle;
    int n;
    switch ( sa->format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
		n = sf_readf_float(s, buf, nframes);
		break;
	case SA_SAMPLE_FORMAT_S16:
		n = sf_readf_short(s, buf, nframes);
		break;
	default:
		assert(0);
		break;
    }
    if ( n < 0 ) {
	fprintf(stderr, "sf_read: ");
	sf_perror(s);
	return -1;
    }

    if ( sa->rxnoise != 0.0f ) {
	int i;
	float *fbuf = buf;
	float f = sa->rxnoise * 2;
	for ( i=0; i<nframes; i++ )
	    fbuf[i] += (rand()/RAND_MAX - 0.5f) * f;
    }

    // fprintf(stderr, "sf_read: nframes=%ld n=%d\n", nframes, n);
    return n;
}


static ssize_t
sa_stdio_write( simpleaudio *sa, void *buf, size_t nframes )
{
    // fprintf(stderr, "sf_write: nframes=%ld\n", nframes);
    SNDFILE *s = (SNDFILE *)sa->backend_handle;
    int n;
    switch ( sa->format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
		n = sf_writef_float(s, buf, nframes);
		break;
	case SA_SAMPLE_FORMAT_S16:
		n = sf_writef_short(s, buf, nframes);
		break;
	default:
		assert(0);
		break;
    }
    if ( n < 0 ) {
	fprintf(stderr, "sf_write: ");
	sf_perror(s);
	return -1;
    }
    return n;
}


static void
sa_stdio_close( simpleaudio *sa )
{
    sf_close(sa->backend_handle);
}

static int
sa_stdio_open_stream(
		simpleaudio *sa,
		const char *backend_device,
		sa_direction_t sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    int sf_format;
    switch ( sa->format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
		sf_format = SF_FORMAT_FLOAT;
		break;
	case SA_SAMPLE_FORMAT_S16:
		sf_format = SF_FORMAT_PCM_16;
		break;
	default:
		assert(0);
    }

    /* setting for SA_STREAM_PLAYBACK (file write) */
    SF_INFO sfinfo = {
	.format = sf_format,
	.samplerate = rate,
	.channels = channels,
    };

    /* Create the recording stream from stdin or stdout */
    SNDFILE *s;
    s = sf_open_fd(sa_stream_direction == SA_STREAM_RECORD ? 0 : 1,
	    sa_stream_direction == SA_STREAM_RECORD ? SFM_READ : SFM_WRITE,
	    &sfinfo,
            0);
    if ( !s ) {
	sf_perror(s);
        return 0;
    }

    // Disable the insertion of this questionable "PEAK chunk" header thing.
    // Relates only to writing SF_FORMAT_FLOAT .wav and .aiff files
    // (minimodem --tx --float-samples).  When left enabled, this adds some
    // wonky bytes to the header which change from run to run (different every
    // wall-clock second.  WTF?
    // http://www.mega-nerd.com/libsndfile/command.html#SFC_SET_ADD_PEAK_CHUNK
    /* Turn off the PEAK chunk. */
    sf_command(s, SFC_SET_ADD_PEAK_CHUNK, NULL, SF_FALSE);

    /* good or bad to override these? */
    sa->rate = sfinfo.samplerate;
    sa->channels = sfinfo.channels;

    sa->backend_handle = s;
    sa->backend_framesize = sa->channels * sa->samplesize; 

    return 1;
}


const struct simpleaudio_backend simpleaudio_backend_stdio = {
    sa_stdio_open_stream,
    sa_stdio_read,
    sa_stdio_write,
    sa_stdio_close,
};

#endif /* USE_STDIO */
