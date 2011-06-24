/*
 * simpleaudio-sndfile.c
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


#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#include <sndfile.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * sndfile backend for simpleaudio
 */


static ssize_t
sa_sndfile_read( simpleaudio *sa, float *buf, size_t nframes )
{
    SNDFILE *s = (SNDFILE *)sa->backend_handle;
    int n;
    if ((n = sf_readf_float(s, buf, nframes)) < 0) {
	fprintf(stderr, "sf_read_float: ");
	sf_perror(s);
	return -1;
    }
//fprintf(stderr, "sf_readf_float: nframes=%ld n=%d\n", nframes, n);
    return n;
}


static ssize_t
sa_sndfile_write( simpleaudio *sa, float *buf, size_t nframes )
{
//fprintf(stderr, "sf_writef_float: nframes=%ld\n", nframes);
    SNDFILE *s = (SNDFILE *)sa->backend_handle;
    int n;
    if ((n = sf_writef_float(s, buf, nframes)) < 0) {
	fprintf(stderr, "sf_read_float: ");
	sf_perror(s);
	return -1;
    }
    return n;
}


static void
sa_sndfile_close( simpleaudio *sa )
{
    sf_close(sa->backend_handle);
}


static const struct simpleaudio_backend simpleaudio_backend_sndfile = {
    sa_sndfile_read,
    sa_sndfile_write,
    sa_sndfile_close,
};

simpleaudio *
simpleaudio_open_stream_sndfile(
		int sa_stream_direction,
		char *path )
{
    /* setting for SA_STREAM_PLAYBACK (file write) */
    SF_INFO sfinfo = {
	.format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16,	// FIXME - hardcoded
	.samplerate = 48000,
	.channels = 1,
    };

    if ( sa_stream_direction == SA_STREAM_RECORD )
	sfinfo.format = 0;

    /* Create the recording stream */
    SNDFILE *s;
    s = sf_open(path,
	    sa_stream_direction == SA_STREAM_RECORD ? SFM_READ : SFM_WRITE,
	    &sfinfo);
    if ( !s ) {
	fprintf(stderr, "%s: ", path);
	sf_perror(s);
        return NULL;
    }

    simpleaudio *sa = malloc(sizeof(simpleaudio));
    if ( !sa ) {
	perror("malloc");
	sf_close(s);
        return NULL;
    }
    sa->rate = sfinfo.samplerate;
    sa->channels = sfinfo.channels;
    sa->backend = &simpleaudio_backend_sndfile;
    sa->backend_handle = s;
    sa->backend_framesize = sa->channels * sizeof(float);

    return sa;
}

