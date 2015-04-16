/*
 * simpleaudio-alsa.c
 *
 * Copyright (C) 2012 Kamal Mostafa <kamal@whence.com>
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

#if USE_ALSA

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <alsa/asoundlib.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * ALSA backend for simpleaudio
 */


static ssize_t
sa_alsa_read( simpleaudio *sa, void *buf, size_t nframes )
{
    ssize_t frames_read = 0;
    snd_pcm_t *pcm = (snd_pcm_t *)sa->backend_handle;
    while ( frames_read < nframes ) {
	ssize_t r;
	void * data = buf+frames_read*sa->backend_framesize;
	ssize_t count = nframes-frames_read;
	r = snd_pcm_readi(pcm, data, count);
	if ( r >= 0 ) {
	    frames_read += r;
	    if ( r != count )
		fprintf(stderr, "#short+%zd#\n", r);
	    continue;
	}
	if (r == -EPIPE) {	// Underrun
	    fprintf(stderr, "#");
	    snd_pcm_prepare(pcm);
	} else  {
	    fprintf(stderr, "snd_pcm_readi: %s\n", snd_strerror(r));
	    if (r == -EAGAIN || r== -ESTRPIPE)
		snd_pcm_wait(pcm, 1000);
	    else
		return r;
	}
    }
    // fprintf(stderr,("[%zd]\n"), frames_read);
    return frames_read;
}


static ssize_t
sa_alsa_write( simpleaudio *sa, void *buf, size_t nframes )
{
    ssize_t frames_written = 0;
    snd_pcm_t *pcm = (snd_pcm_t *)sa->backend_handle;
    while ( frames_written < nframes ) {
	ssize_t r;
	r = snd_pcm_writei(pcm, buf+frames_written*sa->backend_framesize, nframes-frames_written);
	if (r < 0) {
	    /* recover from e.g. underruns, and try once more */
	    snd_pcm_recover(pcm, r, 0 /*silent*/);
	    r = snd_pcm_writei(pcm, buf+frames_written*sa->backend_framesize, nframes-frames_written);
	}
	if (r < 0) {
	    fprintf(stderr, "E: %s\n", snd_strerror(frames_written));
	    return -1;
	}
	frames_written += r;
    }
    assert (frames_written == nframes);
    return frames_written;
}


static void
sa_alsa_close( simpleaudio *sa )
{
    snd_pcm_drain(sa->backend_handle);
    snd_pcm_close(sa->backend_handle);
}

static int
sa_alsa_open_stream(
		simpleaudio *sa,
		const char *backend_device,
		sa_direction_t sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    snd_pcm_t *pcm;
    int error;

    char *be_device;
    if ( ! backend_device ) {
	be_device = "default";
    } else {
	be_device = alloca(32);
	if ( strchr(backend_device, ':') )
	    snprintf(be_device, 32, "%s", backend_device);
	else if ( strchr(backend_device, ',') )
	    snprintf(be_device, 32, "plughw:%s", backend_device);
	else
	    snprintf(be_device, 32, "plughw:%s,0", backend_device);
    }

    error = snd_pcm_open(&pcm,
		be_device,
		sa_stream_direction == SA_STREAM_RECORD ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
		0 /*mode*/);
    if (error) {
	fprintf(stderr, "E: Cannot create ALSA stream: %s\n", snd_strerror(error));
	return 0;
    }

    snd_pcm_format_t pcm_format;

    switch ( sa->format ) {
	case SA_SAMPLE_FORMAT_FLOAT:
		pcm_format = SND_PCM_FORMAT_FLOAT;
		break;
	case SA_SAMPLE_FORMAT_S16:
		pcm_format = SND_PCM_FORMAT_S16;
		break;
	default:
		assert(0);
    }

    /* set up ALSA hardware params */
    error = snd_pcm_set_params(pcm,
		pcm_format,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		channels,
		rate,
		1 /* soft_resample (allow) */,
		100000 /* latency (us) */);
    if (error) {
	fprintf(stderr, "E: %s\n", snd_strerror(error));
	snd_pcm_close(pcm);
	return 0;
    }

#if 0
    snd_pcm_sw_params_t  *swparams;
    snd_pcm_sw_params_alloca(&swparams);
    error = snd_pcm_sw_params_current(pcm, swparams);
    if (error) {
	fprintf(stderr, "E: %s\n", snd_strerror(error));
	snd_pcm_close(pcm);
	return NULL;
    }
    snd_pcm_sw_params_set_start_threshold(pcm, swparams, NFRAMES_VAL);
    snd_pcm_sw_params_set_stop_threshold(pcm, swparams, NFRAMES_VAL);
    snd_pcm_sw_params_set_silence_threshold(pcm, swparams, NFRAMES_VAL);
    error = snd_pcm_sw_params(pcm, swparams);
    if (error) {
	fprintf(stderr, "E: %s\n", snd_strerror(error));
	snd_pcm_close(pcm);
	return NULL;
    }
#endif

    sa->backend_handle = pcm;
    sa->backend_framesize = sa->channels * sa->samplesize; 

    return 1;
}


const struct simpleaudio_backend simpleaudio_backend_alsa = {
    sa_alsa_open_stream,
    sa_alsa_read,
    sa_alsa_write,
    sa_alsa_close,
};

#endif /* USE_ALSA */
