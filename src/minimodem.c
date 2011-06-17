/*
 * minimodem.c
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

#include "simpleaudio.h"
#include "fsk.h"
#include "baudot.h"


/*
 * ASCII 8-bit data frame processor (passthrough)
 */
static unsigned int
frame_process_ascii8( char *dataout_p, unsigned int dataout_size,
	unsigned int bits )
{
    if ( dataout_p == NULL )	// frame processor reset: noop
	return 0;
    assert( (bits & ~0xFF) == 0 );
    assert( dataout_size >= 1);
    *dataout_p = bits;
    return 1;
}


/*
 * Baudot 5-bit data frame processor
 */
static unsigned int
frame_process_baudot( char *dataout_p, unsigned int dataout_size,
	unsigned int bits )
{
    if ( dataout_p == NULL ) {	// frame processor reset: reset Baudot state
	    baudot_reset();
	    return 0;
    }
    assert( (bits & ~0x1F) == 0 );
    assert( dataout_size >= 1);
    return baudot(bits, dataout_p);
}


int
main( int argc, char*argv[] )
{
    if ( argc < 2 ) {
	fprintf(stderr, "usage: minimodem {baud|mode} [filename] "
					"[ band_width ] "
					"[ mark_hz space_hz ]\n");
	return 1;
    }

    int argi = 1;

    float	decode_rate;
    int		decode_n_data_bits;

    unsigned int (*frame_process)( char *dataout_p, unsigned int dataout_size,
					unsigned int bits );

    if ( strncasecmp(argv[argi],"rtty",5)==0 ) {
	decode_rate = 45.45;
	decode_n_data_bits = 5;
	frame_process = frame_process_baudot;
    } else {
	decode_rate = atof(argv[argi]);
	decode_n_data_bits = 8;
	frame_process = frame_process_ascii8;
    }
    argi++;


    unsigned int band_width;
    unsigned int bfsk_mark_f;
    unsigned int bfsk_space_f;
    unsigned int autodetect_shift;

    if ( decode_rate >= 400 ) {
	/*
	 * Bell 202:     baud=1200 mark=1200 space=2200
	 */
	bfsk_mark_f  = 1200;
	bfsk_space_f = 2200;
	band_width = 200;
	autodetect_shift = 0;	// not used
    } else if ( decode_rate >= 100 ) {
	/*
	 * Bell 103:     baud=300 mark=1270 space=1070
	 * ITU-T V.21:   baud=300 mark=1280 space=1080
	 */
	bfsk_mark_f  = 1270;
	bfsk_space_f = 1070;
	band_width = 50;	// close enough
	autodetect_shift = 200;
    } else {
	/*
	 * RTTY:     baud=45.45 mark/space=variable shift=-170
	 */
	bfsk_mark_f  = 0;
	bfsk_space_f = 0;
	band_width = 10;
//	band_width = 68;	// FIXME FIXME FIXME -- causes assert crash
	autodetect_shift = 170;
    }


    /*
     * Open the input audio stream
     */
    simpleaudio *sa = NULL;
    if ( argi < argc && strncmp(argv[argi],"-",2)!=0 ) {
	sa = simpleaudio_open_source_sndfile(argv[argi++]);
	if ( !sa )
	    return 1;
    }
    if ( ! sa ) {
	sa = simpleaudio_open_stream_pulseaudio(SA_STREAM_RECORD,
				argv[0], "input audio");
    }
    if ( !sa )
        return 1;

    unsigned int sample_rate = simpleaudio_get_rate(sa);
    unsigned int nchannels = simpleaudio_get_channels(sa);

    assert( nchannels == 1 );

    /*
     * Alloc for band_width and tone frequency overrides
     * FIXME -- tone override doesn't work with autodetect carrier
     */

    if ( argi < argc ) {
	band_width = atoi(argv[argi++]);	// FIXME make band_width float?
    }

    if ( argi < argc ) {
	assert(argc-argi == 2);
	bfsk_mark_f = atoi(argv[argi++]);
	bfsk_space_f = atoi(argv[argi++]);
    }

    /*
     * Prepare the input sample chunk rate
     */
    float nsamples_per_bit = sample_rate / decode_rate;


    /*
     * Prepare the fsk plan
     */

    fsk_plan *fskp;
    fskp = fsk_plan_new(sample_rate, bfsk_mark_f, bfsk_space_f,
				band_width, decode_n_data_bits);
    if ( !fskp ) {
        fprintf(stderr, "fsk_plan_new() failed\n");
        return 1;
    }

    /*
     * Prepare the input sample buffer.  For 8-bit frames with prev/start/stop
     * we need 11 data-bits worth of samples, and we will scan through one bits
     * worth at a time, hence we need a minimum total input buffer size of 12
     * data-bits.  */
// FIXME I should be able to reduce this to * 9 for 5-bit data, but
// it SOMETIMES crashes -- probably due to non-integer nsamples_per_bit
// FIXME by passing it down into the fsk code?
    size_t	samplebuf_size = ceilf(nsamples_per_bit) * 12;
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

#define CARRIER_AUTODETECT_THRESHOLD	0.03
#ifdef CARRIER_AUTODETECT_THRESHOLD
	/* Auto-detect carrier frequency */
	static int carrier_band = -1;
	// FIXME?: hardcoded 300 baud trigger for carrier autodetect
	if ( decode_rate <= 300 && carrier_band < 0 ) {
	    unsigned int i;
//	    float nsamples_per_scan = fskp->fftsize;
	    float nsamples_per_scan = nsamples_per_bit;
	    for ( i=0; i+nsamples_per_scan<=samples_nvalid;
						 i+=nsamples_per_scan ) {
		carrier_band = fsk_detect_carrier(fskp,
				    samplebuf+i, nsamples_per_scan,
				    CARRIER_AUTODETECT_THRESHOLD);
		if ( carrier_band >= 0 )
		    break;
	    }
	    advance = i + nsamples_per_scan;
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

	    debug_log("### TONE freq=%u ###\n",
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
	  // FIXME: explain
	  if ( ++noconfidence > FSK_MAX_NOCONFIDENCE_BITS )
	  {
#ifdef CARRIER_AUTODETECT_THRESHOLD
	    carrier_band = -1;
#endif
	    if ( carrier ) {
		fprintf(stderr, "### NOCARRIER ndata=%u confidence=%f ###\n",
			nframes_decoded, confidence_total / nframes_decoded );
		carrier = 0;
		confidence_total = 0;
		nframes_decoded = 0;
	      }
	    }

	    /* Advance the sample stream forward by try_max_nsamples so the
	     * next time around the loop we continue searching from where
	     * we left off this time.		*/
	    advance = try_max_nsamples;
	    continue;
	}


	if ( !carrier ) {
	    fprintf(stderr, "### CARRIER %u @ %u Hz ###\n",
		    (unsigned int)(decode_rate + 0.5),
		    fskp->b_mark * fskp->band_width);
	    carrier = 1;
	    frame_process(0, 0, 0);	/* reset the frame processor */
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

	debug_log("@ nsamples_per_bit=%.3f n_data_bits=%u "
			" frame_start=%u advance=%u\n",
		    nsamples_per_bit, fskp->n_data_bits,
		    frame_start_sample, advance);


	/*
	 * Send the raw data frame bits to the backend frame processor
	 * for final conversion to output data bytes.
	 */

	unsigned int dataout_size = 4096;
	char dataoutbuf[4096];
	unsigned int dataout_nbytes = 0;

	dataout_nbytes += frame_process(dataoutbuf + dataout_nbytes,
						dataout_size - dataout_nbytes,
						bits);

	/*
	 * Print the output buffer to stdout
	 */
	if ( dataout_nbytes ) {
	    char *p = dataoutbuf;
	    for ( ; dataout_nbytes; p++,dataout_nbytes-- ) {
		char printable_char = isprint(*p)||isspace(*p) ? *p : '.';
		printf( "%c", printable_char );
	    }
	    fflush(stdout);
	}

    } /* end of the main loop */

    if ( carrier ) {
	fprintf(stderr, "### NOCARRIER ndata=%u confidence=%f ###\n",
		nframes_decoded, confidence_total / nframes_decoded );
    }

    simpleaudio_close(sa);

    fsk_plan_destroy(fskp);

    return ret;
}
