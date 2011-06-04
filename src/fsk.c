#define USE_FFT

#include <malloc.h>
#include <string.h>
#include <math.h>	// fabs, hypotf
#ifdef USE_FFT
#include <fftw3.h>
#endif

#include <errno.h>

#include <stdio.h>
#include <ctype.h>


//#define FSK_DEBUG

#ifdef FSK_DEBUG
# define debug_log(format, args...)  fprintf(stderr, format, ## args)
#else
# define debug_log(format, args...)
#endif


typedef struct fsk_plan fsk_plan;

struct fsk_plan {
	float		sample_rate;
    	float		f_mark;
    	float		f_space;
	float		filter_bw;
	unsigned int	n_data_bits;
#ifdef USE_FFT
	int		fftsize;	// fftw wants this to be signed. why?
	unsigned int	b_mark;
	unsigned int	b_space;
	fftwf_plan	fftplan;
	float		*fftin;
	fftwf_complex	*fftout;
#endif
};


fsk_plan *
fsk_plan_new(
	float		sample_rate,
    	float		f_mark,
    	float		f_space,
	float		filter_bw,
	unsigned int	n_data_bits
	)
{
    fsk_plan *fskp = malloc(sizeof(fsk_plan));
    if ( !fskp )
	return NULL;

    fskp->sample_rate = sample_rate;
    fskp->f_mark = f_mark;
    fskp->f_space = f_space;
    fskp->filter_bw = filter_bw;
    fskp->n_data_bits = n_data_bits;

#ifdef USE_FFT
    unsigned int fft_bw = filter_bw;

    float fft_half_bw = (float)fft_bw / 2.0;
    fskp->fftsize = (sample_rate + fft_half_bw) / fft_bw;
    unsigned int nbands = fskp->fftsize / 2 + 1;

    fskp->b_mark  = (f_mark  + fft_half_bw) / fft_bw;
    fskp->b_space = (f_space + fft_half_bw) / fft_bw;
    if ( fskp->b_mark >= nbands || fskp->b_space >= nbands ) {
        fprintf(stderr, "b_mark=%u or b_space=%u is invalid (nbands=%u)\n",
		fskp->b_mark, fskp->b_space, nbands);
	free(fskp);
	errno = EINVAL;
	return NULL;
    }
    debug_log("### b_mark=%u b_space=%u fftsize=%u\n",
	    fskp->b_mark, fskp->b_space, fskp->fftsize);


    // FIXME:
    unsigned int pa_nchannels = 1;

    // FIXME check these:
    fskp->fftin  = fftwf_malloc(fskp->fftsize * sizeof(float) * pa_nchannels);
    fskp->fftout = fftwf_malloc(nbands * sizeof(fftwf_complex) * pa_nchannels);

    /* complex fftw plan, works for N channels: */
    fskp->fftplan = fftwf_plan_many_dft_r2c(
	    /*rank*/1, &fskp->fftsize, /*howmany*/pa_nchannels,
	    fskp->fftin, NULL, /*istride*/pa_nchannels, /*idist*/1,
	    fskp->fftout, NULL, /*ostride*/1, /*odist*/nbands,
	    FFTW_ESTIMATE);

    if ( !fskp->fftplan ) {
        fprintf(stderr, "fftwf_plan_dft_r2c_1d() failed\n");
	fftwf_free(fskp->fftin);
	fftwf_free(fskp->fftout);
	free(fskp);
	errno = EINVAL;
        return NULL;
    }
#endif

    return fskp;
}

void
fsk_plan_destroy( fsk_plan *fskp )
{
    fftwf_free(fskp->fftin);
    fftwf_free(fskp->fftout);
    fftwf_destroy_plan(fskp->fftplan);
    free(fskp);
}


static inline float
band_mag( fftwf_complex * const cplx, unsigned int band, float scalar )
{
    float re = cplx[band][0];
    float im = cplx[band][1];
    float mag = hypotf(re, im) * scalar;
    return mag;
}


static void
fsk_bit_analyze( fsk_plan *fskp, float *samples, unsigned int bit_nsamples,
	float *mag_mark_outp, float *mag_space_outp)
{
    unsigned int pa_nchannels = 1;
    bzero(fskp->fftin, (fskp->fftsize * sizeof(float) * pa_nchannels));
    memcpy(fskp->fftin, samples, bit_nsamples * sizeof(float));
    fftwf_execute(fskp->fftplan);
    float magscalar = 1.0 / ((float)bit_nsamples/2.0);
    *mag_mark_outp  = band_mag(fskp->fftout, fskp->b_mark,  magscalar);
    *mag_space_outp = band_mag(fskp->fftout, fskp->b_space, magscalar);
    debug_log( "\t%.2f  %.2f  %s  sig=%.2f\n",
	    *mag_mark_outp, *mag_space_outp,
	    *mag_mark_outp > *mag_space_outp ? "mark      " : "     space",
	    fabs(*mag_mark_outp - *mag_space_outp)
	    );
}


static float
fsk_frame_decode( fsk_plan *fskp, float *samples, unsigned int frame_nsamples,
	unsigned int *bits_outp )
{
    float v = 0;

    /* 1 prev_stop + n_data_bits + 1 start + 1 stop == n_data_bits + 3 */
    float samples_per_bit = (float)frame_nsamples / (fskp->n_data_bits + 3);

    unsigned int bit_nsamples = (float)(samples_per_bit + 0.5);

    // 0123456789A
    // isddddddddp	i == idle bit (a.k.a. previous stop bit)
    //			s == start bit
    //			d == data bits
    //			p == stop bit
    // MSddddddddM  <-- expected mark/space framing pattern

    unsigned int begin_i_idlebit = 0;
    unsigned int begin_s_startbit = (float)(samples_per_bit * 1 + 0.5);
    unsigned int begin_p_stopbit  = (float)(samples_per_bit * 10 + 0.5);

    /*
     * To optimize performance for a streaming scenario, check start bit first,
     * then stop, then idle bits...  we're "searching" for start, must validate
     * stop, and finally we want to to collect idle's v value.  After all that
     * collect the n_data_bits
     */

    float sM, sS, pM, pS, iM, iS;

    unsigned int bit;

debug_log("\t\tstart   ");
    fsk_bit_analyze(fskp, samples+begin_s_startbit, bit_nsamples, &sM, &sS);
    bit = sM > sS;
    if ( bit != 0 )
	return 0.0;
    v += sS - sM;

debug_log("\t\tstop    ");
    fsk_bit_analyze(fskp, samples+begin_p_stopbit,  bit_nsamples, &pM, &pS);
    bit = pM > pS;
    if ( bit != 1 )
	return 0.0;
    v += pM - pS;

debug_log("\t\tidle    ");
    fsk_bit_analyze(fskp, samples+begin_i_idlebit,  bit_nsamples, &iM, &iS);
    bit = iM > iS;
    if ( bit != 1 )
	return 0.0;
    v += iM - iS;

    unsigned int bits_out = 0;
    int i;
    for ( i=0; i<fskp->n_data_bits; i++ ) {
	unsigned int begin = (float)(samples_per_bit * (i+2) + 0.5);
	float dM, dS;
debug_log("\t\tdata    ");
	fsk_bit_analyze(fskp, samples+begin, bit_nsamples, &dM, &dS);
	bit = dM > dS;
	v += fabs(dM - dS);
	bits_out |= bit << i;
    }
    *bits_outp = bits_out;

debug_log( "v=%f\n", v );
    return v;
}


int
fsk_find_frame( fsk_plan *fskp, float *samples, unsigned int frame_nsamples,
	unsigned int try_max_nsamples,
	unsigned int try_step_nsamples,
	unsigned int *bits_outp
	)
{
    unsigned int t;
    unsigned int best_t = 0;
    float best_v = 0.0;
    unsigned int best_bits = 0;
    for ( t=0; t<try_max_nsamples; t+=try_step_nsamples )
    {
	float v;
	unsigned int bits_out = 0;
    debug_log("try fsk_frame_decode(skip=%+d)\n", t);
	v = fsk_frame_decode(fskp, samples+t, frame_nsamples, &bits_out);
	if ( best_v < v ) {
	    best_t = t;
	    best_v = v;
	    best_bits = bits_out;
	}
    }
#define FSK_MINV	(11 * 0.2)
    if ( best_v <= FSK_MINV ) {
	debug_log( "no frame\n" );
	return -1;
    }

    debug_log("DATA '%c' @ v=%f t=%d\n",
	    isprint(best_bits)||isspace(best_bits) ? best_bits : '.',
	    best_v, best_t);

    *bits_outp = best_bits;

    return best_t;
}

/****************************************************************************/


#include <assert.h>
#include <stdlib.h>

#include "simpleaudio.h"
#include "tscope_print.h"

int
main( int argc, char*argv[] )
{

    int ret = 1;

    if ( argc < 2 ) {
	fprintf(stderr, "usage: fsk [filename] baud_rate [ mark_hz space_hz ]\n");
	return 1;
    }

    unsigned char textscope = 0;
    int argi = 1;
    if ( argi < argc && strcmp(argv[argi],"-s") == 0 )  {
	textscope = 1;
	argi++;
    }

    simpleaudio *sa;

    char *p;
    for ( p=argv[argi]; *p; p++ )
	if ( !isdigit(*p) )
	    break;
    if ( *p ) {
	sa = simpleaudio_open_source_sndfile(argv[argi]);
	argi++;
    } else {
	sa = simpleaudio_open_source_pulseaudio(argv[0], "bfsk demodulator");
    }
    if ( !sa )
        return 1;

    unsigned int sample_rate = simpleaudio_get_rate(sa);
//    unsigned int nchannels = simpleaudio_get_channels(sa);


    unsigned int decode_rate;
    decode_rate = atoi(argv[argi++]);

    unsigned int band_width;
    band_width = decode_rate;

    /*
     * Bell 103:     baud=300 mark=1270 space=1070
     * ITU-T V.21:   baud=300 mark=1280 space=1080
     */
    unsigned int bfsk_mark_f  = 1270;
    unsigned int bfsk_space_f = 1070;
    // band_width = 10;
    band_width = 100;	/* close enough */

    /*
     * Bell 202:     baud=1200 mark=1200 space=2200
     */
    if ( decode_rate >= 400 ) {
	bfsk_mark_f  = 1200;
	bfsk_space_f = 2200;
	band_width = 200;
    }

    if ( argi < argc ) {
	assert(argc-argi == 2);
	bfsk_mark_f = atoi(argv[argi++]);
	bfsk_space_f = atoi(argv[argi++]);
    }

    /*
     * Prepare the input sample chunk rate
     */
    int nsamples_per_bit = sample_rate / decode_rate;


    /*
     * Run the main loop
     */

    ret = 0;





    fsk_plan *fskp = fsk_plan_new(sample_rate,
				    bfsk_mark_f, bfsk_space_f,
				    band_width, 8);

    if ( !fskp ) {
        fprintf(stderr, "fsk_plan_new() failed\n");
        return 1;
    }


    size_t	fill_nsamples = nsamples_per_bit * 12;

    size_t	buf_nsamples = fill_nsamples;
    float	*samples = malloc(buf_nsamples * sizeof(float));

    size_t	read_nsamples = fill_nsamples;
    float	*read_bufptr = samples;

    int carrier = 0;

    while ( 1 ) {

	debug_log( "@read samples+%ld n=%lu\n",
		read_bufptr - samples, read_nsamples);

	if ((ret=simpleaudio_read(sa, read_bufptr, read_nsamples)) <= 0)
            break;
//	carrier_nsamples += read_nsamples;


	unsigned int frame_nsamples = nsamples_per_bit * 11;

	unsigned int bits = 0;

	unsigned int try_step_nsamples = nsamples_per_bit / 4;
	if ( try_step_nsamples == 0 )
	    try_step_nsamples = 1;


debug_log( "--------------------------\n");
	int t =
	fsk_find_frame(fskp, samples, frame_nsamples,
			/*try_max_nsamples*/ nsamples_per_bit,
			try_step_nsamples,
			&bits
			);

	if ( t <= 0 ) {
	    if ( carrier )
		fprintf(stderr, "\n### NOCARRIER ###\n");
	    carrier = 0;

	    t = nsamples_per_bit;

	} else {
	    if ( !carrier )
		fprintf(stderr, "\n### CARRIER ###\n");
	    carrier = 1;

	    // t += nsamples_per_bit * 10;
	    t += nsamples_per_bit * 9.5;
	    debug_log( "@ t=%u\n", t);

	    char the_byte = isprint(bits)||isspace(bits) ? bits : '.';
	    printf( "%c", the_byte );
	    fflush(stdout);

	} 

	memmove(samples, samples+t, (fill_nsamples-t)*sizeof(float));
	read_bufptr = samples + (fill_nsamples-t);
	read_nsamples = t;

	assert ( read_nsamples <= buf_nsamples );
	assert ( read_nsamples > 0 );
	
    }

    if ( ret != 0 )
	fprintf(stderr, "simpleaudio_read: error\n");

    simpleaudio_close(sa);

    fsk_plan_destroy(fskp);

    return ret;
}
