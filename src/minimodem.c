/*
 * minimodem.c
 *
 * minimodem - software audio Bell-type or RTTY FSK modem
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


#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define VERSION "unknown"
#endif

#include "simpleaudio.h"
#include "fsk.h"
#include "baudot.h"


/*
 * ASCII 8-bit data framebits decoder/encoder (passthrough)
 */

/* returns the number of datawords stuffed into *databits_outp */
int
framebits_encode_ascii8( unsigned int *databits_outp, char char_out )
{
    *databits_outp = char_out;
    return 1;
}

/* returns nbytes decoded */
static unsigned int
framebits_decode_ascii8( char *dataout_p, unsigned int dataout_size,
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
 * Baudot 5-bit data framebits decoder/encoder
 */

#define framebits_encode_baudot baudot_encode

/* returns nbytes decoded */
static unsigned int
framebits_decode_baudot( char *dataout_p, unsigned int dataout_size,
	unsigned int bits )
{
    if ( dataout_p == NULL ) {	// frame processor reset: reset Baudot state
	    baudot_reset();
	    return 0;
    }
    assert( (bits & ~0x1F) == 0 );
    assert( dataout_size >= 1);
    return baudot_decode(dataout_p, bits);
}



int		tx_transmitting = 0;
int		tx_leader_bits_len = 2;
int		tx_trailer_bits_len = 2;

simpleaudio	*tx_sa_out;
float		tx_bfsk_mark_f;
unsigned int	tx_bit_nsamples;

void
tx_stop_transmit_sighandler( int sig )
{
    // fprintf(stderr, "alarm\n");

    int j;
    for ( j=0; j<tx_trailer_bits_len; j++ )
	simpleaudio_tone(tx_sa_out, tx_bfsk_mark_f, tx_bit_nsamples);

    // 0.5 sec of zero samples to flush - FIXME lame
    size_t sample_rate = simpleaudio_get_rate(tx_sa_out);
    simpleaudio_tone(tx_sa_out, 0, sample_rate/2);

    tx_transmitting = 0;
}


/*
 * rudimentary BFSK transmitter
 */
static void fsk_transmit_stdin(
	simpleaudio *sa_out,
	float data_rate,
	float bfsk_mark_f,
	float bfsk_space_f,
	int n_data_bits,
	float bfsk_txstopbits,
	int (*framebits_encoder)( unsigned int *databits_outp, char char_out )
	)
{
    size_t sample_rate = simpleaudio_get_rate(sa_out);
    size_t bit_nsamples = sample_rate / data_rate + 0.5;
    int c;

    tx_sa_out = sa_out;
    tx_bfsk_mark_f = bfsk_mark_f;
    tx_bit_nsamples = bit_nsamples;

    // one-shot
    struct itimerval itv = {
	{0, 0},						// it_interval
	{0, 1000000/(data_rate+data_rate*0.03) }	// it_value
    };

    signal(SIGALRM, tx_stop_transmit_sighandler);

    tx_transmitting = 0;
    while ( (c = getchar()) != EOF )
    {
	setitimer(ITIMER_REAL, NULL, NULL);

	// fprintf(stderr, "<c=%d>", c);
	unsigned int nwords;
	unsigned int bits[2];
	nwords = framebits_encoder(bits, c);

	if ( !tx_transmitting )
	{
	    tx_transmitting = 1;
	    int j;
	    for ( j=0; j<tx_leader_bits_len; j++ )
		simpleaudio_tone(sa_out, bfsk_mark_f, bit_nsamples);
	}
	unsigned int j;
	for ( j=0; j<nwords; j++ ) {
	    simpleaudio_tone(sa_out, bfsk_space_f, bit_nsamples);	// start
	    int i;
	    for ( i=0; i<n_data_bits; i++ ) {				// data
		unsigned int bit = ( bits[j] >> i ) & 1;
		float tone_freq = bit == 1 ? bfsk_mark_f : bfsk_space_f;
		simpleaudio_tone(sa_out, tone_freq, bit_nsamples);
	    }
	    simpleaudio_tone(sa_out, bfsk_mark_f,
				bit_nsamples * bfsk_txstopbits);	// stop
	}

	setitimer(ITIMER_REAL, &itv, NULL);
    }
    setitimer(ITIMER_REAL, NULL, NULL);
    if ( !tx_transmitting )
	return;

    tx_stop_transmit_sighandler(0);
}


static void
report_no_carrier( fsk_plan *fskp,
	unsigned int sample_rate,
	float bfsk_data_rate,
	float nsamples_per_bit,
	unsigned int nframes_decoded,
	size_t carrier_nsamples,
	float confidence_total )
{
    unsigned long long nbits_total = nframes_decoded * (fskp->n_data_bits+2);
#if 0
    fprintf(stderr, "nframes_decoded=%u\n", nframes_decoded);
    fprintf(stderr, "nbits_total=%llu\n", nbits_total);
    fprintf(stderr, "carrier_nsamples=%lu\n", carrier_nsamples);
    fprintf(stderr, "nsamples_per_bit=%f\n", nsamples_per_bit);
#endif
    float throughput_rate = nbits_total * sample_rate / (float)carrier_nsamples;
    fprintf(stderr, "### NOCARRIER ndata=%u confidence=%.2f throughput=%.2f",
	    nframes_decoded,
	    confidence_total / nframes_decoded,
	    throughput_rate);
    if ( (size_t)(nbits_total * nsamples_per_bit + 0.5) == carrier_nsamples ) {
	fprintf(stderr, " (rate perfect) ###\n");
    } else {
	float throughput_skew = (throughput_rate - bfsk_data_rate)
			    / bfsk_data_rate;
	fprintf(stderr, " (%.1f%% %s) ###\n",
		fabs(throughput_skew) * 100.0,
		signbit(throughput_skew) ? "slow" : "fast"
		);
    }
}

void
version()
{
    printf(
    "minimodem %s\n"
    "Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>\n"
    "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n\n"
    "Written by Kamal Mostafa <kamal@whence.com>.\n",
	VERSION);
}

void
usage()
{
    fprintf(stderr,
    "usage: minimodem [--tx|--rx] [options] {baudmode}\n"
    "		    -t, --tx, --transmit, --write\n"
    "		    -r, --rx, --receive,  --read     (default)\n"
    "		[options]\n"
    "		    -a, --auto-carrier\n"
    "		    -c, --confidence {threshold}\n"
    "		    -8, --ascii		ASCII  8-N-1\n"
    "		    -5, --baudot	Baudot 5-N-1\n"
    "		    -f, --file {filename.flac}\n"
    "		    -b, --bandwidth {rx_bandwidth}\n"
    "		    -M, --mark {mark_freq}\n"
    "		    -S, --space {space_freq}\n"
    "		    -T, --txstopbits {m.n}\n"
    "		    -q, --quiet\n"
    "		    -V, --version\n"
    "		{baudmode}\n"
    "		    1200 : Bell202  1200 bps --ascii\n"
    "		     300 : Bell103   300 bps --ascii\n"
    "		  N>=100 : Bellxxx     N bps --ascii\n"
    "		   N<100 : RTTY        N bps --baudot\n"
    "		    rtty : RTTY    45.45 bps --baudot\n"
    );
    exit(1);
}

int
main( int argc, char*argv[] )
{
    char *modem_mode = NULL;
    int TX_mode = -1;
    int quiet_mode = 0;
    float band_width = 0;
    unsigned int bfsk_mark_f = 0;
    unsigned int bfsk_space_f = 0;
    float bfsk_txstopbits = 0;
    unsigned int bfsk_n_data_bits = 0;
    int autodetect_shift;
    char *filename = NULL;
    char *program_name;

    float	carrier_autodetect_threshold = 0.0;
    float	bfsk_confidence_threshold = 0.6;

    program_name = strrchr(argv[0], '/');
    if ( program_name )
	program_name++;
    else
	program_name = argv[0];

    int c;
    int option_index;
    
    while ( 1 ) {
	static struct option long_options[] = {
	    { "version",	0, 0, 'V' },
	    { "tx",		0, 0, 't' },
	    { "transmit",	0, 0, 't' },
	    { "write",		0, 0, 't' },
	    { "rx",		0, 0, 'r' },
	    { "receive",	0, 0, 'r' },
	    { "read",		0, 0, 'r' },
	    { "confidence",	1, 0, 'c' },
	    { "auto-carrier",	0, 0, 'a' },
	    { "ascii",		0, 0, '8' },
	    { "baudot",		0, 0, '5' },
	    { "file",		1, 0, 'f' },
	    { "bandwidth",	1, 0, 'b' },
	    { "mark",		1, 0, 'M' },
	    { "space",		1, 0, 'S' },
	    { "txstopbits",	1, 0, 'T' },
	    { "quiet",		0, 0, 'q' },
	    { 0 }
	};
	c = getopt_long(argc, argv, "Vtrc:a85f:b:M:S:T:q",
		long_options, &option_index);
	if ( c == -1 )
	    break;
	switch( c ) {
	    case 'V':
			version();
			exit(0);
	    case 't':
			if ( TX_mode == 0 )
			    usage();
			TX_mode = 1;
			break;
	    case 'r':
			if ( TX_mode == 1 )
			    usage();
			TX_mode = 0;
			break;
	    case 'c':
			bfsk_confidence_threshold = atof(optarg);
			break;
	    case 'a':
			carrier_autodetect_threshold = 0.001;
			break;
	    case 'f':
			filename = optarg;
			break;
	    case '8':
			bfsk_n_data_bits = 8;
			break;
	    case '5':
			bfsk_n_data_bits = 5;
			break;
	    case 'b':
			band_width = atof(optarg);
			assert( band_width != 0 );
			break;
	    case 'M':
			bfsk_mark_f = atoi(optarg);
			assert( bfsk_mark_f > 0 );
			break;
	    case 'S':
			bfsk_space_f = atoi(optarg);
			assert( bfsk_space_f > 0 );
			break;
	    case 'T':
			bfsk_txstopbits = atof(optarg);
			assert( bfsk_txstopbits > 0 );
			break;
	    case 'q':
			quiet_mode = 1;
			break;
	    default:
			usage();
	}
    }
    if ( TX_mode == -1 )
	TX_mode = 0;

#if 0
    if (optind < argc) {
	printf("non-option ARGV-elements: ");
	while (optind < argc)
	    printf("%s ", argv[optind++]);
	printf("\n");
    }
#endif

    if (optind + 1 !=  argc) {
	fprintf(stderr, "E: *** Must specify {baudmode} (try \"300\") ***\n");
	usage();
    }

    modem_mode = argv[optind++];


    float	bfsk_data_rate = 0.0;
    int (*bfsk_framebits_encode)( unsigned int *databits_outp, char char_out );

    unsigned int (*bfsk_framebits_decode)( char *dataout_p, unsigned int dataout_size,
					unsigned int bits );

    if ( strncasecmp(modem_mode, "rtty",5)==0 ) {
	bfsk_data_rate = 45.45;
	if ( bfsk_n_data_bits == 0 )
	    bfsk_n_data_bits = 5;
    } else {
	bfsk_data_rate = atof(modem_mode);
	if ( bfsk_n_data_bits == 0 )
	    bfsk_n_data_bits = 8;
    }
    if ( bfsk_data_rate == 0.0 )
	usage();

    if ( bfsk_n_data_bits == 8 ) {
	bfsk_framebits_decode = framebits_decode_ascii8;
	bfsk_framebits_encode = framebits_encode_ascii8;
    } else if ( bfsk_n_data_bits == 5 ) {
	bfsk_framebits_decode = framebits_decode_baudot;
	bfsk_framebits_encode = framebits_encode_baudot;
    } else {
	assert( 0 && bfsk_n_data_bits );
    }

    if ( bfsk_data_rate >= 400 ) {
	/*
	 * Bell 202:     baud=1200 mark=1200 space=2200
	 */
	autodetect_shift = -1000;
	if ( bfsk_mark_f == 0 )
	    bfsk_mark_f  = 1200;
	if ( bfsk_space_f == 0 )
	    bfsk_space_f = bfsk_mark_f - autodetect_shift;
	if ( band_width == 0 )
	    band_width = 200;
    } else if ( bfsk_data_rate >= 100 ) {
	/*
	 * Bell 103:     baud=300 mark=1270 space=1070
	 * ITU-T V.21:   baud=300 mark=1280 space=1080
	 */
	autodetect_shift = 200;
	if ( bfsk_mark_f == 0 )
	    bfsk_mark_f  = 1270;
	if ( bfsk_space_f == 0 )
	    bfsk_space_f = bfsk_mark_f - autodetect_shift;
	if ( band_width == 0 )
	    band_width = 50;	// close enough
    } else {
	/*
	 * RTTY:     baud=45.45 mark/space=variable shift=-170
	 */
	autodetect_shift = 170;
	if ( bfsk_mark_f == 0 )
	    bfsk_mark_f  = 1585;
	if ( bfsk_space_f == 0 )
	    bfsk_space_f = bfsk_mark_f - autodetect_shift;
	if ( bfsk_txstopbits == 0 )
	    bfsk_txstopbits = 1.5;	// conventional for RTTY (?)
	if ( band_width == 0 ) {
	    band_width = 10;	// FIXME chosen arbitrarily
	}
    }

    if ( bfsk_txstopbits == 0 )
	bfsk_txstopbits = 1.0;

    /* restrict band_width to <= data rate (FIXME?) */
    if ( band_width > bfsk_data_rate )
	band_width = bfsk_data_rate;


    /*
     * Handle transmit mode
     */
    if ( TX_mode ) {

	simpleaudio *sa_out = NULL;

	if ( filename ) {
	    sa_out = simpleaudio_open_stream_sndfile(SA_STREAM_PLAYBACK,
					filename);
	    if ( ! sa_out )
		return 1;
	}
	if ( ! sa_out )
	    sa_out = simpleaudio_open_stream_pulseaudio(SA_STREAM_PLAYBACK,
					program_name, "output audio");
	if ( ! sa_out )
	    return 1;

	fsk_transmit_stdin(sa_out,
				bfsk_data_rate,
				bfsk_mark_f, bfsk_space_f,
				bfsk_n_data_bits,
				bfsk_txstopbits,
				bfsk_framebits_encode
				);
	return 0;
    }


    /*
     * Open the input audio stream
     */
    simpleaudio *sa = NULL;
    if ( filename ) {
	sa = simpleaudio_open_stream_sndfile(SA_STREAM_RECORD, filename);
	if ( ! sa )
	    return 1;
    }
    if ( ! sa )
	sa = simpleaudio_open_stream_pulseaudio(SA_STREAM_RECORD,
				    program_name, "input audio");
    if ( !sa )
        return 1;

    unsigned int sample_rate = simpleaudio_get_rate(sa);
    unsigned int nchannels = simpleaudio_get_channels(sa);

    assert( nchannels == 1 );


    /*
     * Prepare the input sample chunk rate
     */
    float nsamples_per_bit = sample_rate / bfsk_data_rate;


    /*
     * Prepare the fsk plan
     */

    fsk_plan *fskp;
    fskp = fsk_plan_new(sample_rate, bfsk_mark_f, bfsk_space_f,
				band_width, bfsk_n_data_bits);
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
    size_t		carrier_nsamples = 0;

    unsigned int	noconfidence = 0;
    size_t		noconfidence_nsamples = 0;
    unsigned int	advance = 0;

    while ( 1 ) {

	debug_log("advance=%u\n", advance);

	if ( carrier && nframes_decoded > 0 )
	    carrier_nsamples += advance;
	
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
		samples_readptr - samplebuf, read_nsamples, r);
	if ( r < 0 ) {
	    fprintf(stderr, "simpleaudio_read: error\n");
	    ret = -1;
            break;
	}
	else if ( r > 0 )
	    samples_nvalid += r;

	if ( samples_nvalid == 0 )
	    break;

	/* Auto-detect carrier frequency */
	static int carrier_band = -1;
	if ( carrier_autodetect_threshold > 0.0 && carrier_band < 0 ) {
	    unsigned int i;
	    float nsamples_per_scan = nsamples_per_bit;
	    if ( nsamples_per_scan > fskp->fftsize )
		nsamples_per_scan = fskp->fftsize;
	    for ( i=0; i+nsamples_per_scan<=samples_nvalid;
						 i+=nsamples_per_scan ) {
		carrier_band = fsk_detect_carrier(fskp,
				    samplebuf+i, nsamples_per_scan,
				    carrier_autodetect_threshold);
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

	    debug_log("### TONE freq=%.1f ###\n",
		    carrier_band * fskp->band_width);

	    fsk_set_tones_by_bandshift(fskp, /*b_mark*/carrier_band, b_shift);
	}

	/*
	 * The main processing algorithm: scan samplesbuf for FSK frames,
	 * looking at an entire frame at once.
	 */

	debug_log( "--------------------------\n");

	unsigned int frame_nsamples = nsamples_per_bit * fskp->n_frame_bits;

	if ( samples_nvalid < frame_nsamples )
	    break;

	unsigned int try_max_nsamples = nsamples_per_bit;
#define FSK_ANALYZE_NSTEPS		10	/* accuracy vs. performance */
		// Note: FSK_ANALYZE_NSTEPS has subtle effects on the
		// "rate perfect" calculation.  oh well.
	unsigned int try_step_nsamples = nsamples_per_bit / FSK_ANALYZE_NSTEPS;
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

	// FIXME: hardcoded chop off framing bits
	if ( fskp->n_data_bits == 5 )
	    bits = ( bits >> 2 ) & 0x1F;
	else
	    bits = ( bits >> 2 ) & 0xFF;

#define FSK_MAX_NOCONFIDENCE_BITS	20

#define FSK_SCAN_LAG			0.2

	if ( confidence <= bfsk_confidence_threshold ) {
	    // FIXME: explain
	    if ( ++noconfidence > FSK_MAX_NOCONFIDENCE_BITS )
	    {
		carrier_band = -1;
		if ( carrier ) {
		    carrier_nsamples -= noconfidence_nsamples;
		    if ( nframes_decoded > 0 )
			carrier_nsamples += nsamples_per_bit * FSK_SCAN_LAG;
		    if ( !quiet_mode )
			report_no_carrier(fskp, sample_rate, bfsk_data_rate,
			    nsamples_per_bit, nframes_decoded,
			    carrier_nsamples, confidence_total);
		    carrier = 0;
		    carrier_nsamples = 0;
		    confidence_total = 0;
		    nframes_decoded = 0;
		}
	    }

	    /* Advance the sample stream forward by try_max_nsamples so the
	     * next time around the loop we continue searching from where
	     * we left off this time.		*/
	    advance = try_max_nsamples;
	    noconfidence_nsamples += advance;
	    continue;
	}

	if ( !carrier ) {
	    if ( !quiet_mode ) {
		if ( bfsk_data_rate >= 100 )
		    fprintf(stderr, "### CARRIER %u @ %.1f Hz ###\n",
			    (unsigned int)(bfsk_data_rate + 0.5),
			    fskp->b_mark * fskp->band_width);
		else
		    fprintf(stderr, "### CARRIER %.2f @ %.1f Hz ###\n",
			    bfsk_data_rate,
			    fskp->b_mark * fskp->band_width);
	    }
	    carrier = 1;
	    /* back up carrier_nsamples to account for the imminent advance */
	    noconfidence_nsamples = frame_start_sample;
	    bfsk_framebits_decode(0, 0, 0);	/* reset the frame processor */
	}


	confidence_total += confidence;
	nframes_decoded++;
	noconfidence = 0;

	/* Advance the sample stream forward past the decoded frame
	 * but not past the stop bit, since we want it to appear as
	 * the prev_stop bit of the next frame, so ...
	 *
	 * advance = 1 prev_stop + 1 start + N data bits == n_data_bits+2
	 *
	 * but actually advance just a bit less than that to allow
	 * for clock skew, hence FSK_SCAN_LAG.
	 */
	advance = frame_start_sample +
	    nsamples_per_bit * (float)(fskp->n_data_bits + 2 - FSK_SCAN_LAG);

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

	dataout_nbytes += bfsk_framebits_decode(dataoutbuf + dataout_nbytes,
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
	carrier_nsamples -= noconfidence_nsamples;
	if ( nframes_decoded > 0 )
	    carrier_nsamples += nsamples_per_bit * FSK_SCAN_LAG;
	if ( !quiet_mode )
	    report_no_carrier(fskp, sample_rate, bfsk_data_rate,
		nsamples_per_bit, nframes_decoded,
		carrier_nsamples, confidence_total);
    }

    simpleaudio_close(sa);

    fsk_plan_destroy(fskp);

    return ret;
}
