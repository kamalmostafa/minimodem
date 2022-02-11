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

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * stdio backend for simpleaudio
 */

static ssize_t
sa_stdio_read( simpleaudio *sa, void *buf, size_t nframes )
{
    int n;
    switch ( sa->format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
                n = fread(buf, 4, nframes, stdin);
		break;
	case SA_SAMPLE_FORMAT_S16:
                n = fread(buf, 2, nframes, stdin);
		break;
	default:
		assert(0);
		break;
    }
    if ( n < 0 ) {
	fprintf(stderr, "read error");
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
    int n;
    switch ( sa->format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
                n = fwrite(buf, 4, nframes, stdout);
		break;
	case SA_SAMPLE_FORMAT_S16:
                n = fwrite(buf, 2, nframes, stdout);
		break;
	default:
		assert(0);
		break;
    }
    if ( n < 0 ) {
	fprintf(stderr, "write error");
	return -1;
    }
    return n;
}


static void
sa_stdio_close( simpleaudio *sa )
{
    // This is probably safe even when reading, right?
    fflush(stdout);
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
    /* good or bad to override these? */
    sa->rate = rate;
    sa->channels = channels;

    sa->backend_handle = NULL;
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
