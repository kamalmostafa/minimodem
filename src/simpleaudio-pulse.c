/*
 * simpleaudio-pulse.c
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
 */


#include <stdio.h>
#include <malloc.h>
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
sa_pulse_read( simpleaudio *sa, float *buf, size_t nframes )
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
sa_pulse_write( simpleaudio *sa, float *buf, size_t nframes )
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
    pa_simple_free(sa->backend_handle);
}


static const struct simpleaudio_backend simpleaudio_backend_pulse = {
    sa_pulse_read,
    sa_pulse_write,
    sa_pulse_close,
};

simpleaudio *
simpleaudio_open_stream_pulseaudio(
		// unsigned int rate, unsigned int channels,
		int sa_stream_direction,
		char *app_name, char *stream_name )
{
    int error;

    // FIXME - use source for something
    // just take the default pulseaudio source for now

    /* The sample type to use */
    pa_sample_spec ss = {
        .format = PA_SAMPLE_FLOAT32,
	// .rate = rate,
	.rate = 48000,
	// .channels = channels,
	.channels = 1,
    };

    /* Create the recording stream */
    pa_simple *s;
    s = pa_simple_new(NULL, app_name,
	    sa_stream_direction == SA_STREAM_RECORD ? PA_STREAM_RECORD : PA_STREAM_PLAYBACK,
	    NULL, stream_name,
	    &ss, NULL, NULL, &error);
    if ( !s ) {
        fprintf(stderr, "pa_simple_new: %s\n", pa_strerror(error));
        return NULL;
    }

    simpleaudio *sa = malloc(sizeof(simpleaudio));
    if ( !sa ) {
	perror("malloc");
	pa_simple_free(s);
        return NULL;
    }
    sa->rate = ss.rate;
    sa->channels = ss.channels;
    sa->backend = &simpleaudio_backend_pulse;
    sa->backend_handle = s;
    sa->backend_framesize = pa_frame_size(&ss);

    assert( sa->backend_framesize == ss.channels * sizeof(float) );

    return sa;
}

