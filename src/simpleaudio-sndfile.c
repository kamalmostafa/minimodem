/*
 * simpleaudio-sndfile.c
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

#if USE_SNDFILE

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>

#include <sndfile.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * sndfile backend for simpleaudio
 */


static ssize_t
sa_sndfile_read( simpleaudio *sa, void *buf, size_t nframes )
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
    // fprintf(stderr, "sf_read: nframes=%ld n=%d\n", nframes, n);
    return n;
}


static ssize_t
sa_sndfile_write( simpleaudio *sa, void *buf, size_t nframes )
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
sa_sndfile_close( simpleaudio *sa )
{
    sf_close(sa->backend_handle);
}


/* (Why) doesn't libsndfile provide an API for this?... */
static const struct sndfile_format {
    unsigned int major_format;
    char *str;
} sndfile_formats[] = {
    { SF_FORMAT_WAV,	"WAV" },
    { SF_FORMAT_AIFF,	"AIFF" },
    { SF_FORMAT_AU,	"AU" },
    { SF_FORMAT_RAW,	"RAW" },
    { SF_FORMAT_PAF,	"PAF" },
    { SF_FORMAT_SVX,	"SVX" },
    { SF_FORMAT_NIST,	"NIST" },
    { SF_FORMAT_VOC,	"VOC" },
    { SF_FORMAT_IRCAM,	"IRCAM" },
    { SF_FORMAT_W64,	"W64" },
    { SF_FORMAT_MAT4,	"MAT4" },
    { SF_FORMAT_MAT5,	"MAT5" },
    { SF_FORMAT_PVF,	"PVF" },
    { SF_FORMAT_XI,	"XI" },
    { SF_FORMAT_HTK,	"HTK" },
    { SF_FORMAT_SDS,	"SDS" },
    { SF_FORMAT_AVR,	"AVR" },
    { SF_FORMAT_WAVEX,	"WAVEX" },
    { SF_FORMAT_SD2,	"SD2" },
    { SF_FORMAT_FLAC,	"FLAC" },
    { SF_FORMAT_CAF,	"CAF" },
    { SF_FORMAT_WVE,	"WVE" },
    { SF_FORMAT_OGG,	"OGG" },
    { SF_FORMAT_MPC2K,	"MPC2K" },
    { SF_FORMAT_RF64,	"RF64" },
    { 0, 0 }
};

static unsigned int
sndfile_format_from_path( const char *path )
{
    const char *p = strrchr(path, '.');
    if ( p )
	p++;
    else
	p = path;

    const struct sndfile_format *sfmt;
    for ( sfmt=sndfile_formats; sfmt->str; sfmt++ )
	if ( strcasecmp(sfmt->str,p) == 0 )
	    return sfmt->major_format;
    return SF_FORMAT_WAV;
}

static int
sa_sndfile_open_stream(
		simpleaudio *sa,
		sa_direction_t sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    const char *path = stream_name;

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

    if ( sa_stream_direction == SA_STREAM_PLAYBACK )
	sfinfo.format = sndfile_format_from_path(path) | sf_format;

    /* Create the recording stream */
    SNDFILE *s;
    s = sf_open(path,
	    sa_stream_direction == SA_STREAM_RECORD ? SFM_READ : SFM_WRITE,
	    &sfinfo);
    if ( !s ) {
	fprintf(stderr, "%s: ", path);
	sf_perror(s);
        return 0;
    }

    /* good or bad to override these? */
    sa->rate = sfinfo.samplerate;
    sa->channels = sfinfo.channels;

    sa->backend_handle = s;
    sa->backend_framesize = sa->channels * sa->samplesize; 

    return 1;
}


const struct simpleaudio_backend simpleaudio_backend_sndfile = {
    sa_sndfile_open_stream,
    sa_sndfile_read,
    sa_sndfile_write,
    sa_sndfile_close,
};

#endif /* USE_SNDFILE */
