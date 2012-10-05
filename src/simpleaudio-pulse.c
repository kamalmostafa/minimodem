/*
 * simpleaudio-pulse.c
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

#if USE_PULSEAUDIO

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * pulseaudio[pa_simple] backend for simpleaudio
 */


static ssize_t
sa_pulse_read( simpleaudio *sa, void *buf, size_t nframes )
{
    int error;
    pa_simple *s = (pa_simple *)sa->backend_handle;
    size_t nbytes = nframes * sa->backend_framesize;
    /* N.B. pa_simple_read always returns 0 or -1, not the number of
     * read bytes!*/
    if (pa_simple_read(s, buf, nbytes, &error) < 0) {
	fprintf(stderr, "pa_simple_read: %s\n", pa_strerror(error));
	return -1;
    }
    return nframes;
}


static ssize_t
sa_pulse_write( simpleaudio *sa, void *buf, size_t nframes )
{
    int error;
    pa_simple *s = (pa_simple *)sa->backend_handle;
    size_t nbytes = nframes * sa->backend_framesize;
    /* ????? N.B. pa_simple_write always returns 0 or -1, not the number of
     * written bytes!*/
    if (pa_simple_write(s, buf, nbytes, &error) < 0) {
	fprintf(stderr, "pa_simple_write: %s\n", pa_strerror(error));
	return -1;
    }
    return nframes;
}


static void
sa_pulse_close( simpleaudio *sa )
{
    pa_simple_drain(sa->backend_handle, NULL);
    pa_simple_free(sa->backend_handle);
}

static int
sa_pulse_open_stream(
		simpleaudio *sa,
		const char *backend_device,
		sa_direction_t sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    int error;

    // FIXME - use source for something
    // just take the default pulseaudio source for now

    pa_sample_format_t pa_format;

    switch ( sa->format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
		pa_format = PA_SAMPLE_FLOAT32;
		break;
	case SA_SAMPLE_FORMAT_S16:
		pa_format = PA_SAMPLE_S16LE;	// FIXME: handle S16BE
		break;
	default:
		assert(0);
    }

    /* The sample type to use */
    pa_sample_spec ss = {
        .format = pa_format,
	.rate = rate,
	.channels = channels,
    };

    pa_buffer_attr attr = {
	.maxlength = (uint32_t)-1,
	.tlength = (uint32_t)-1,
	.prebuf = (uint32_t)-1,
	.minreq = (uint32_t)-1,
	.fragsize = (uint32_t)-1,
    };

    attr.fragsize = 0;	/* set for lowest possible capture latency */
    attr.tlength = 0;	/* set lowest possible playback latency */
    // Do not mess with attr.prebuf!  Setting it =1 causes some playback (--tx)
    // sessions to be wiped out by noise (I do not know why).

    /* Create the playback or recording stream */
    pa_simple *s;
    s = pa_simple_new(NULL, app_name,
	    sa_stream_direction == SA_STREAM_RECORD ? PA_STREAM_RECORD : PA_STREAM_PLAYBACK,
	    NULL, stream_name,
	    &ss, NULL, &attr, &error);
    if ( !s ) {
        fprintf(stderr, "E: Cannot create PulseAudio stream: %s\n ", pa_strerror(error));
        return 0;
    }

    /* good or bad to override these? */
    sa->rate = ss.rate;
    sa->channels = ss.channels;

    sa->backend_handle = s;
    sa->backend_framesize = pa_frame_size(&ss);

    return 1;
}


const struct simpleaudio_backend simpleaudio_backend_pulseaudio = {
    sa_pulse_open_stream,
    sa_pulse_read,
    sa_pulse_write,
    sa_pulse_close,
};

#endif /* USE_PULSEAUDIO */
