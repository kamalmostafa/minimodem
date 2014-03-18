/*
 * simpleaudio.c
 *
 * Copyright (C) 2011-2012 Kamal Mostafa <kamal@whence.com>
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

#include "simpleaudio.h"
#include "simpleaudio_internal.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
# define USE_PULSEAUDIO 1
# define USE_ALSA 1
#endif

simpleaudio *
simpleaudio_open_stream(
		sa_backend_t	sa_backend,
		const char	*backend_device,
		sa_direction_t	sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    simpleaudio *sa = calloc(1, sizeof(simpleaudio));
    if ( !sa ) {
	perror("malloc");
        return NULL;
    }

    sa->format = sa_format;
    sa->rate = rate;
    sa->channels = channels;

    switch ( sa_format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
	    assert( sizeof(float) == 4 );
	    sa->samplesize = sizeof(float);
	    break;
	case SA_SAMPLE_FORMAT_S16:
	    assert( sizeof(short) == 2 );
	    sa->samplesize = sizeof(short);
	    break;
	default:
	    fprintf(stderr, "simpleaudio_open_stream: no such sa_format (%d)\n", sa_format);
	    goto err_out;
	    break;
    }

    switch ( sa_backend ) {

#if USE_SNDFILE
	case SA_BACKEND_FILE:
	    sa->backend = &simpleaudio_backend_sndfile;
	    break;
#endif

#if USE_BENCHMARKS
	case SA_BACKEND_BENCHMARK:
	    sa->backend = &simpleaudio_backend_benchmark;
	    break;
#endif

	case SA_BACKEND_SYSDEFAULT:
#if USE_PULSEAUDIO
	    sa->backend = &simpleaudio_backend_pulseaudio;
#elif USE_ALSA
	    sa->backend = &simpleaudio_backend_alsa;
#else
	    fprintf(stderr, "simpleaudio_open_stream: no SA_BACKEND_SYSDEFAULT was configured\n");
	    goto err_out;
#endif
	    break;

#if USE_ALSA
	case SA_BACKEND_ALSA:
	    sa->backend = &simpleaudio_backend_alsa;
	    break;
#endif

#if USE_PULSEAUDIO
	case SA_BACKEND_PULSEAUDIO:
	    sa->backend = &simpleaudio_backend_pulseaudio;
	    break;
#endif

	default:
	    fprintf(stderr, "simpleaudio_open_stream: no such sa_backend (%d).  not configured at build?\n", sa_backend);
	    goto err_out;
    }

    int ok = sa->backend->simpleaudio_open_stream(sa,
		backend_device, sa_stream_direction, sa_format,
		rate, channels, app_name, stream_name);

    if ( sa->channels != channels ) {
	fprintf(stderr, "%s: input stream must be %u-channel (not %u)\n",
		stream_name, channels, sa->channels);
	simpleaudio_close(sa);
	return 0;
    }

    if ( ok ) {
	assert( sa->backend_framesize == sa->channels * sa->samplesize );
	return sa;
    }

err_out:
    free(sa);
    return NULL;
}

sa_format_t
simpleaudio_get_format( simpleaudio *sa )
{
    return sa->format;
}

unsigned int
simpleaudio_get_rate( simpleaudio *sa )
{
    return sa->rate;
}

unsigned int
simpleaudio_get_channels( simpleaudio *sa )
{
    return sa->channels;
}

unsigned int
simpleaudio_get_framesize( simpleaudio *sa )
{
    return sa->backend_framesize;
}

unsigned int
simpleaudio_get_samplesize( simpleaudio *sa )
{
    return sa->samplesize;
}

void
simpleaudio_set_rxnoise( simpleaudio *sa, float rxnoise_factor )
{
    sa->rxnoise = rxnoise_factor;
}

ssize_t
simpleaudio_read( simpleaudio *sa, void *buf, size_t nframes )
{
    return sa->backend->simpleaudio_read(sa, buf, nframes);
}

ssize_t
simpleaudio_write( simpleaudio *sa, void *buf, size_t nframes )
{
    return sa->backend->simpleaudio_write(sa, buf, nframes);
}

void
simpleaudio_close( simpleaudio *sa )
{
    sa->backend->simpleaudio_close(sa);
    free(sa);
}
