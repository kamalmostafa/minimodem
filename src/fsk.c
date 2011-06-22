/*
 * fsk.c
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
 */


#include <malloc.h>
#include <string.h>
#include <math.h>	// fabs, hypotf
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "fsk.h"


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
    fskp->n_data_bits = n_data_bits;

    /* 1 prev_stop + n_data_bits + 1 start + 1 stop == n_data_bits + 3 */
    fskp->n_frame_bits = fskp->n_data_bits + 3;

#ifdef USE_FFT
    fskp->band_width = filter_bw;

    float fft_half_bw = fskp->band_width / 2.0;
    fskp->fftsize = (sample_rate + fft_half_bw) / fskp->band_width;
    fskp->nbands = fskp->fftsize / 2 + 1;

    fskp->b_mark  = (f_mark  + fft_half_bw) / fskp->band_width;
    fskp->b_space = (f_space + fft_half_bw) / fskp->band_width;
    if ( fskp->b_mark >= fskp->nbands || fskp->b_space >= fskp->nbands ) {
        fprintf(stderr, "b_mark=%u or b_space=%u is invalid (nbands=%u)\n",
		fskp->b_mark, fskp->b_space, fskp->nbands);
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
    fskp->fftout = fftwf_malloc(fskp->nbands * sizeof(fftwf_complex) * pa_nchannels);

    /* complex fftw plan, works for N channels: */
    fskp->fftplan = fftwf_plan_many_dft_r2c(
	    /*rank*/1, &fskp->fftsize, /*howmany*/pa_nchannels,
	    fskp->fftin, NULL, /*istride*/pa_nchannels, /*idist*/1,
	    fskp->fftout, NULL, /*ostride*/1, /*odist*/fskp->nbands,
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
	unsigned int *bit_outp, float *bit_strength_outp)
{
    unsigned int pa_nchannels = 1;	// FIXME
    bzero(fskp->fftin, (fskp->fftsize * sizeof(float) * pa_nchannels));
    memcpy(fskp->fftin, samples, bit_nsamples * sizeof(float));
    fftwf_execute(fskp->fftplan);
    float magscalar = 1.0 / ((float)bit_nsamples/2.0);
    float mag_mark  = band_mag(fskp->fftout, fskp->b_mark,  magscalar);
    float mag_space = band_mag(fskp->fftout, fskp->b_space, magscalar);
    *bit_outp = mag_mark > mag_space ? 1 : 0;	// mark==1, space==0
    *bit_strength_outp = fabs(mag_mark - mag_space);
    debug_log( "\t%.2f  %.2f  %s  bit=%u bit_strength=%.2f\n",
	    mag_mark, mag_space,
	    mag_mark > mag_space ? "mark      " : "     space",
	    *bit_outp, *bit_strength_outp);
}


/* returns confidence value [0.0 to 1.0] */
static void
fsk_bits_analyze( fsk_plan *fskp, float *samples, float samples_per_bit,
	unsigned int *bits_outp, float *bit_strengths_outp )
{
    unsigned int bit_nsamples = (float)(samples_per_bit + 0.5);
    unsigned int nbits = fskp->n_data_bits;

    unsigned int bits_out = 0;
    int i;
    for ( i=0; i<nbits; i++ ) {
	unsigned int begin_databit = (float)(samples_per_bit * i + 0.5);
	unsigned int bit;
	debug_log("\t\tdata  begin_data_bit=%u  ", begin_databit);
	fsk_bit_analyze(fskp, samples+begin_databit, bit_nsamples,
			    &bit, &bit_strengths_outp[i]);
	bits_out |= bit << i;
    }
    *bits_outp = bits_out;
}


/* returns confidence value [0.0 to 1.0] */
static float
fsk_frame_analyze( fsk_plan *fskp, float *samples, float samples_per_bit,
	unsigned int *bits_outp )
{
    float v = 0;

    unsigned int bit_nsamples = (float)(samples_per_bit + 0.5);

    // 0123456789A
    // isddddddddp	i == idle bit (a.k.a. prev_stop bit)
    //			s == start bit
    //			d == data bits
    //			p == stop bit
    // MSddddddddM  <-- expected mark/space framing pattern

    unsigned int begin_idlebit = 0;
    unsigned int begin_startbit = (float)(samples_per_bit * 1 + 0.5);
    unsigned int begin_databits = (float)(samples_per_bit * 2 + 0.5);
    unsigned int begin_stopbit  = (float)(samples_per_bit * (fskp->n_data_bits+2) + 0.5);

    /*
     * To optimize performance for a streaming scenario, check start bit first,
     * then stop, then idle bits...  we're "searching" for start, must validate
     * stop, and finally we want to to collect idle's v value.  After all that
     * collect the n_data_bits
     */

    float s_str, p_str, i_str;
    unsigned int bit;

    debug_log("\t\tstart   ");
    fsk_bit_analyze(fskp, samples+begin_startbit, bit_nsamples, &bit, &s_str);
    if ( bit != 0 )
	return 0.0;
    v += s_str;

    debug_log("\t\tstop    ");
    fsk_bit_analyze(fskp, samples+begin_stopbit, bit_nsamples, &bit, &p_str);
    if ( bit != 1 )
	return 0.0;
    v += p_str;

#define AVOID_TRANSIENTS	0.7
#ifdef AVOID_TRANSIENTS
    /* Compare strength of stop bit and start bit, to avoid detecting
     * a transient as a start bit, as often results in a single false
     * character when the mark "leader" tone begins.  Require that the
     * diff between start bit and stop bit strength not be "large". */
    if ( fabs(s_str-p_str) > (s_str * AVOID_TRANSIENTS) ) {
	debug_log("avoid_transient\n");
	return 0.0;
    }
#endif

    debug_log("\t\tidle    ");
    fsk_bit_analyze(fskp, samples+begin_idlebit, bit_nsamples, &bit, &i_str);
    if ( bit != 1 )
	return 0.0;
    v += i_str;

    // Analyze the actual data payload bits
    float databit_strengths[32];
    fsk_bits_analyze(fskp, samples+begin_databits, samples_per_bit,
			bits_outp, databit_strengths);

    // computer average bit strength 'v'
    int i;
    for ( i=0; i<fskp->n_data_bits; i++ )
	v += databit_strengths[i];
    v /= (fskp->n_data_bits + 3);

#define FSK_MIN_STRENGTH	0.05
    if ( v < FSK_MIN_STRENGTH )
	return 0.0;

    float confidence = 0;
    confidence += 1.0 - fabs(i_str - v);
    confidence += 1.0 - fabs(s_str - v);
    confidence += 1.0 - fabs(p_str - v);
    for ( i=0; i<fskp->n_data_bits; i++ )
	confidence += 1.0 - fabs(databit_strengths[i] - v);
    confidence /= (fskp->n_data_bits + 3);

    return confidence;
}

/* returns confidence value [0.0 to 1.0] */
float
fsk_find_frame( fsk_plan *fskp, float *samples, unsigned int frame_nsamples,
	unsigned int try_max_nsamples,
	unsigned int try_step_nsamples,
	unsigned int *bits_outp,
	unsigned int *frame_start_outp
	)
{
    float samples_per_bit = (float)frame_nsamples / fskp->n_frame_bits;

    unsigned int t;
    unsigned int best_t = 0;
    float best_c = 0.0;
    unsigned int best_bits = 0;
    for ( t=0; t<(try_max_nsamples+try_step_nsamples); t+=try_step_nsamples )
    {
	float c;
	unsigned int bits_out = 0;
	debug_log("try fsk_frame_analyze(skip=%+d)\n", t);
	c = fsk_frame_analyze(fskp, samples+t, samples_per_bit, &bits_out);
	if ( best_c < c ) {
	    best_t = t;
	    best_c = c;
	    best_bits = bits_out;
	}
    }

    *bits_outp = best_bits;
    *frame_start_outp = best_t;

    float confidence = best_c;

    debug_log("FSK_FRAME datum='%c' (0x%02x)   c=%f  t=%d\n",
	    isprint(best_bits)||isspace(best_bits) ? best_bits : '.',
	    best_bits,
	    confidence, best_t);

    return confidence;
}

#include <assert.h>		// FIXME

// #define FSK_AUTODETECT_MIN_FREQ		600
// #define FSK_AUTODETECT_MAX_FREQ		5000

int
fsk_detect_carrier(fsk_plan *fskp, float *samples, unsigned int nsamples,
	float min_mag_threshold )
{
    assert( nsamples <= fskp->fftsize );

    unsigned int pa_nchannels = 1;	// FIXME
    bzero(fskp->fftin, (fskp->fftsize * sizeof(float) * pa_nchannels));
    memcpy(fskp->fftin, samples, nsamples * sizeof(float));
    fftwf_execute(fskp->fftplan);
    float magscalar = 1.0 / ((float)nsamples/2.0);
    float max_mag = 0.0;
    int max_mag_band = -1;
    int i = 1;	/* start detection at the first non-DC band */
    int nbands = fskp->nbands;
#ifdef FSK_AUTODETECT_MIN_FREQ
    i = (FSK_AUTODETECT_MIN_FREQ + (fskp->band_width/2))
			    / fskp->band_width;
#endif
#ifdef FSK_AUTODETECT_MAX_FREQ
    nbands = (FSK_AUTODETECT_MAX_FREQ + (fskp->band_width/2))
			    / fskp->band_width;
    if ( nbands > fskp->nbands )
	 nbands = fskp->nbands:
#endif
    for ( ; i<nbands; i++ ) {
	float mag = band_mag(fskp->fftout, i,  magscalar);
	if ( mag < min_mag_threshold )
	    continue;
	if ( max_mag < mag ) {
	    max_mag = mag;
	    max_mag_band = i;
	}
    }
    if ( max_mag_band < 0 )
	return -1;

    return max_mag_band;
}


void
fsk_set_tones_by_bandshift( fsk_plan *fskp, unsigned int b_mark, int b_shift )
{
    assert( b_shift != 0 );
    assert( b_mark < fskp->nbands ); 

    int b_space = b_mark + b_shift;
    assert( b_space >= 0 );
    assert( b_space < fskp->nbands ); 

    fskp->b_mark = b_mark;
    fskp->b_space = b_space;
    fskp->f_mark = b_mark * fskp->band_width;
    fskp->f_space = b_space * fskp->band_width;
}

