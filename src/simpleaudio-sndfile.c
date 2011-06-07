
#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#include <sndfile.h>

#include "simpleaudio.h"
#include "simpleaudio_internal.h"


/*
 * sndfile backend for simpleaudio
 */


static ssize_t
sa_sndfile_read( simpleaudio *sa, float *buf, size_t nframes )
{
    SNDFILE *s = (SNDFILE *)sa->backend_handle;
    int n;
    if ((n = sf_readf_float(s, buf, nframes)) < 0) {
	fprintf(stderr, "sf_read_float: ");
	sf_perror(s);
	return -1;
    }
//fprintf(stderr, "sf_read_float: nframes=%ld n=%d\n", nframes, n);
    return n;
}


static void
sa_sndfile_close( simpleaudio *sa )
{
    sf_close(sa->backend_handle);
}


static const struct simpleaudio_backend simpleaudio_backend_pulse = {
    sa_sndfile_read,
    sa_sndfile_close,
};

simpleaudio *
simpleaudio_open_source_sndfile(char *path)
{
    SF_INFO sfinfo = {
	.format = 0
    };

    /* Create the recording stream */
    SNDFILE *s;
    s = sf_open(path, SFM_READ, &sfinfo);
    if ( !s ) {
	fprintf(stderr, "%s: ", path);
	sf_perror(s);
        return NULL;
    }

    simpleaudio *sa = malloc(sizeof(simpleaudio));
    if ( !sa ) {
	perror("malloc");
	sf_close(s);
        return NULL;
    }
    sa->rate = sfinfo.samplerate;
    sa->channels = sfinfo.channels;
    sa->backend = &simpleaudio_backend_pulse;
    sa->backend_handle = s;
    sa->backend_framesize = sa->channels * sizeof(float);

    return sa;
}

