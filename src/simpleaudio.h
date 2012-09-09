/*
 * simpleaudio.h
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

#ifndef SIMPLEAUDIO_H
#define SIMPLEAUDIO_H

#include <sys/types.h>

struct simpleaudio;
typedef struct simpleaudio simpleaudio;

/*
 * simpleaudio_open_source_XXXX() routines which return a (simpleaudio *)
 * are provided by the separate backend modules.
 *
 */

/* sa_backend */
typedef enum {
	SA_BACKEND_SYSDEFAULT=0,
	SA_BACKEND_FILE,
	SA_BACKEND_BENCHMARK,
	SA_BACKEND_ALSA,
	SA_BACKEND_PULSEAUDIO,
} sa_backend_t;

/* sa_stream_direction */
typedef enum {
	SA_STREAM_PLAYBACK,
	SA_STREAM_RECORD,
} sa_direction_t;

/* sa_stream_format */
typedef enum {
	SA_SAMPLE_FORMAT_S16,
	SA_SAMPLE_FORMAT_FLOAT,
} sa_format_t;

simpleaudio *
simpleaudio_open_stream(
		sa_backend_t	sa_backend,
		const char	*backend_device,
		sa_direction_t	sa_stream_direction,
		sa_format_t	sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name );

unsigned int
simpleaudio_get_rate( simpleaudio *sa );

unsigned int
simpleaudio_get_channels( simpleaudio *sa );

unsigned int
simpleaudio_get_framesize( simpleaudio *sa );

sa_format_t
simpleaudio_get_format( simpleaudio *sa );

unsigned int
simpleaudio_get_samplesize( simpleaudio *sa );

void
simpleaudio_set_rxnoise( simpleaudio *sa, float rxnoise_factor );

ssize_t
simpleaudio_read( simpleaudio *sa, void *buf, size_t nframes );

ssize_t
simpleaudio_write( simpleaudio *sa, void *buf, size_t nframes );

void
simpleaudio_close( simpleaudio *sa );


/*
 * simpleaudio tone generator
 */

void
simpleaudio_tone_reset();

void
simpleaudio_tone(simpleaudio *sa_out, float tone_freq, size_t nsamples_dur);

void
simpleaudio_tone_init( unsigned int new_sin_table_len, float mag );

#endif
