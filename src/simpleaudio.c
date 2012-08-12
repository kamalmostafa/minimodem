/*
 * simpleaudio.c
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

unsigned int
simpleaudio_get_framesize( simpleaudio *sa )
{
    return sa->backend_framesize;
}

unsigned int
simpleaudio_get_samplesize( simpleaudio *sa )
{
    return sa->samplesize;
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
