/*
 * simpleaudio_internal.h
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

#ifndef SIMPLEAUDIO_INTERNAL_H
#define SIMPLEAUDIO_INTERNAL_H


#include "simpleaudio.h"


struct simpleaudio_backend;
typedef struct simpleaudio_backend simpleaudio_backend;

struct simpleaudio {
	const struct simpleaudio_backend	*backend;
	sa_format_t	format;
	unsigned int	rate;
	unsigned int	channels;
	void *		backend_handle;
	unsigned int	samplesize;
	unsigned int	backend_framesize;
	float		rxnoise;		// only for the sndfile backend
};

struct simpleaudio_backend {

	int /* boolean 'ok' value */
	(*simpleaudio_open_stream)(
		simpleaudio *	sa,
		const char	*backend_device,
		sa_direction_t	sa_stream_direction,
		sa_format_t sa_format,
		unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name );

	ssize_t
	(*simpleaudio_read)( simpleaudio *sa, void *buf, size_t nframes );

	ssize_t
	(*simpleaudio_write)( simpleaudio *sa, void *buf, size_t nframes );

	void
	(*simpleaudio_close)( simpleaudio *sa );
};

extern const struct simpleaudio_backend simpleaudio_backend_benchmark;
extern const struct simpleaudio_backend simpleaudio_backend_sndfile;
extern const struct simpleaudio_backend simpleaudio_backend_alsa;
extern const struct simpleaudio_backend simpleaudio_backend_pulseaudio;

#endif
