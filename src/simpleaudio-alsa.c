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
sa_alsa_read( simpleaudio *sa, float *buf, size_t nframes )
{
    ssize_t frames_read = 0;
    snd_pcm_t *pcm = (snd_pcm_t *)sa->backend_handle;
    while ( frames_read < nframes ) {
	ssize_t r;
	r = snd_pcm_readi(pcm, buf, nframes-frames_read);
	if (r < 0) {
	    /* silently recover from e.g. overruns, and try once more */
	    snd_pcm_prepare(pcm);
	    r = snd_pcm_readi(pcm, buf, nframes-frames_read);
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
sa_alsa_write( simpleaudio *sa, float *buf, size_t nframes )
{
    ssize_t frames_written;
    snd_pcm_t *pcm = (snd_pcm_t *)sa->backend_handle;
    frames_written = snd_pcm_writei(pcm, buf, nframes);
    if (frames_written < 0) {
	/* silently recover from e.g. underruns, and try once more */
	snd_pcm_recover(pcm, frames_written, 1 /*silent*/);
	frames_written = snd_pcm_writei(pcm, buf, nframes);
    }
    if (frames_written < 0) {
	fprintf(stderr, "E: %s\n", snd_strerror(frames_written));
	return -1;
    }
    return frames_written;
}


static void
sa_alsa_close( simpleaudio *sa )
{
    snd_pcm_drain(sa->backend_handle);
    snd_pcm_close(sa->backend_handle);
}


static const struct simpleaudio_backend simpleaudio_backend_alsa = {
    sa_alsa_read,
    sa_alsa_write,
    sa_alsa_close,
};

simpleaudio *
simpleaudio_open_stream_alsa(
		// unsigned int rate, unsigned int channels,
		int sa_stream_direction,
		char *app_name, char *stream_name )
{
    unsigned int rate = 48000;
    unsigned int channels = 1;

    snd_pcm_t *pcm;
    int error;

    error = snd_pcm_open(&pcm,
		"plughw:0,0",
		sa_stream_direction == SA_STREAM_RECORD ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
		0 /*mode*/);
    if (error) {
	fprintf(stderr, "E: Cannot create ALSA stream: %s\n", snd_strerror(error));
	return NULL;
    }

    error = snd_pcm_set_params(pcm,
		SND_PCM_FORMAT_FLOAT,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		channels,
		rate,
		1 /* soft_resample (allow) */,
		0 /* latency */);
    if (error) {
	fprintf(stderr, "E: %s\n", snd_strerror(error));
	snd_pcm_close(pcm);
	return NULL;
    }

    simpleaudio *sa = malloc(sizeof(simpleaudio));
    if ( !sa ) {
	perror("malloc");
	snd_pcm_close(pcm);
        return NULL;
    }
    sa->rate = rate;
    sa->channels = channels;
    sa->backend = &simpleaudio_backend_alsa;
    sa->backend_handle = pcm;
    sa->backend_framesize = sizeof(float);

    return sa;
}

#endif /* USE_PULSEAUDIO */
