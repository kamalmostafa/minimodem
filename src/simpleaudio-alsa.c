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
#include <malloc.h>
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
	r = snd_pcm_readi(pcm, buf+frames_read*sa->backend_framesize, nframes-frames_read);
	if (r < 0) {
	    /* recover from e.g. overruns, and try once more */
	    fprintf(stderr, "snd_pcm_readi: reset for %s\n", snd_strerror(r));
	    snd_pcm_prepare(pcm);
	    r = snd_pcm_readi(pcm, buf+frames_read*sa->backend_framesize, nframes-frames_read);
	}
	if (r < 0) {
	    fprintf(stderr, "snd_pcm_readi: %s\n", snd_strerror(r));
	    return -1;
	}
	if (r == 0)
	    break;
	frames_read += r;
    }
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
		sa_direction_t sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name )
{
    snd_pcm_t *pcm;
    int error;

    error = snd_pcm_open(&pcm,
		"plughw:0,0",
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
		200*1000 /* latency (200 ms) */);
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
