/*
 * fsk.c
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


#include <malloc.h>
#include <string.h>
#include <math.h>	// fabs, hypotf
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

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
static float
fsk_frame_analyze( fsk_plan *fskp, float *samples, float samples_per_bit,
	unsigned int *bits_outp )
{
    int n_bits = fskp->n_frame_bits;
//    char *expect_bit_string = "10dddddddd1";
    char expect_bit_string[32];
    memset(expect_bit_string, 'd', 32);
    expect_bit_string[0] = '1';
    expect_bit_string[1] = '0';
    expect_bit_string[fskp->n_data_bits + 2] = '1';

    // example...
    // 0123456789A
    // isddddddddp	i == idle bit (a.k.a. prev_stop bit)
    //			s == start bit
    //			d == data bits
    //			p == stop bit
    // MSddddddddM  <-- expected mark/space framing pattern

    unsigned int bit_nsamples = (float)(samples_per_bit + 0.5);

    unsigned int	bit_values[32];
    float		bit_strengths[32];
    unsigned int	bit_begin_sample;
    int			bitnum;

    float		v = 0;

    char *expect_bits = expect_bit_string;

    /* pass #1 - process and check only the "required" (1/0) expect_bits */
    for ( bitnum=0; bitnum<n_bits; bitnum++ ) {
	if ( expect_bits[bitnum] == 'd' )
	    continue;
	assert( expect_bits[bitnum] == '1' || expect_bits[bitnum] == '0' );

	bit_begin_sample = (float)(samples_per_bit * bitnum + 0.5);
	debug_log( " bit# %2u @ %7u: ", bitnum, bit_begin_sample);
	fsk_bit_analyze(fskp, samples+bit_begin_sample, bit_nsamples,
		&bit_values[bitnum], &bit_strengths[bitnum]);

	if ( (expect_bits[bitnum] - '0') != bit_values[bitnum] )
	    return 0.0; /* does not match expected; abort frame analysis. */

	v += bit_strengths[bitnum];
    }

// Note: CONFIDENCE_ALGO 3 does not need AVOID_TRANSIENTS
// #define AVOID_TRANSIENTS	0.7
//
#ifdef AVOID_TRANSIENTS
    /* Compare strength of stop bit and start bit, to avoid detecting
     * a transient as a start bit, as often results in a single false
     * character when the mark "leader" tone begins.  Require that the
     * diff between start bit and stop bit strength not be "large". */
    //float i_str = bit_strengths[0];
    float s_str = bit_strengths[1];
    float p_str = bit_strengths[fskp->n_data_bits + 2];
    if ( fabs(s_str-p_str) > (s_str * AVOID_TRANSIENTS) ) {
	debug_log(" avoid transient\n");
	return 0.0;
    }
#endif

    /* pass #2 - process only the dontcare ('d') expect_bits */
    for ( bitnum=0; bitnum<n_bits; bitnum++ ) {
	if ( expect_bits[bitnum] != 'd' )
	    continue;
	bit_begin_sample = (float)(samples_per_bit * bitnum + 0.5);
	debug_log( " bit# %2u @ %7u: ", bitnum, bit_begin_sample);
	fsk_bit_analyze(fskp, samples+bit_begin_sample, bit_nsamples,
		&bit_values[bitnum], &bit_strengths[bitnum]);
	v += bit_strengths[bitnum];
    }

    /* compute average bit strength 'v' */
    v /= n_bits;

#define FSK_MIN_STRENGTH	0.005
    if ( v < FSK_MIN_STRENGTH )
	return 0.0;


#define CONFIDENCE_ALGO	3

    float worst_divergence = 0;
    int i;
    for ( i=0; i<n_bits; i++ ) {
	float normalized_bit_str = bit_strengths[i] / v;
	float divergence = fabs(1.0 - normalized_bit_str);
	if ( worst_divergence < divergence )
	    worst_divergence = divergence;
    }
    float confidence = 1.0 - worst_divergence;
    if ( confidence <= 0.0 )
	return 0.0;


    *bits_outp = 0;
    for ( i=0; i<n_bits; i++ )
	*bits_outp |= bit_values[i] << i;

    debug_log("    frame confidence (algo #%u) = %f\n",
	    CONFIDENCE_ALGO, confidence);
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

    if ( confidence == 0 )
	return 0;

    // FIXME? hardcoded chop off framing bits for debug
#ifdef FSK_DEBUG
    unsigned char bitchar = ( best_bits >> 2 ) & 0xFF;
    debug_log("FSK_FRAME datum='%c' (0x%02x)   c=%f  t=%d\n",
	    isprint(bitchar)||isspace(bitchar) ? bitchar : '.',
	    bitchar,
	    confidence, best_t);
#endif

    return confidence;
}

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

