/*
 * simpleaudio.c
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
 */

#include "simpleaudio.h"
#include "simpleaudio_internal.h"
#include "malloc.h"

unsigned int
simpleaudio_get_rate( simpleaudio *sa )
{
    return sa->rate;
}

unsigned int
simpleaudio_get_channels( simpleaudio *sa )
{
    return sa->channels;
}

ssize_t
simpleaudio_read( simpleaudio *sa, float *buf, size_t nframes )
{
    return sa->backend->simpleaudio_read(sa, buf, nframes);
}

ssize_t
simpleaudio_write( simpleaudio *sa, float *buf, size_t nframes )
{
    return sa->backend->simpleaudio_write(sa, buf, nframes);
}

void
simpleaudio_close( simpleaudio *sa )
{
    sa->backend->simpleaudio_close(sa);
    free(sa);
}
