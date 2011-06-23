/*
 * simpleaudio_internal.h
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

#ifndef SIMPLEAUDIO_INTERNAL_H
#define SIMPLEAUDIO_INTERNAL_H


#include "simpleaudio.h"

/*
 * Backend modules must provide an "open" routine which returns a
 * (simpleaudio *) to the caller.
 */

struct simpleaudio_backend;
typedef struct simpleaudio_backend simpleaudio_backend;

struct simpleaudio {
	const struct simpleaudio_backend	*backend;
	unsigned int	rate;
	unsigned int	channels;
	void *		backend_handle;
	unsigned int	backend_framesize;
};

struct simpleaudio_backend {
	ssize_t
	(*simpleaudio_read)( simpleaudio *sa, float *buf, size_t nframes );
	ssize_t
	(*simpleaudio_write)( simpleaudio *sa, float *buf, size_t nframes );
	void
	(*simpleaudio_close)( simpleaudio *sa );
};


#endif
