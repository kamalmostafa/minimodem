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

	unsigned int	n_frame_bits;
#ifdef USE_FFT
	int		fftsize;	// fftw wants this to be signed. why?
	unsigned int	nbands;
	unsigned int	band_width;
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

    /* 1 prev_stop + n_data_bits + 1 start + 1 stop == n_data_bits + 3 */
    fskp->n_frame_bits = fskp->n_data_bits + 3;

#ifdef USE_FFT
    fskp->band_width = filter_bw;

    float fft_half_bw = (float)fskp->band_width / 2.0;
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


static float
fsk_frame_decode( fsk_plan *fskp, float *samples, unsigned int frame_nsamples,
	unsigned int *bits_outp )
{
    float v = 0;

    float samples_per_bit = (float)frame_nsamples / fskp->n_frame_bits;

    unsigned int bit_nsamples = (float)(samples_per_bit + 0.5);

    // 0123456789A
    // isddddddddp	i == idle bit (a.k.a. prev_stop bit)
    //			s == start bit
    //			d == data bits
    //			p == stop bit
    // MSddddddddM  <-- expected mark/space framing pattern

    unsigned int begin_idlebit = 0;
    unsigned int begin_startbit = (float)(samples_per_bit * 1 + 0.5);
    unsigned int begin_stopbit  = (float)(samples_per_bit * 10 + 0.5);

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

    unsigned int bits_out = 0;
    int i;
    float d_str[32];
    for ( i=0; i<fskp->n_data_bits; i++ ) {
	debug_log("\t\tdata    ");
	unsigned int begin_databit = (float)(samples_per_bit * (i+2) + 0.5);
	fsk_bit_analyze(fskp, samples+begin_databit, bit_nsamples,
							    &bit, &d_str[i]);
	v += d_str[i];
	bits_out |= bit << i;
    }
    *bits_outp = bits_out;


    /* Compute frame decode confidence as the inverse of the average
     * bit-strength delta from the average bit-strength.  (whew).*/
    v /= fskp->n_frame_bits;	/* v = average bit-strength */

    /*
     * Filter out noise below FSK_MIN_STRENGTH threshold
     */
#define FSK_MIN_STRENGTH	0.05
    if ( v < FSK_MIN_STRENGTH )
	return 0.0;

    float confidence = 0;
    confidence += 1.0 - fabs(i_str - v);
    confidence += 1.0 - fabs(s_str - v);
    confidence += 1.0 - fabs(p_str - v);
    for ( i=0; i<fskp->n_data_bits; i++ ) {
	confidence += 1.0 - fabs(d_str[i] - v);
    }
    confidence /= fskp->n_frame_bits;

debug_log( "frame decode confidence=%f\n", confidence );
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
    unsigned int t;
    unsigned int best_t = 0;
    float best_c = 0.0;
    unsigned int best_bits = 0;
    for ( t=0; t<try_max_nsamples; t+=try_step_nsamples )
    {
	float c;
	unsigned int bits_out = 0;
    debug_log("try fsk_frame_decode(skip=%+d)\n", t);
	c = fsk_frame_decode(fskp, samples+t, frame_nsamples, &bits_out);
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

    fprintf(stderr, "### TONE freq=%u mag=%.2f ###\n",
	    max_mag_band * fskp->band_width, max_mag);

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

/****************************************************************************/


#include <assert.h>
#include <stdlib.h>

#include "simpleaudio.h"
#include "tscope_print.h"

int
main( int argc, char*argv[] )
{
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
    unsigned int autodetect_shift = 200;
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
     * Prepare the fsk plan
     */

    fsk_plan *fskp = fsk_plan_new(sample_rate,
				    bfsk_mark_f, bfsk_space_f,
				    band_width, 8);

    if ( !fskp ) {
        fprintf(stderr, "fsk_plan_new() failed\n");
        return 1;
    }

    /*
     * Run the main loop
     */

    int ret = 0;

    size_t	fill_nsamples = nsamples_per_bit * 12;

    size_t	buf_nsamples = fill_nsamples;
    float	*samples = malloc(buf_nsamples * sizeof(float));

    size_t	read_nsamples = fill_nsamples;
    float	*read_bufptr = samples;

    float		confidence_total = 0;
    unsigned int	nframes_decoded = 0;

    int carrier = 0;
    unsigned int noconfidence = 0;
    unsigned int advance = 0;

    while ( 1 ) {

	if ( advance ) {
	    memmove(samples, samples+advance,
		    (fill_nsamples-advance)*sizeof(float));
	    read_bufptr = samples + (fill_nsamples-advance);
	    read_nsamples = advance;
	}

	debug_log( "@read samples+%ld n=%lu\n",
		read_bufptr - samples, read_nsamples);

	assert ( read_nsamples <= buf_nsamples );
	assert ( read_nsamples > 0 );

	if ((ret=simpleaudio_read(sa, read_bufptr, read_nsamples)) <= 0)
            break;

#define CARRIER_AUTODETECT_THRESHOLD	0.10

#ifdef CARRIER_AUTODETECT_THRESHOLD
	static int carrier_band = -1;
	// FIXME?: hardcoded 300 baud trigger for carrier autodetect
	if ( decode_rate <= 300 && carrier_band < 0 ) {
	    unsigned int i;
	    for ( i=0; i+fskp->fftsize<=buf_nsamples; i+=fskp->fftsize ) {
		carrier_band = fsk_detect_carrier(fskp,
				    samples+i, fskp->fftsize,
				    CARRIER_AUTODETECT_THRESHOLD);
		if ( carrier_band >= 0 )
		    break;
	    }
	    advance = i;	/* set advance, in case we end up continuing */
	    if ( carrier_band < 0 )
		continue;

	    // FIXME: hardcoded negative shift
	    int b_shift = - (float)(autodetect_shift + fskp->band_width/2.0)
						/ fskp->band_width;
	    /* only accept a carrier as b_mark if it will not result
	     * in a b_space band which is "too low". */
	    if ( carrier_band + b_shift < 1 ) {
		carrier_band = -1;
		continue;
	    }
	    fsk_set_tones_by_bandshift(fskp, /*b_mark*/carrier_band, b_shift);
	}
#endif

debug_log( "--------------------------\n");

	// FIXME: explain
	unsigned int frame_nsamples = nsamples_per_bit * fskp->n_frame_bits;

	// FIXME: explain
	unsigned int try_max_nsamples = nsamples_per_bit;

	// FIXME: explain
	// THIS BREAKS ONE OF THE 1200 TESTS, WHEN I USE AVOID_TRANSIENTS...
	//unsigned int try_step_nsamples = nsamples_per_bit / 4;
	// BUT THIS FIXES IT AGAIN:
	unsigned int try_step_nsamples = nsamples_per_bit / 8;
	if ( try_step_nsamples == 0 )
	    try_step_nsamples = 1;

	float confidence;
	unsigned int bits = 0;
	/* Note: frame_start_sample is actually the sample where the
	 * prev_stop bit begins (since the "frame" includes the prev_stop). */
	unsigned int frame_start_sample = 0;

	confidence = fsk_find_frame(fskp, samples, frame_nsamples,
			try_max_nsamples,
			try_step_nsamples,
			&bits,
			&frame_start_sample
			);

#define FSK_MIN_CONFIDENCE	0.5	/* not critical */

	if ( confidence <= FSK_MIN_CONFIDENCE ) {

	    if ( carrier ) {
	      // FIXME: explain
#define FSK_MAX_NOCONFIDENCE_BITS	20
	      if ( ++noconfidence > FSK_MAX_NOCONFIDENCE_BITS )
	      {
		fprintf(stderr, "### NOCARRIER nbytes=%u confidence=%f ###\n",
			nframes_decoded, confidence_total / nframes_decoded );
		carrier = 0;
		confidence_total = 0;
		nframes_decoded = 0;
#ifdef CARRIER_AUTODETECT_THRESHOLD
		carrier_band = -1;
#endif
	      }
	    }

	    /* Advance the sample stream forward by try_max_nsamples so the
	     * next time around the loop we continue searching from where
	     * we left off this time.		*/
	    advance = try_max_nsamples;

	} else {

	    if ( !carrier ) {
		fprintf(stderr, "### CARRIER ###\n");
		carrier = 1;
	    }
	    confidence_total += confidence;
	    nframes_decoded++;
	    noconfidence = 0;

	    /* Advance the sample stream forward past the decoded frame
	     * but not past the stop bit, since we want it to appear as
	     * the prev_stop bit of the next frame, so ...
	     *
	     * advance = 1 prev_stop + 1 start + 8 data bits == 10 bits
	     *
	     * but actually advance just a bit less than that to allow
	     * for clock skew, so ...
	     *
	     * advance = 9.5 bits		*/
	    advance = frame_start_sample + nsamples_per_bit * (float)(fskp->n_data_bits + 1.5);

	    debug_log( "@ frame_start=%u  advance=%u\n", frame_start_sample, advance);

	    char the_byte = isprint(bits)||isspace(bits) ? bits : '.';
	    printf( "%c", the_byte );
	    fflush(stdout);


	}

    }

    if ( ret != 0 )
	fprintf(stderr, "simpleaudio_read: error\n");

    if ( carrier ) {
	fprintf(stderr, "### NOCARRIER nbytes=%u confidence=%f ###\n",
		nframes_decoded, confidence_total / nframes_decoded );
    }

    simpleaudio_close(sa);

    fsk_plan_destroy(fskp);

    return ret;
}
