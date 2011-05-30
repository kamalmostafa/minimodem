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

simpleaudio *
simpleaudio_open_source_pulseaudio(
		// unsigned int rate, unsigned int channels,
		char *app_name, char *stream_name );

simpleaudio *
simpleaudio_open_source_sndfile(char *path);

/*
 * common simpleaudio_ API routines available to any backend:
 */

unsigned int
simpleaudio_get_rate( simpleaudio *sa );

unsigned int
simpleaudio_get_channels( simpleaudio *sa );

size_t
simpleaudio_read( simpleaudio *sa, float *buf, size_t nframes );

void
simpleaudio_close( simpleaudio *sa );


#endif
