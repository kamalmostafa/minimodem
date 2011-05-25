/*
 * miniscope.c
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
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <math.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include <fftw3.h>


static inline
float
band_mag( fftwf_complex * const cplx, unsigned int band, float scalar )
{
    float re = cplx[band][0];
    float im = cplx[band][1];
    float mag = sqrtf(re*re + im*im) * scalar;
    return mag;
}

int main(int argc, char*argv[]) {
    /* The sample type to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_FLOAT32,
        .rate = 9600,	// pulseaudio will resample its configured audio rate

        .channels = 2	// 2 channel stereo
        // .channels = 1	// pulseaudio will downmix (additively) to 1 channel
        // .channels = 3	// 2 channel stereo + 1 mixed channel
    };
    int ret = 1;
    int error;


    if ( argc < 2 ) {
	fprintf(stderr, "usage: miniscope baud_rate [ band_width ]\n");
	return 1;
    }



    int argi = 1;
    unsigned int decode_rate = atoi(argv[argi++]);
    unsigned int band_width = decode_rate;
    if ( argi < argc )
	band_width = atoi(argv[argi++]);
    assert( band_width <= decode_rate );

    unsigned int sample_rate = ss.rate;



    /* Create the recording stream */
    pa_simple *s;
    s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error);
    if ( !s ) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        return 1;
    }

    int pa_samplesize = pa_sample_size(&ss);
    int pa_framesize = pa_frame_size(&ss);
    int pa_nchannels = ss.channels;

    assert( pa_framesize == pa_samplesize * pa_nchannels );


    /* Create the FFT plan */
    fftwf_plan	fftplan;

    int fftsize = sample_rate / band_width;

    if ( fftsize & 1 )
        fprintf(stderr, __FILE__": WARNING: fftsize %u is not even\n", fftsize);

    unsigned int nbands = fftsize / 2 + 1;

    float *fftin = fftwf_malloc(fftsize * sizeof(float) * pa_nchannels);

    fftwf_complex *fftout = fftwf_malloc(nbands * sizeof(fftwf_complex) * pa_nchannels);

    /*
     * works only for 1 channel:
    fftplan = fftwf_plan_dft_r2c_1d(fftsize, fftin, fftout, FFTW_ESTIMATE);
     */
    /*
     * works for N channels:
     */
    fftplan = fftwf_plan_many_dft_r2c(
	    /*rank*/1, &fftsize, /*howmany*/pa_nchannels,
	    fftin, NULL, /*istride*/pa_nchannels, /*idist*/1,
	    fftout, NULL, /*ostride*/1, /*odist*/nbands,
	    FFTW_ESTIMATE | FFTW_PRESERVE_INPUT );
    /* Nb. FFTW_PRESERVE_INPUT is needed for the "shift the input window" trick */

    if ( !fftplan ) {
        fprintf(stderr, __FILE__": fftwf_plan_dft_r2c_1d() failed\n");
        return 1;
    }


    void *pa_samples_in = fftin;	// read samples directly into fftin
    assert( pa_samplesize == sizeof(float) );


    /*
     * Prepare the input sample chunk rate
     */
    int nsamples = sample_rate / decode_rate;

    // float magscalar = 1.0 / (fftsize/2.0); /* normalize fftw output */
    float magscalar = 1.0 / (nsamples/2.0); /* normalize fftw output */


    float actual_decode_rate = (float)sample_rate / nsamples;
    fprintf(stderr, "### baud=%.2f ###\n", actual_decode_rate);


    /*
     * Run the main loop
     */

    ret = 0;

    while ( 1 ) {

	size_t	nframes = nsamples;
	size_t	nbytes = nframes * pa_framesize;

	bzero(fftin, (fftsize * sizeof(float) * pa_nchannels));
        if (pa_simple_read(s, pa_samples_in, nbytes, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n",
		    pa_strerror(error));
	    ret = 1;
            break;
        }

	float inmax=0, inmin=0;
	int i;
	for ( i=0; i<fftsize; i++ ) {
//	    if ( fftin[i] > 1.0 || fftin[i] < -1.0 )
//		fprintf(stderr, __FILE__": WARNING input datum %.3f\n", fftin[i]);
	    if ( inmin > fftin[i] )
		 inmin = fftin[i];
	    if ( inmax < fftin[i] )
		 inmax = fftin[i];
	}


	fftwf_execute(fftplan);

	{
	    float magmax = 0;

	    for ( i=0; i<nbands*pa_nchannels; i++ ) {
		if ( i%nbands == 0 )
		    printf("|");
		float mag = band_mag(fftout, i, magscalar);
		if ( mag > magmax )
		    magmax = mag;
		char *magchars = " .-=^";
		char c = magchars[0];
		if ( mag > 0.10 ) c = magchars[1];
		if ( mag > 0.25 ) c = magchars[2];
		if ( mag > 0.50 ) c = magchars[3];
		if ( mag > 1.00 ) c = magchars[4];
		printf("%c", c);

		// if ( i > 70 )
		//    break;
	    }
	    printf("|");
	    // printf("in %+4.2f %+4.2f", inmin, inmax);
	    printf(">mag %.2f", magmax);
	    printf("\n");
	}

    }

    pa_simple_free(s);

    fftwf_free(fftin);
    fftwf_free(fftout);
    fftwf_destroy_plan(fftplan);

    return ret;
}
