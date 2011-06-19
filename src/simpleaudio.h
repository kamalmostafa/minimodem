/*
 * simpleaudio.h
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
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

enum {
    SA_STREAM_PLAYBACK,
    SA_STREAM_RECORD,
};

simpleaudio *
simpleaudio_open_stream_pulseaudio(
		// unsigned int rate, unsigned int channels,
		int sa_stream_direction,
		char *app_name, char *stream_name );

simpleaudio *
simpleaudio_open_stream_sndfile(
		int sa_stream_direction,
		char *path );

/*
 * common simpleaudio_ API routines available to any backend:
 */

unsigned int
simpleaudio_get_rate( simpleaudio *sa );

unsigned int
simpleaudio_get_channels( simpleaudio *sa );

ssize_t
simpleaudio_read( simpleaudio *sa, float *buf, size_t nframes );

ssize_t
simpleaudio_write( simpleaudio *sa, float *buf, size_t nframes );

void
simpleaudio_close( simpleaudio *sa );


/*
 * simpleaudio tone generator
 */

void
simpleaudio_tone_reset();

void
simpleaudio_tone(simpleaudio *sa_out, float tone_freq, size_t nsamples_dur);


#endif
