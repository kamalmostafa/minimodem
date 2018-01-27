/*
 * simpleaudio-sndio.c
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

#if USE_SNDIO

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sndio.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * sndio backend for simpleaudio
 */


static ssize_t
sa_sndio_read( simpleaudio *sa, void *buf, size_t nframes )
{
    size_t nbytes = nframes * sa->backend_framesize;
    sio_read((struct sio_hdl *)sa->backend_handle, buf, nbytes);
    return nframes;
}


static ssize_t
sa_sndio_write( simpleaudio *sa, void *buf, size_t nframes )
{
    size_t nbytes = nframes * sa->backend_framesize;
    sio_write((struct sio_hdl *)sa->backend_handle, buf, nbytes);
    return nframes;
}


static void
sa_sndio_close( simpleaudio *sa )
{
    sio_stop(sa->backend_handle);
}


static int
sa_sndio_open_stream(
		simpleaudio *sa,
		const char *backend_device,
		sa_direction_t sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    struct sio_hdl *hdl;
    struct sio_par par;

    const char *be_device;
    if ( ! backend_device )
        be_device = SIO_DEVANY;
    else
        be_device = backend_device;

    hdl = sio_open(
		be_device,
		sa_stream_direction == SA_STREAM_RECORD ? SIO_REC : SIO_PLAY,
		0 /* nbio_flag */);
    sio_initpar(&par);

    switch ( sa->format ) {
        case SA_SAMPLE_FORMAT_S16:
		par.bits = 16;
		par.sig = 1;
		par.le = SIO_LE_NATIVE;
		break;
	// FIXME: Add support for SA_SAMPLE_FORMAT_FLOAT
        default:
		assert(0);
    }

    par.bps = SIO_BPS(par.bits);
    par.rate = rate;
    par.xrun = SIO_IGNORE;

    if ( SA_STREAM_RECORD )
        par.rchan = channels;
    else
        par.pchan = channels;

    sio_setpar(hdl, &par);
    sio_start(hdl);

    sa->backend_handle = hdl;
    sa->backend_framesize = sa->channels * sa->samplesize;

    return 1;
}


const struct simpleaudio_backend simpleaudio_backend_sndio = {
    sa_sndio_open_stream,
    sa_sndio_read,
    sa_sndio_write,
    sa_sndio_close,
};

#endif /* USE_SNDIO */
