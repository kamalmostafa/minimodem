/*
 * tscope.c
 *
 * Author: Kamal Mostafa <kamal@whence.com>
 *
 * Unpublished work, not licensed for any purpose.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include <fftw3.h>

#include "tscope_print.h"


int
main( int argc, char*argv[] )
{
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_FLOAT32,
        .rate = 9600,		// pulseaudio will resample to this rate
        .channels = 2		// 2 channel stereo
        //.channels = 1		// downmix (additively) to 1 channel
        //.channels = 3		// 2 channel stereo + 1 mixed channel
    };

#if 1
    static pa_buffer_attr pa_ba = {
	.maxlength = (uint32_t)-1,
	.fragsize = 0,	// filled in at runtime
    };
#endif

    unsigned int	decode_rate = 50;
    unsigned int	band_width = decode_rate;

    int			one_line_mode = 1;
    int			show_maxmag = 1;

    if ( ! isatty(1) )
	one_line_mode = 0;

    int argi = 1;
    while ( argi < argc && argv[argi][0] == '-' ) {
	/* -s switch enables "scrolling mode" instead of "one line mode" */
	if ( argv[argi][1] == 's' ) {
	    one_line_mode = 0;
	} else {
	    fprintf(stderr,
		"usage: tscope [-s] [ analysis_rate [ band_width ] ]\n");
	    return 1;
	}
	argi++;
    }
    if ( argi < argc )
	decode_rate = atoi(argv[argi++]);
    if ( argi < argc )
	band_width = atoi(argv[argi++]);

    assert( band_width <= decode_rate );

    unsigned int sample_rate = ss.rate;

    if ( isatty(1) )
	pa_ba.fragsize = sample_rate / decode_rate;

    /* Initiate the capture stream */
    int error;
    pa_simple *s;
    s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL,
	    "text spectrum scope",
	    &ss, NULL, &pa_ba, &error);
	    // &ss, NULL, NULL, &error);
    if ( !s ) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n",
		pa_strerror(error));
        return 1;
    }

    int pa_samplesize = pa_sample_size(&ss);
    int pa_framesize = pa_frame_size(&ss);
    int pa_nchannels = ss.channels;

    assert( pa_samplesize == sizeof(float) );
    assert( pa_framesize == pa_samplesize * pa_nchannels );

    /* Create the FFT plan */
    fftwf_plan	fftplan;
    int fftsize = sample_rate / band_width;
    if ( fftsize & 1 )
        fprintf(stderr, __FILE__": WARNING: fftsize %u is not even\n", fftsize);
    unsigned int nbands = fftsize / 2 + 1;

    float *fftin = fftwf_malloc(fftsize * sizeof(float) * pa_nchannels);

    fftwf_complex *fftout = fftwf_malloc(nbands *
				    sizeof(fftwf_complex) * pa_nchannels);

    /* basic fftw plan, works for only 1 channel:
    fftplan = fftwf_plan_dft_r2c_1d(fftsize, fftin, fftout, FFTW_ESTIMATE);
     */
    /* complex fftw plan, works for N channels: */
    fftplan = fftwf_plan_many_dft_r2c(
	    /*rank*/1, &fftsize, /*howmany*/pa_nchannels,
	    fftin, NULL, /*istride*/pa_nchannels, /*idist*/1,
	    fftout, NULL, /*ostride*/1, /*odist*/nbands,
	    FFTW_ESTIMATE);

    if ( !fftplan ) {
        fprintf(stderr, __FILE__": fftwf_plan_dft_r2c_1d() failed\n");
        return 1;
    }

    /* Calculate the input sample chunk rate */
    int		nsamples = sample_rate / decode_rate;
    size_t	nframes = nsamples;
    size_t	nbytes = nframes * pa_framesize;

    /* Calculate the fftw output normalization factor */
    float magscalar = 1.0 / (nsamples/2.0);

# if 0
    float actual_decode_rate = (float)sample_rate / nsamples;
    fprintf(stderr, "### baud=%.2f ###\n", actual_decode_rate);
# endif

    /* Prepare the text scope output buffer */
    // sadly, COLUMNS is not exported by default (?)
    char *columns_env = getenv("COLUMNS");
    int columns = columns_env ? atoi(columns_env) : 80;
    int show_nbands = ( (columns - 2 - 10) / pa_nchannels ) - 1;
    if ( show_nbands > nbands )
         show_nbands = nbands;
    char *magline = malloc(show_nbands+1);

    /*
     * Run the main loop
     */
    while ( 1 )
    {
	// for possible future use...
	//   bzero(fftin, (fftsize * sizeof(float) * pa_nchannels));

	/* read a chunk of input sample frames (directly into the
	 * FFT input buffer) */
        if (pa_simple_read(s, fftin, nbytes, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n",
		    pa_strerror(error));
	    return(1);
        }

	/* run the FFT to compute the spectrum (for all pa_nchannels) */
	fftwf_execute(fftplan);

	/* display the spectrum magnitudes for each channel */
	int n;
	for ( n=0; n<pa_nchannels; n++ )
	    tscope_print(fftout+n*nbands, show_nbands, magscalar,
				one_line_mode, show_maxmag);
	printf( one_line_mode ? "\r" : "\n" );
	fflush(stdout);
    }

    /* Clean up */
    free(magline);
    pa_simple_free(s);
    fftwf_free(fftin);
    fftwf_free(fftout);
    fftwf_destroy_plan(fftplan);

    return 0;
}
