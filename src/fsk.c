/*
 * fsk.c
 *
 * Copyright (C) 2011-2016 Kamal Mostafa <kamal@whence.com>
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


#include <stdlib.h>
#include <string.h>
#include <math.h>	// fabsf, hypotf
#include <float.h>	// FLT_EPSILON
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
	float		filter_bw
	)
{
    fsk_plan *fskp = malloc(sizeof(fsk_plan));
    if ( !fskp )
	return NULL;

    fskp->sample_rate = sample_rate;
    fskp->f_mark = f_mark;
    fskp->f_space = f_space;

#ifdef USE_FFT
    fskp->band_width = filter_bw;

    float fft_half_bw = fskp->band_width / 2.0f;
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
    debug_log("### b_mark=%u b_space=%u fftsize=%d\n",
	    fskp->b_mark, fskp->b_space, fskp->fftsize);


    // FIXME:
    unsigned int pa_nchannels = 1;

    // FIXME check these:
    fskp->fftin  = fftwf_malloc(fskp->fftsize * sizeof(float) * pa_nchannels);
    bzero(fskp->fftin, (fskp->fftsize * sizeof(float) * pa_nchannels));
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
	unsigned int *bit_outp,
	float *bit_signal_mag_outp,
	float *bit_noise_mag_outp
	)
{
    // FIXME: Fast and loose ... don't bzero fftin, just assume its only ever
    // been used for bit_nsamples so the remainder is still zeroed.  Sketchy.
    //
    // unsigned int pa_nchannels = 1;	// FIXME
    // bzero(fskp->fftin, (fskp->fftsize * sizeof(float) * pa_nchannels));

    memcpy(fskp->fftin, samples, bit_nsamples * sizeof(float));

    float magscalar = 2.0f / (float)bit_nsamples;

#if 0
    //// apodization window

    //// raised cosine windows
# if 0
    float a0=0.54, a1=(1.0f - a0);	// Hamming window
# else
    float a0=0.5, a1=0.5;		// Hann window
# endif

    magscalar /= a0;		// true for all raised cosine windows (I think)

    int i;
    for ( i=0; i<bit_nsamples; i++ ) {
	float zoff = 0.0; // 0.5  // FIXME which is it??
	unsigned int z = bit_nsamples /* not -1  ... explain */;
	float w = a0
		- a1 * cosf((2.0*M_PI*((float)i+zoff)) / z);
	fskp->fftin[i] *= w;
    }
#endif


    fftwf_execute(fskp->fftplan);
    float mag_mark  = band_mag(fskp->fftout, fskp->b_mark,  magscalar);
    float mag_space = band_mag(fskp->fftout, fskp->b_space, magscalar);
    // mark==1, space==0
    if ( mag_mark > mag_space ) {
	*bit_outp = 1;
	*bit_signal_mag_outp = mag_mark;
	*bit_noise_mag_outp = mag_space;
    } else {
	*bit_outp = 0;
	*bit_signal_mag_outp = mag_space;
	*bit_noise_mag_outp = mag_mark;
    }
    debug_log( "\t%.2f  %.2f  %s  bit=%u sig=%.2f noise=%.2f\n",
	    mag_mark, mag_space,
	    mag_mark > mag_space ? "mark      " : "     space",
	    *bit_outp, *bit_signal_mag_outp, *bit_noise_mag_outp);
}


/* returns confidence value [0.0 to INFINITY] */
static float
fsk_frame_analyze( fsk_plan *fskp, float *samples, float samples_per_bit,
	int n_bits, const char *expect_bits_string,
	unsigned long long *bits_outp, float *ampl_outp )
{
    unsigned int bit_nsamples = (float)(samples_per_bit + 0.5f);

    unsigned int	bit_values[64];
    float		bit_sig_mags[64];
    float		bit_noise_mags[64];
    unsigned int	bit_begin_sample;
    int			bitnum;

// various deprecated noise limiter schemes:
//#define FSK_MIN_BIT_SNR 1.4
//#define FSK_MIN_MAGNITUDE 0.10
//#define FSK_AVOID_TRANSIENTS	0.7

    const char *expect_bits = expect_bits_string;

    /* pass #1 - process and check only the "required" (1/0) expect_bits */
    for ( bitnum=0; bitnum<n_bits; bitnum++ ) {
	if ( expect_bits[bitnum] == 'd' )
	    continue;
	assert( expect_bits[bitnum] == '1' || expect_bits[bitnum] == '0' );

	bit_begin_sample = (float)(samples_per_bit * bitnum + 0.5f);
	debug_log( " bit# %2d @ %7u: ", bitnum, bit_begin_sample);
	fsk_bit_analyze(fskp, samples+bit_begin_sample, bit_nsamples,
		&bit_values[bitnum],
		&bit_sig_mags[bitnum],
		&bit_noise_mags[bitnum]);

	if ( (expect_bits[bitnum] - '0') != bit_values[bitnum] )
	    return 0.0; /* does not match expected; abort frame analysis. */

#ifdef FSK_MIN_BIT_SNR
	float bit_snr = bit_sig_mags[bitnum] / bit_noise_mags[bitnum];
	if ( bit_snr < FSK_MIN_BIT_SNR )
	    return 0.0;
#endif

# ifdef FSK_MIN_MAGNITUDE
	// Performance hack: reject frame early if sig mag isn't even half
	// of FSK_MIN_MAGNITUDE
	if ( bit_sig_mags[bitnum] < FSK_MIN_MAGNITUDE/2.0 )
	    return 0.0; // too weak; abort frame analysis
# endif
    }

#ifdef FSK_AVOID_TRANSIENTS
    // FIXME: fsk_frame_analyze shouldn't care about start/stop bits,
    // and this really is only correct for "10dd..dd1" format frames anyway:
    // FIXME: this is totally defective, if the checked bits weren't
    // even calculated in pass #1 (e.g. if there are no pass #1 expect bits).
    /* Compare strength of stop bit and start bit, to avoid detecting
     * a transient as a start bit, as often results in a single false
     * character when the mark "leader" tone begins.  Require that the
     * diff between start bit and stop bit strength not be "large". */
    float s_mag = bit_sig_mags[1]; // start bit
    float p_mag = bit_sig_mags[n_bits-1]; // stop bit
    if ( fabsf(s_mag-p_mag) > (s_mag * FSK_AVOID_TRANSIENTS) ) {
	debug_log(" avoid transient\n");
	return 0.0;
    }
#endif

    /* pass #2 - process only the dontcare ('d') expect_bits */
    for ( bitnum=0; bitnum<n_bits; bitnum++ ) {
	if ( expect_bits[bitnum] != 'd' )
	    continue;
	bit_begin_sample = (float)(samples_per_bit * bitnum + 0.5f);
	debug_log( " bit# %2d @ %7u: ", bitnum, bit_begin_sample);
	fsk_bit_analyze(fskp, samples+bit_begin_sample, bit_nsamples,
		&bit_values[bitnum],
		&bit_sig_mags[bitnum],
		&bit_noise_mags[bitnum]);

#ifdef FSK_MIN_BIT_SNR
	float bit_snr = bit_sig_mags[bitnum] / bit_noise_mags[bitnum];
	if ( bit_snr < FSK_MIN_BIT_SNR )
	    return 0.0;
#endif
    }


//#define CONFIDENCE_ALGO 	5
#define CONFIDENCE_ALGO 	6

    float confidence;

#if CONFIDENCE_ALGO == 5 || CONFIDENCE_ALGO == 6

    float total_bit_sig = 0.0, total_bit_noise = 0.0;
    float avg_mark_sig = 0.0, avg_space_sig = 0.0;
    unsigned int n_mark = 0, n_space = 0;
    for ( bitnum=0; bitnum<n_bits; bitnum++ ) {
	// Deal with floating point data type quantization noise...
	// If total_bit_noise <= FLT_EPSILON, then assume it to be 0.0,
	// so that we end up with snr==inf.
	total_bit_sig   += bit_sig_mags[bitnum];
	if ( bit_noise_mags[bitnum] > FLT_EPSILON )
	    total_bit_noise += bit_noise_mags[bitnum];

	if ( bit_values[bitnum] == 1 ) {
	    avg_mark_sig += bit_sig_mags[bitnum];
	    n_mark++;
	} else {
	    avg_space_sig += bit_sig_mags[bitnum];
	    n_space++;
	}
    }

    // Compute the "frame SNR"
    float snr = total_bit_sig / total_bit_noise;

    // Compute avg bit sig and noise magnitudes
    float avg_bit_sig   = total_bit_sig / n_bits;

    // Compute separate avg bit sig for mark and space
    if ( n_mark )
	avg_mark_sig /= n_mark;
    if ( n_space )
	avg_space_sig /= n_space;

#if CONFIDENCE_ALGO == 6
    // Compute average "divergence": bit_mag_divergence / other_bits_mag
    float divergence = 0.0;
    for ( bitnum=0; bitnum<n_bits; bitnum++ ) {
	float avg_bit_sig_other;
	avg_bit_sig_other = bit_values[bitnum] ? avg_mark_sig : avg_space_sig;
	divergence += fabsf(bit_sig_mags[bitnum] - avg_bit_sig_other)
					/ avg_bit_sig_other;
    }
    divergence *= 2;
    divergence /= n_bits;
#endif

#ifdef FSK_DEBUG
    float avg_bit_noise = total_bit_noise / n_bits;
    debug_log("    divg=%.3f snr=%.3f avg{bit_sig=%.3f bit_noise=%.3f(%s)}\n",
# if CONFIDENCE_ALGO == 6
	    divergence,
# else
	    0.0,
# endif
	    snr, avg_bit_sig, avg_bit_noise,
	    avg_bit_noise == 0.0 ? "zero" : "non-zero"
	    );
#endif

# ifdef FSK_MIN_MAGNITUDE
    if ( avg_bit_sig < FSK_MIN_MAGNITUDE )
	return 0.0; // too weak; reject frame
# endif

#if CONFIDENCE_ALGO == 6
    // Frame confidence is the frame ( SNR * consistency )
    confidence = snr * (1.0f - divergence);
#else
    // Frame confidence is the frame SNR
    confidence = snr;
#endif

    *ampl_outp = avg_bit_sig;

#elif CONFIDENCE_ALGO == 4

    /* compute average bit strengths: v_mark and v_space */
    /* compute average bit strength */
    /* don't consider the stop bit -- we'll look at it next time around */
    float v_mark=0.0, v_space=0.0, avg_bit_strength=0.0;
    unsigned int v_mark_count=0, v_space_count=0;
    for ( bitnum=0; bitnum<(n_bits-1); bitnum++ ) {
	if ( bit_values[bitnum] == 1 ) {
	    v_mark += bit_strengths[bitnum];
	    v_mark_count++;
	} else {
	    v_space += bit_strengths[bitnum];
	    v_space_count++;
	}
	avg_bit_strength += bit_strengths[bitnum];
    }
    if ( v_mark_count )
	v_mark /= v_mark_count;
    if ( v_space_count )
	v_space /= v_space_count;
    avg_bit_strength /= (n_bits-1);

#ifdef FSK_DEBUG
    debug_log("    bit_mag ");
    for ( bitnum=0; bitnum<n_bits-1; bitnum++ )
	debug_log("%.2f  ", bit_sig_mags[bitnum]);
    debug_log("\n");
    debug_log("    bit_str ");
    for ( bitnum=0; bitnum<n_bits-1; bitnum++ )
	debug_log("%.2f  ", bit_strengths[bitnum]);
    debug_log("\n");
#endif


    /* determine the worst bit strength divergence */
    float worst_divergence = 0;
    /* nbits-1: don't consider the stop bit */
    debug_log("mag_diverge ");
    for ( bitnum=0; bitnum<n_bits-1; bitnum++ )
    {
	float normalized_bit_str;
	if ( bit_values[bitnum] == 1 )
	    normalized_bit_str = bit_strengths[bitnum] - v_mark;
	else
	    normalized_bit_str = bit_strengths[bitnum] - v_space;
	debug_log("%.2f  ", normalized_bit_str);
	// float divergence = fabsf(1.0 - normalized_bit_str);
	float divergence = fabsf(normalized_bit_str);
	if ( worst_divergence < divergence )
	    worst_divergence = divergence;
    }
    debug_log("\n");

    debug_log("    avg_str=%.6f avg_mark=%.6f avg_space=%.6f, worst_div=%.6f\n",
	    avg_bit_strength, v_mark, v_space, worst_divergence);

    // confidence = (1.0 - worst_divergence) * avg_bit_strength;
    confidence = (1.0 - worst_divergence);
    if ( confidence <= 0.0 )
	return 0.0;

#else /* CONFIDENCE_ALGO 3 */

    /* compute average bit strength 'v' */
    /* don't consider the stop bit -- we'll look at it next time around */
    float v = 0.0;
    for ( bitnum=0; bitnum<(n_bits-1); bitnum++ )
	v += bit_strengths[bitnum];
    v /= (n_bits-1);

// #define FSK_MIN_STRENGTH	0.005
# ifdef FSK_MIN_STRENGTH
    if ( v < FSK_MIN_STRENGTH )
	return 0.0;
# endif

    float worst_divergence = 0;
    /* nbits-1: don't consider the stop bit */
    for ( bitnum=0; bitnum<n_bits-1; bitnum++ )
    {
	float normalized_bit_str = bit_strengths[bitnum] / v;
	float divergence = fabsf(1.0 - normalized_bit_str);
	if ( worst_divergence < divergence )
	    worst_divergence = divergence;
    }
    confidence = 1.0 - worst_divergence;
    if ( confidence <= 0.0 )
	return 0.0;

#endif /* CONFIDENCE_ALGO */


    // least significant bit first ... reverse the bits as we place them
    // into the bits_outp word.
    *bits_outp = 0;
    for ( bitnum=0; bitnum<n_bits; bitnum++ )
	*bits_outp |= (unsigned long long) bit_values[bitnum] << bitnum;

    debug_log("    frame algo=%d confidence=%f ampl=%f\n",
	    CONFIDENCE_ALGO, confidence, *ampl_outp);
    return confidence;
}

/* returns confidence value [0.0 to 1.0] */
float
fsk_find_frame( fsk_plan *fskp, float *samples, unsigned int frame_nsamples,
	unsigned int try_first_sample,
	unsigned int try_max_nsamples,
	unsigned int try_step_nsamples,
	float try_confidence_search_limit,
	const char *expect_bits_string,
	unsigned long long *bits_outp,
	float *ampl_outp,
	unsigned int *frame_start_outp
	)
{
    int expect_n_bits = strlen(expect_bits_string);

    float samples_per_bit = (float)frame_nsamples / expect_n_bits;

    // try_step_nsamples = 1;	// pedantic TEST

    unsigned int best_t = 0;
    float best_c = 0.0, best_a = 0.0;
    unsigned long long best_bits = 0;
    
    // Scan the frame positions starting with the one try_first_sample,
    // alternating between a step above that, a step below that, above, below,
    // and so on, until we've scanned the whole try_max_nsamples range.
    int j;
    for ( j=0; ; j++ )
    {
	int up = ( j % 2 ) ? 1 : -1;
	int t = try_first_sample + up*((j+1)/2)*try_step_nsamples;
	if ( t >= (int)try_max_nsamples )
	    break;
	if ( t < 0 )
	    continue;

	float c, ampl_out = 0.0;
	unsigned long long bits_out = 0;
	debug_log("try fsk_frame_analyze at t=%d\n", t);
	c = fsk_frame_analyze(fskp, samples+t, samples_per_bit,
			expect_n_bits, expect_bits_string,
			&bits_out, &ampl_out);
	if ( best_c < c ) {
	    best_t = t;
	    best_c = c;
	    best_a = ampl_out;
	    best_bits = bits_out;
	    // If we find a frame with confidence > try_confidence_search_limit
	    // quit searching.
	    if ( best_c >= try_confidence_search_limit )
		break;
	}
    }

    *bits_outp = best_bits;
    *ampl_outp = best_a;
    *frame_start_outp = best_t;

    float confidence = best_c;

    if ( confidence == 0 )
	return 0;

#ifdef FSK_DEBUG
    unsigned char bitchar;
    // FIXME? hardcoded chop off framing bits for debug
    // Hmmm... we have now way to  distinguish between:
    // 		8-bit data with no start/stopbits == 8 bits
    // 		5-bit with prevstop+start+stop == 8 bits
    switch ( expect_n_bits ) {
      case 11:	bitchar = ( *bits_outp >> 2 ) & 0xFF;
		break;
      case 8:
      default:
		bitchar = *bits_outp & 0xFF;
		break;
    }

    debug_log("FSK_FRAME bits='");
    for ( j=0; j<expect_n_bits; j++ )
	debug_log("%c", ( ( *bits_outp >> j ) & 1 ) ? '1' : '0' );
    debug_log("' datum='%c' (0x%02x)   c=%f  a=%f  t=%u\n",
	    isprint(bitchar)||isspace(bitchar) ? bitchar : '.',
	    bitchar,
	    confidence, best_a, best_t);
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
    float magscalar = 1.0f / ((float)nsamples/2.0f);
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

