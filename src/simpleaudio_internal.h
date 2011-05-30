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
	size_t
	(*simpleaudio_read)( simpleaudio *sa, float *buf, size_t nframes );
	void
	(*simpleaudio_close)( simpleaudio *sa );
};


#endif
