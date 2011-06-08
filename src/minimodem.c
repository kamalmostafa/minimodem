
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "simpleaudio.h"
#include "fsk.h"

int
main( int argc, char*argv[] )
{
    if ( argc < 2 ) {
	fprintf(stderr, "usage: minimodem [filename] baud_rate "
					"[ mark_hz space_hz ]\n");
	return 1;
    }

    int argi = 1;

    simpleaudio *sa;

    char *p;
    for ( p=argv[argi]; *p; p++ )
	if ( !isdigit(*p) )
	    break;
    if ( *p ) {
	sa = simpleaudio_open_source_sndfile(argv[argi]);
	argi++;
    } else {
	sa = simpleaudio_open_source_pulseaudio(argv[0], "FSK demodulator");
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
#define CARRIER_AUTODETECT_THRESHOLD	0.10
#ifdef CARRIER_AUTODETECT_THRESHOLD
    unsigned int autodetect_shift = 200;
#endif
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
     * Prepare the input sample buffer.  For 8-bit frames with prev/start/stop
     * we need 11 data-bits worth of samples, and we will scan through one bits
     * worth at a time, hence we need a minimum total input buffer size of 12
     * data-bits.  */
    size_t	samplebuf_size = nsamples_per_bit * 12;
    float	*samplebuf = malloc(samplebuf_size * sizeof(float));
    float	*samples_readptr = samplebuf;
    size_t	read_nsamples = samplebuf_size;
    size_t	samples_nvalid = 0;
    debug_log("samplebuf_size=%lu\n", samplebuf_size);

    /*
     * Run the main loop
     */

    int			ret = 0;

    int			carrier = 0;
    float		confidence_total = 0;
    unsigned int	nframes_decoded = 0;

    unsigned int	noconfidence = 0;
    unsigned int	advance = 0;

    while ( 1 ) {

	debug_log("advance=%u\n", advance);
	
	/* Shift the samples in samplebuf by 'advance' samples */
	assert( advance <= samplebuf_size );
	if ( advance == samplebuf_size ) {
	    samples_nvalid = 0;
	    samples_readptr = samplebuf;
	    read_nsamples = samplebuf_size;
	    advance = 0;
	}
	if ( advance ) {
	    if ( advance > samples_nvalid )
		break;
	    memmove(samplebuf, samplebuf+advance,
		    (samplebuf_size-advance)*sizeof(float));
	    samples_nvalid -= advance;
	    samples_readptr = samplebuf + (samplebuf_size-advance);
	    read_nsamples = advance;
	}

	/* Read more samples into samplebuf (fill it) */
	assert ( read_nsamples > 0 );
	assert ( samples_nvalid + read_nsamples <= samplebuf_size );
	ssize_t r;
	r = simpleaudio_read(sa, samples_readptr, read_nsamples);
	debug_log("simpleaudio_read(samplebuf+%ld, n=%lu) returns %ld\n",
		samples_readptr - samplebuf, samples_nvalid, r);
	if ( r < 0 ) {
	    fprintf(stderr, "simpleaudio_read: error\n");
	    ret = -1;
            break;
	}
	else if ( r > 0 )
	    samples_nvalid += r;

	if ( samples_nvalid == 0 )
	    break;

#ifdef CARRIER_AUTODETECT_THRESHOLD
	/* Auto-detect carrier frequency */
	static int carrier_band = -1;
	// FIXME?: hardcoded 300 baud trigger for carrier autodetect
	if ( decode_rate <= 300 && carrier_band < 0 ) {
	    unsigned int i;
	    for ( i=0; i+fskp->fftsize<=samples_nvalid; i+=fskp->fftsize ) {
		carrier_band = fsk_detect_carrier(fskp,
				    samplebuf+i, fskp->fftsize,
				    CARRIER_AUTODETECT_THRESHOLD);
		if ( carrier_band >= 0 )
		    break;
	    }
	    advance = i + fskp->fftsize;
	    if ( advance > samples_nvalid )
		advance = samples_nvalid;
	    if ( carrier_band < 0 ) {
		debug_log("autodetected carrier band not found\n");
		continue;
	    }

	    // FIXME: hardcoded negative shift
	    int b_shift = - (float)(autodetect_shift + fskp->band_width/2.0)
						/ fskp->band_width;
	    /* only accept a carrier as b_mark if it will not result
	     * in a b_space band which is "too low". */
	    if ( carrier_band + b_shift < 1 ) {
		debug_log("autodetected space band too low\n" );
		carrier_band = -1;
		continue;
	    }

	    fprintf(stderr, "### TONE freq=%u ###\n",
		    carrier_band * fskp->band_width);

	    fsk_set_tones_by_bandshift(fskp, /*b_mark*/carrier_band, b_shift);
	}
#endif

	/*
	 * The main processing algorithm: scan samplesbuf for FSK frames,
	 * looking at an entire frame at once.
	 */

	debug_log( "--------------------------\n");

	unsigned int frame_nsamples = nsamples_per_bit * fskp->n_frame_bits;

	if ( samples_nvalid < frame_nsamples )
	    break;

	// FIXME: explain
	unsigned int try_max_nsamples = nsamples_per_bit;
	unsigned int try_step_nsamples = nsamples_per_bit / 8;
	if ( try_step_nsamples == 0 )
	    try_step_nsamples = 1;

	float confidence;
	unsigned int bits = 0;
	/* Note: frame_start_sample is actually the sample where the
	 * prev_stop bit begins (since the "frame" includes the prev_stop). */
	unsigned int frame_start_sample = 0;

	confidence = fsk_find_frame(fskp, samplebuf, frame_nsamples,
			try_max_nsamples,
			try_step_nsamples,
			&bits,
			&frame_start_sample
			);

#define FSK_MIN_CONFIDENCE		0.5	/* not critical */
#define FSK_MAX_NOCONFIDENCE_BITS	20

	if ( confidence <= FSK_MIN_CONFIDENCE ) {
	    if ( carrier ) {
	      // FIXME: explain
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
	    continue;
	}


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
	advance = frame_start_sample +
		    nsamples_per_bit * (float)(fskp->n_data_bits + 1.5);

	debug_log( "@ frame_start=%u  advance=%u\n",
		    frame_start_sample, advance);

	char the_byte = isprint(bits)||isspace(bits) ? bits : '.';
	printf( "%c", the_byte );
	fflush(stdout);

    } /* end of the main loop */

    if ( carrier ) {
	fprintf(stderr, "### NOCARRIER nbytes=%u confidence=%f ###\n",
		nframes_decoded, confidence_total / nframes_decoded );
    }

    simpleaudio_close(sa);

    fsk_plan_destroy(fskp);

    return ret;
}
