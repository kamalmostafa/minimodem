/*
 * simpleaudio-benchmark.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if USE_BENCHMARKS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/time.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * benchmark backend for simpleaudio
 */

struct benchmark_data {
    struct timeval	tv_start;
    unsigned long long	total_nframes;
};


static ssize_t
sa_benchmark_dummy_readwrite( simpleaudio *sa, void *buf, size_t nframes )
{
    struct benchmark_data *d = sa->backend_handle;
    d->total_nframes += nframes;
    return nframes;
}

static void
sa_benchmark_close( simpleaudio *sa )
{
    struct timeval tv_stop;
    gettimeofday(&tv_stop, NULL);

    struct benchmark_data *d = sa->backend_handle;
    unsigned long long runtime_usec, playtime_usec;
    runtime_usec = (tv_stop.tv_sec - d->tv_start.tv_sec) * 1000000;
    runtime_usec += tv_stop.tv_usec;
    runtime_usec -= d->tv_start.tv_usec;

    playtime_usec = d->total_nframes * 1000000 / sa->rate;

    unsigned long long performance = d->total_nframes * 1000000 / runtime_usec;

    fprintf(stdout, "    frames count:    \t%llu\n", d->total_nframes);
    fprintf(stdout, "    audio playtime:  \t%2llu.%06llu sec\n",
	    playtime_usec/1000000, playtime_usec%1000000);
    fprintf(stdout, "    elapsed runtime: \t%2llu.%06llu sec\n",
	    runtime_usec/1000000, runtime_usec%1000000);
    fprintf(stdout, "    performance:     \t%llu samples/sec\n",
	    performance);
    fflush(stdout);

    free(sa->backend_handle);
}

static int
sa_benchmark_open_stream(
		simpleaudio *sa,
		const char *backend_device,
		sa_direction_t sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    struct benchmark_data *d = calloc(1, sizeof(struct benchmark_data));
    if ( !d ) {
	perror("malloc");
	return 0;
    }

    sa->backend_handle = d;
    sa->backend_framesize = sa->channels * sa->samplesize; 

    fprintf(stdout, "  %s\n", stream_name);
    fflush(stdout);

    gettimeofday(&d->tv_start, NULL);

    return 1;
}


const struct simpleaudio_backend simpleaudio_backend_benchmark = {
    sa_benchmark_open_stream,
    sa_benchmark_dummy_readwrite /* read */,
    sa_benchmark_dummy_readwrite /* write */,
    sa_benchmark_close,
};

#endif /* USE_BENCHMARKS */
