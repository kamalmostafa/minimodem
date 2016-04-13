/*
 * minimodem.c
 *
 * minimodem - software audio Bell-type or RTTY FSK modem
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


#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define VERSION "unknown"
#endif

#include "simpleaudio.h"
#include "fsk.h"
#include "databits.h"

char *program_name = "";

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

static void fsk_transmit_frame(
	simpleaudio *sa_out,
	unsigned int bits,
	unsigned int n_data_bits,
	size_t bit_nsamples,
	float bfsk_mark_f,
	float bfsk_space_f,
	float bfsk_nstartbits,
	float bfsk_nstopbits,
	int invert_start_stop,
	int bfsk_msb_first
	)
{
    int i;
    if ( bfsk_nstartbits > 0 )
	simpleaudio_tone(sa_out, invert_start_stop ? bfsk_mark_f : bfsk_space_f,
			bit_nsamples * bfsk_nstartbits);	// start
    for ( i=0; i<n_data_bits; i++ ) {				// data
	unsigned int bit;
	if (bfsk_msb_first) {
		bit = ( bits >> (n_data_bits - i - 1) ) & 1;
	} else {
		bit = ( bits >> i ) & 1;
	}

	float tone_freq = bit == 1 ? bfsk_mark_f : bfsk_space_f;
	simpleaudio_tone(sa_out, tone_freq, bit_nsamples);
    }
    if ( bfsk_nstopbits > 0 )
	simpleaudio_tone(sa_out, invert_start_stop ? bfsk_space_f : bfsk_mark_f,
			bit_nsamples * bfsk_nstopbits);		// stop
}

static void fsk_transmit_stdin(
	simpleaudio *sa_out,
	int tx_interactive,
	float data_rate,
	float bfsk_mark_f,
	float bfsk_space_f,
	int n_data_bits,
	float bfsk_nstartbits,
	float bfsk_nstopbits,
	int invert_start_stop,
	int bfsk_msb_first,
	unsigned int bfsk_do_tx_sync_bytes,
	unsigned int bfsk_sync_byte,
	databits_encoder encode,
	int txcarrier
	)
{
    size_t sample_rate = simpleaudio_get_rate(sa_out);
    size_t bit_nsamples = sample_rate / data_rate + 0.5f;

    tx_sa_out = sa_out;
    tx_bfsk_mark_f = bfsk_mark_f;
    tx_bit_nsamples = bit_nsamples;

    // one-shot
    struct itimerval itv = {
	{0, 0},						// it_interval
	{0, 1000000/(float)(data_rate+data_rate*0.03f)}	// it_value
    };

    struct itimerval itv_zero = {
	{0, 0},						// it_interval
	{0, 0}						// it_value
    };

    // arbitrary chosen timeout value: 1/25 of a second
    unsigned int idle_carrier_usec = (1000000/25);

    int block_input = tx_interactive && !txcarrier;
    if ( block_input )
	signal(SIGALRM, tx_stop_transmit_sighandler);

    // Set up for select() should we need it
    int fd = fileno(stdin);
    fd_set fdset;

    tx_transmitting = 0;
    int end_of_file = 0;
    unsigned char buf;
    int n_read = 0;
    int idle = 0;
    while ( !end_of_file )
    {
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        struct timeval tv_idletimeout = { 0, 0 };

	if ( !tx_interactive ) {
	    // When stdin blocks we "emit idle tone", for a duration of
	    // idle_carrier_usec.  If !tx_interactive (i.e. writing to an
	    // audio file) make the select timeout the same duration.
	    tv_idletimeout.tv_usec = idle_carrier_usec;
	}

        if( block_input || select(fd+1, &fdset, NULL, NULL, &tv_idletimeout) )
        {
	    n_read = read(fd, &buf, sizeof(buf));
	    if( n_read <= 0 ) //Includes EOF (0) and errors (-1)
	    {
		end_of_file = 1;
		continue;     //Do nothing else
	    }
            idle = 0;
        }
	else
	    idle = 1;

	// Cause any running timer to immediately trigger
	if ( block_input )
	    setitimer(ITIMER_REAL, &itv_zero, NULL);

	if( !idle )
	{
	    // fprintf(stderr, "<c=%d>", c);
	    unsigned int nwords;
	    unsigned int bits[2];
	    unsigned int j;
	    nwords = encode(bits, buf);

	    if ( !tx_transmitting )
	    {
	        tx_transmitting = 1;
                /* emit leader tone (mark) */
                for ( j=0; j<tx_leader_bits_len; j++ )
                    simpleaudio_tone(sa_out, invert_start_stop ? bfsk_space_f : bfsk_mark_f, bit_nsamples);
	    }
	    if ( tx_transmitting < 2)
	    {
		tx_transmitting = 2;
		/* emit "preamble" of sync bytes */
		for ( j=0; j<bfsk_do_tx_sync_bytes; j++ )
		    fsk_transmit_frame(sa_out, bfsk_sync_byte, n_data_bits,
			    bit_nsamples, bfsk_mark_f, bfsk_space_f,
			    bfsk_nstartbits, bfsk_nstopbits, invert_start_stop, 0);
	    }

	    /* emit data bits */
	    for ( j=0; j<nwords; j++ )
		fsk_transmit_frame(sa_out, bits[j], n_data_bits,
			    bit_nsamples, bfsk_mark_f, bfsk_space_f,
			    bfsk_nstartbits, bfsk_nstopbits, invert_start_stop, bfsk_msb_first);
        }
        else
        {
	    tx_transmitting = 1;
            /* emit idle tone (mark) */
	    simpleaudio_tone(sa_out,
		    invert_start_stop ? bfsk_space_f : bfsk_mark_f,
		    idle_carrier_usec * sample_rate / 1000000);
	}

	if ( block_input )
	    setitimer(ITIMER_REAL, &itv, NULL);
    }
    if ( block_input ) {
	setitimer(ITIMER_REAL, &itv_zero, NULL);
	signal(SIGALRM, SIG_DFL);
    }
    if ( !tx_transmitting )
	return;

    tx_stop_transmit_sighandler(0);
}


static void
report_no_carrier( fsk_plan *fskp,
	unsigned int sample_rate,
	float bfsk_data_rate,
	float frame_n_bits,
	unsigned int nframes_decoded,
	size_t carrier_nsamples,
	float confidence_total,
	float amplitude_total )
{
    float nbits_decoded = nframes_decoded * frame_n_bits;
#if 0
    fprintf(stderr, "nframes_decoded=%u\n", nframes_decoded);
    fprintf(stderr, "nbits_decoded=%f\n", nbits_decoded);
    fprintf(stderr, "carrier_nsamples=%lu\n", carrier_nsamples);
#endif
    float throughput_rate =
		nbits_decoded * sample_rate / (float)carrier_nsamples;
    fprintf(stderr, "\n### NOCARRIER ndata=%u confidence=%.3f ampl=%.3f bps=%.2f",
	    nframes_decoded,
	    (double)(confidence_total / nframes_decoded),
	    (double)(amplitude_total / nframes_decoded),
	    (double)(throughput_rate));
#if 0
    fprintf(stderr, " bits*sr=%llu rate*nsamp=%llu",
	    (unsigned long long)(nbits_decoded * sample_rate + 0.5),
	    (unsigned long long)(bfsk_data_rate * carrier_nsamples) );
#endif
    if ( (unsigned long long)(nbits_decoded * sample_rate + 0.5f) == (unsigned long long)(bfsk_data_rate * carrier_nsamples) ) {
	fprintf(stderr, " (rate perfect) ###\n");
    } else {
	float throughput_skew = (throughput_rate - bfsk_data_rate)
			    / bfsk_data_rate;
	fprintf(stderr, " (%.1f%% %s) ###\n",
		(double)(fabsf(throughput_skew) * 100.0f),
		signbit(throughput_skew) ? "slow" : "fast"
		);
    }
}

void
generate_test_tones( simpleaudio *sa_out, unsigned int duration_sec )
{
    unsigned int sample_rate = simpleaudio_get_rate(sa_out);
    unsigned int nframes = sample_rate / 10;
    int i;
    for ( i=0; i<(sample_rate/nframes*duration_sec); i++ ) {
	simpleaudio_tone(sa_out, 1000, nframes/2);
	simpleaudio_tone(sa_out, 1777, nframes/2);
    }
}

static int
benchmarks()
{
    fprintf(stdout, "minimodem %s benchmarks\n", VERSION);

    int ret;
    ret = system("sed -n -e '/^model name/{p;q}' -e '/^cpu model/{p;q}' /proc/cpuinfo");
    if ( ret )
	;	// don't care, hush compiler.

    fflush(stdout);

    unsigned int sample_rate = 48000;
    sa_backend_t backend = SA_BACKEND_BENCHMARK;
    // backend = SA_BACKEND_SYSDEFAULT;	// for test

    simpleaudio *sa_out;


    // enable the sine wave LUT
    simpleaudio_tone_init(1024, 1.0);

    sa_out = simpleaudio_open_stream(backend, NULL, SA_STREAM_PLAYBACK,
			SA_SAMPLE_FORMAT_S16, sample_rate, 1,
			program_name, "generate-tones-lut1024-S16-mono");
    if ( ! sa_out )
	return 0;
    generate_test_tones(sa_out, 10);
    simpleaudio_close(sa_out);

    sa_out = simpleaudio_open_stream(backend, NULL, SA_STREAM_PLAYBACK,
			SA_SAMPLE_FORMAT_FLOAT, sample_rate, 1,
			program_name, "generate-tones-lut1024-FLOAT-mono");
    if ( ! sa_out )
	return 0;
    generate_test_tones(sa_out, 10);
    simpleaudio_close(sa_out);


    // disable the sine wave LUT
    simpleaudio_tone_init(0, 1.0);

    sa_out = simpleaudio_open_stream(backend, NULL, SA_STREAM_PLAYBACK,
			SA_SAMPLE_FORMAT_S16, sample_rate, 1,
			program_name, "generate-tones-nolut-S16-mono");
    if ( ! sa_out )
	return 0;
    generate_test_tones(sa_out, 10);
    simpleaudio_close(sa_out);

    sa_out = simpleaudio_open_stream(backend, NULL, SA_STREAM_PLAYBACK,
			SA_SAMPLE_FORMAT_FLOAT, sample_rate, 1,
			program_name, "generate-tones-nolut-FLOAT-mono");
    if ( ! sa_out )
	return 0;
    generate_test_tones(sa_out, 10);
    simpleaudio_close(sa_out);


    return 1;
}


static int rx_stop = 0;

void
rx_stop_sighandler( int sig )
{
    rx_stop = 1;
}


void
version()
{
    printf(
    "minimodem %s\n"
    "Copyright (C) 2011-2016 Kamal Mostafa <kamal@whence.com>\n"
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
    "		    -i, --inverted\n"
    "		    -c, --confidence {min-confidence-threshold}\n"
    "		    -l, --limit {max-confidence-search-limit}\n"
    "		    -8, --ascii		ASCII  8-N-1\n"
    "		    -7,			ASCII  7-N-1\n"
    "		    -5, --baudot	Baudot 5-N-1\n"
    "		    -f, --file {filename.flac}\n"
    "		    -b, --bandwidth {rx_bandwidth}\n"
    "		    -v, --volume {amplitude or 'E'}\n"
    "		    -M, --mark {mark_freq}\n"
    "		    -S, --space {space_freq}\n"
    "		    --startbits {n}\n"
    "		    --stopbits {n.n}\n"
    "		    --invert-start-stop\n"
    "		    --sync-byte {0xXX}\n"
    "		    -q, --quiet\n"
    "		    -R, --samplerate {rate}\n"
    "		    -V, --version\n"
    "		    -A, --alsa[=plughw:X,Y]\n"
    "		    --lut={tx_sin_table_len}\n"
    "		    --float-samples\n"
    "		    --rx-one\n"
    "		    --benchmarks\n"
    "		    --binary-output\n"
    "		    --binary-raw {nbits}\n"
    "		    --print-filter\n"
    "		    --tx-carrier\n"
    "		{baudmode}\n"
    "	    any_number_N       Bell-like      N bps --ascii\n"
    "		    1200       Bell202     1200 bps --ascii\n"
    "		     300       Bell103      300 bps --ascii\n"
    "		    rtty       RTTY       45.45 bps --baudot --stopbits=1.5\n"
    "		     tdd       TTY/TDD    45.45 bps --baudot --stopbits=2.0\n"
    "		    same       NOAA SAME 520.83 bps --sync-byte=0xAB ...\n"
    "		callerid       Bell202 CID 1200 bps\n"
    "     uic{-train,-ground}       UIC-751-3 Train/Ground 600 bps\n"
    );
    exit(1);
}

int
build_expect_bits_string( char *expect_bits_string,
	int bfsk_nstartbits,
	int bfsk_n_data_bits,
	float bfsk_nstopbits,
	int invert_start_stop,
	int use_expect_bits,
	unsigned long long expect_bits )
{
	// example expect_bits_string
	//	  0123456789A
	//	  isddddddddp	i == idle bit (a.k.a. prev_stop bit)
	//			s == start bit  d == data bits  p == stop bit
	// ebs = "10dddddddd1"  <-- expected mark/space framing pattern
	//
	// NOTE! expect_n_bits ends up being (frame_n_bits+1), because
	// we expect the prev_stop bit in addition to this frame's own
	// (start + n_data_bits + stop) bits.  But for each decoded frame,
	// we will advance just frame_n_bits worth of samples, leaving us
	// pointing at our stop bit -- it becomes the next frame's prev_stop.
	//
	//                  prev_stop--v
	//                       start--v        v--stop
	// char *expect_bits_string = "10dddddddd1";
	//
	char start_bit_value = invert_start_stop ? '1' : '0';
	char stop_bit_value = invert_start_stop ? '0' : '1';
	int j = 0;
	if ( bfsk_nstopbits != 0.0f )
	    expect_bits_string[j++] = stop_bit_value;
	int i;
	// Nb. only integer number of start bits works (for rx)
	for ( i=0; i<bfsk_nstartbits; i++ )
	    expect_bits_string[j++] = start_bit_value;
	for ( i=0; i<bfsk_n_data_bits; i++,j++ ) {
	    if ( use_expect_bits )
		expect_bits_string[j] = ( (expect_bits>>i)&1 ) + '0';
	    else
		expect_bits_string[j] = 'd';
	}
	if ( bfsk_nstopbits != 0.0f )
	    expect_bits_string[j++] = stop_bit_value;
	expect_bits_string[j] = 0;

	return j;
}

int
main( int argc, char*argv[] )
{
    char *modem_mode = NULL;
    int TX_mode = -1;
    int quiet_mode = 0;
    int output_print_filter = 0;
    float band_width = 0;
    float bfsk_mark_f = 0;
    float bfsk_space_f = 0;
    unsigned int bfsk_inverted_freqs = 0;
    int bfsk_nstartbits = -1;
    float bfsk_nstopbits = -1;
    unsigned int bfsk_do_rx_sync = 0;
    unsigned int bfsk_do_tx_sync_bytes = 0;
    unsigned long long bfsk_sync_byte = -1;
    unsigned int bfsk_n_data_bits = 0;
    int bfsk_msb_first = 0;
    char *expect_data_string = NULL;
    char *expect_sync_string = NULL;
    unsigned int expect_n_bits;
    int invert_start_stop = 0;
    int autodetect_shift;
    char *filename = NULL;

    float	carrier_autodetect_threshold = 0.0;

    // fsk_confidence_threshold : signal-to-noise squelch control
    //
    // The minimum SNR-ish confidence level seen as "a signal".
    float fsk_confidence_threshold = 1.5;

    // fsk_confidence_search_limit : performance vs. quality
    //
    // If we find a frame with confidence > confidence_search_limit,
    // quit searching for a better frame.  confidence_search_limit has a
    // dramatic effect on peformance (high value yields low performance, but
    // higher decode quality, for noisy or hard-to-discern signals (Bell 103,
    // or skewed rates).
    float fsk_confidence_search_limit = 2.3f;
    // float fsk_confidence_search_limit = INFINITY;  /* for test */

    sa_backend_t sa_backend = SA_BACKEND_SYSDEFAULT;
    char *sa_backend_device = NULL;
    sa_format_t sample_format = SA_SAMPLE_FORMAT_S16;
    unsigned int sample_rate = 48000;
    unsigned int nchannels = 1; // FIXME: only works with one channel

    float tx_amplitude = 1.0;
    unsigned int tx_sin_table_len = 4096;

    unsigned int rx_one = 0;
    float rxnoise_factor = 0.0;

    int txcarrier = 0;

    int output_mode_binary = 0;
    int output_mode_raw_nbits = 0;

    float	bfsk_data_rate = 0.0;
    databits_encoder	*bfsk_databits_encode;
    databits_decoder	*bfsk_databits_decode;

    bfsk_databits_decode = databits_decode_ascii8;
    bfsk_databits_encode = databits_encode_ascii8;

    /* validate the default system audio mechanism */
#if !(USE_PULSEAUDIO || USE_ALSA)
# define _MINIMODEM_NO_SYSTEM_AUDIO
# if !USE_SNDFILE
#  error At least one of {USE_PULSEAUDIO,USE_ALSA,USE_SNDFILE} must be enabled!
# endif
#endif

    program_name = strrchr(argv[0], '/');
    if ( program_name )
	program_name++;
    else
	program_name = argv[0];

    int c;
    int option_index;
    
    enum {
	MINIMODEM_OPT_UNUSED=256,	// placeholder
	MINIMODEM_OPT_MSBFIRST,
	MINIMODEM_OPT_STARTBITS,
	MINIMODEM_OPT_STOPBITS,
	MINIMODEM_OPT_INVERT_START_STOP,
	MINIMODEM_OPT_SYNC_BYTE,
	MINIMODEM_OPT_LUT,
	MINIMODEM_OPT_FLOAT_SAMPLES,
	MINIMODEM_OPT_RX_ONE,
	MINIMODEM_OPT_BENCHMARKS,
	MINIMODEM_OPT_BINARY_OUTPUT,
	MINIMODEM_OPT_BINARY_RAW,
	MINIMODEM_OPT_PRINT_FILTER,
	MINIMODEM_OPT_XRXNOISE,
	MINIMODEM_OPT_TXCARRIER
    };

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
	    { "limit",		1, 0, 'l' },
	    { "auto-carrier",	0, 0, 'a' },
	    { "inverted",	0, 0, 'i' },
	    { "ascii",		0, 0, '8' },
	    { "",		0, 0, '7' },
	    { "baudot",		0, 0, '5' },
	    { "msb-first",	0, 0, MINIMODEM_OPT_MSBFIRST },
	    { "file",		1, 0, 'f' },
	    { "bandwidth",	1, 0, 'b' },
	    { "volume",		1, 0, 'v' },
	    { "mark",		1, 0, 'M' },
	    { "space",		1, 0, 'S' },
	    { "startbits",	1, 0, MINIMODEM_OPT_STARTBITS },
	    { "stopbits",	1, 0, MINIMODEM_OPT_STOPBITS },
	    { "invert-start-stop", 0, 0, MINIMODEM_OPT_INVERT_START_STOP },
	    { "sync-byte",	1, 0, MINIMODEM_OPT_SYNC_BYTE },
	    { "quiet",		0, 0, 'q' },
	    { "alsa",		2, 0, 'A' },
	    { "samplerate",	1, 0, 'R' },
	    { "lut",		1, 0, MINIMODEM_OPT_LUT },
	    { "float-samples",	0, 0, MINIMODEM_OPT_FLOAT_SAMPLES },
	    { "rx-one",		0, 0, MINIMODEM_OPT_RX_ONE },
	    { "benchmarks",	0, 0, MINIMODEM_OPT_BENCHMARKS },
	    { "binary-output",	0, 0, MINIMODEM_OPT_BINARY_OUTPUT },
	    { "binary-raw",	1, 0, MINIMODEM_OPT_BINARY_RAW },
	    { "print-filter",	0, 0, MINIMODEM_OPT_PRINT_FILTER },
	    { "Xrxnoise",	1, 0, MINIMODEM_OPT_XRXNOISE },
	    { "tx-carrier",      0, 0, MINIMODEM_OPT_TXCARRIER },
	    { 0 }
	};
	c = getopt_long(argc, argv, "Vtrc:l:ai875f:b:v:M:S:T:qA::R:",
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
			fsk_confidence_threshold = atof(optarg);
			break;
	    case 'l':
			fsk_confidence_search_limit = atof(optarg);
			break;
	    case 'a':
			carrier_autodetect_threshold = 0.001;
			break;
	    case 'i':
			bfsk_inverted_freqs = 1;
			break;
	    case 'f':
			filename = optarg;
			break;
	    case '8':
			bfsk_n_data_bits = 8;
			break;
	    case '7':
			bfsk_n_data_bits = 7;
			break;
	    case '5':
			bfsk_n_data_bits = 5;
			bfsk_databits_decode = databits_decode_baudot;
			bfsk_databits_encode = databits_encode_baudot;
			break;
	    case MINIMODEM_OPT_MSBFIRST:
			bfsk_msb_first = 1;
			break;
	    case 'b':
			band_width = atof(optarg);
			assert( band_width != 0 );
			break;
	    case 'v':
			if ( optarg[0] == 'E' )
			    tx_amplitude = FLT_EPSILON;
			else
			    tx_amplitude = atof(optarg);
			assert( tx_amplitude > 0.0f );
			break;
	    case 'M':
			bfsk_mark_f = atof(optarg);
			assert( bfsk_mark_f > 0 );
			break;
	    case 'S':
			bfsk_space_f = atof(optarg);
			assert( bfsk_space_f > 0 );
			break;
	    case MINIMODEM_OPT_STARTBITS:
			bfsk_nstartbits = atoi(optarg);
			// Note: bfsk_nstartbits is limited by arrays
		        //   expect_bits_string[32] and fsk.c:bit_something[32]
			assert( bfsk_nstartbits >= 0 && bfsk_nstartbits <= 20 );
			break;
	    case MINIMODEM_OPT_STOPBITS:
			bfsk_nstopbits = atof(optarg);
			assert( bfsk_nstopbits >= 0 );
			break;
	    case MINIMODEM_OPT_INVERT_START_STOP:
			invert_start_stop = 1;
			break;
	    case MINIMODEM_OPT_SYNC_BYTE:
			bfsk_do_rx_sync = 1;
			bfsk_do_tx_sync_bytes = 16;
			bfsk_sync_byte = strtol(optarg, NULL, 0);
			break;
	    case 'q':
			quiet_mode = 1;
			break;
	    case 'R':
			sample_rate = atoi(optarg);
			assert( sample_rate > 0 );
			break;
	    case 'A':
#if USE_ALSA
			sa_backend = SA_BACKEND_ALSA;
			if ( optarg )
			    sa_backend_device = optarg;
#else
			fprintf(stderr, "E: This build of minimodem was configured without alsa support.\n");
			exit(1);
#endif
			break;
	    case MINIMODEM_OPT_LUT:
			tx_sin_table_len = atoi(optarg);
			break;
	    case MINIMODEM_OPT_FLOAT_SAMPLES:
			sample_format = SA_SAMPLE_FORMAT_FLOAT;
			break;
	    case MINIMODEM_OPT_RX_ONE:
			rx_one = 1;
			break;
	    case MINIMODEM_OPT_BENCHMARKS:
			benchmarks();
			exit(0);
			break;
	    case MINIMODEM_OPT_BINARY_OUTPUT:
			output_mode_binary = 1;
			break;
	    case MINIMODEM_OPT_BINARY_RAW:
			output_mode_raw_nbits = atoi(optarg);
			break;
	    case MINIMODEM_OPT_PRINT_FILTER:
			output_print_filter = 1;
			break;
	    case MINIMODEM_OPT_XRXNOISE:
			rxnoise_factor = atof(optarg);
			break;
	    case MINIMODEM_OPT_TXCARRIER:
			txcarrier = 1;
			break;
	    default:
			usage();
	}
    }
    if ( TX_mode == -1 )
	TX_mode = 0;

    /* The receive code requires floating point samples to feed to the FFT */
    if ( TX_mode == 0 )
	sample_format = SA_SAMPLE_FORMAT_FLOAT;

    if ( filename ) {
#if !USE_SNDFILE
	fprintf(stderr, "E: This build of minimodem was configured without sndfile,\nE:   so the --file flag is not supported.\n");
	exit(1);
#endif
    } else {
#ifdef _MINIMODEM_NO_SYSTEM_AUDIO
	fprintf(stderr, "E: this build of minimodem was configured without system audio support,\nE:   so only the --file mode is supported.\n");
	exit(1);
#endif
    }

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


    if ( strncasecmp(modem_mode, "rtty",5)==0 ) {
	bfsk_databits_decode = databits_decode_baudot;
	bfsk_databits_encode = databits_encode_baudot;
	bfsk_data_rate = 45.45;
	if ( bfsk_n_data_bits == 0 )
	    bfsk_n_data_bits = 5;
	if ( bfsk_nstopbits < 0 )
	    bfsk_nstopbits = 1.5;
    } else if ( strncasecmp(modem_mode, "tdd",4)==0 ) {
	bfsk_databits_decode = databits_decode_baudot;
	bfsk_databits_encode = databits_encode_baudot;
	bfsk_data_rate = 45.45;
	if ( bfsk_n_data_bits == 0 )
	    bfsk_n_data_bits = 5;
	if ( bfsk_nstopbits < 0 )
	    bfsk_nstopbits = 2.0;
	bfsk_mark_f = 1400;
	bfsk_space_f = 1800;
    } else if ( strncasecmp(modem_mode, "same",5)==0 ) {
	// http://www.nws.noaa.gov/nwr/nwrsame.htm
	bfsk_data_rate = 520.0 + 5/6.0;
	bfsk_n_data_bits = 8;
	bfsk_nstartbits = 0;
	bfsk_nstopbits = 0;
	bfsk_do_rx_sync = 1;
	bfsk_do_tx_sync_bytes = 16;
	bfsk_sync_byte = 0xAB;
	bfsk_mark_f = 2083.0 + 1/3.0;
	bfsk_space_f = 1562.5;
	band_width = bfsk_data_rate;
    } else if ( strncasecmp(modem_mode, "caller",6)==0 ) {
	if ( TX_mode ) {
	    fprintf(stderr, "E: callerid --tx mode is not supported.\n");
	    return 1;
	}
	if ( carrier_autodetect_threshold > 0.0f )
	    fprintf(stderr, "W: callerid with --auto-carrier is not recommended.\n");
	bfsk_databits_decode = databits_decode_callerid;
	bfsk_data_rate = 1200;
	bfsk_n_data_bits = 8;
    } else if ( strncasecmp(modem_mode, "uic", 3) == 0 ) {
	if ( TX_mode ) {
	    fprintf(stderr, "E: uic-751-3 --tx mode is not supported.\n");
	    return 1;
	}
	// http://ec.europa.eu/transport/rail/interoperability/doc/ccs-tsi-en-annex.pdf
	if (tolower(modem_mode[4]) == 't')
	    bfsk_databits_decode = databits_decode_uic_train;
	else
	    bfsk_databits_decode = databits_decode_uic_ground;
	bfsk_data_rate = 600;
	bfsk_n_data_bits = 39;
	bfsk_mark_f = 1300;
	bfsk_space_f = 1700;
	bfsk_nstartbits = 8;
	bfsk_nstopbits = 0;
	expect_data_string = "11110010ddddddddddddddddddddddddddddddddddddddd";
	expect_n_bits = 47;
    } else {
	bfsk_data_rate = atof(modem_mode);
	if ( bfsk_n_data_bits == 0 )
	    bfsk_n_data_bits = 8;
    }
    if ( bfsk_data_rate == 0.0f )
	usage();


    if ( output_mode_binary || output_mode_raw_nbits )
	bfsk_databits_decode = databits_decode_binary;

    if ( output_mode_raw_nbits ) {
	bfsk_nstartbits = 0;
	bfsk_nstopbits = 0;
	bfsk_n_data_bits = output_mode_raw_nbits;
    }

    if ( bfsk_data_rate >= 400 ) {
	/*
	 * Bell 202:     baud=1200 mark=1200 space=2200
	 */
	autodetect_shift = - ( bfsk_data_rate * 5 / 6 );
	if ( bfsk_mark_f == 0 )
	    bfsk_mark_f  = bfsk_data_rate / 2 + 600;
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
	if ( band_width == 0 ) {
	    band_width = 10;	// FIXME chosen arbitrarily
	}
    }

    // defaults: 1 start bit, 1 stop bit
    if ( bfsk_nstartbits < 0 )
	bfsk_nstartbits = 1;
    if ( bfsk_nstopbits < 0 )
	bfsk_nstopbits = 1.0;

    // do not transmit any leader tone if no start bits
    if ( bfsk_nstartbits == 0 )
	tx_leader_bits_len = 0;

    if ( bfsk_inverted_freqs ) {
	float t = bfsk_mark_f;
	bfsk_mark_f = bfsk_space_f;
	bfsk_space_f = t;
    }

    /* restrict band_width to <= data rate (FIXME?) */
    if ( band_width > bfsk_data_rate )
	band_width = bfsk_data_rate;

    // sanitize confidence search limit
    if ( fsk_confidence_search_limit < fsk_confidence_threshold )
	fsk_confidence_search_limit = fsk_confidence_threshold;

    char *stream_name = NULL;

    if ( filename ) {
	sa_backend = SA_BACKEND_FILE;
	stream_name = filename;
    }

    /*
     * Handle transmit mode
     */
    if ( TX_mode ) {

	simpleaudio_tone_init(tx_sin_table_len, tx_amplitude);

	int tx_interactive = 0;
	if ( ! stream_name ) {
	    tx_interactive = 1;
	    stream_name = "output audio";
	}

	simpleaudio *sa_out;
	sa_out = simpleaudio_open_stream(sa_backend, sa_backend_device,
					SA_STREAM_PLAYBACK,
					sample_format, sample_rate, nchannels,
					program_name, stream_name);
	if ( ! sa_out )
	    return 1;

	fsk_transmit_stdin(sa_out, tx_interactive,
				bfsk_data_rate,
				bfsk_mark_f, bfsk_space_f,
				bfsk_n_data_bits,
				bfsk_nstartbits,
				bfsk_nstopbits,
				invert_start_stop,
				bfsk_msb_first,
				bfsk_do_tx_sync_bytes,
				bfsk_sync_byte,
				bfsk_databits_encode,
				txcarrier
				);

	simpleaudio_close(sa_out);

	return 0;
    }

    /*
     * Open the input audio stream
     */

    if ( ! stream_name )
	stream_name = "input audio";

    simpleaudio *sa;
    sa = simpleaudio_open_stream(sa_backend, sa_backend_device,
				SA_STREAM_RECORD,
				sample_format, sample_rate, nchannels,
				program_name, stream_name);
    if ( ! sa )
        return 1;

    sample_rate = simpleaudio_get_rate(sa);

    if ( rxnoise_factor != 0.0f )
	simpleaudio_set_rxnoise(sa, rxnoise_factor);

    /*
     * Prepare the input sample chunk rate
     */
    float nsamples_per_bit = sample_rate / bfsk_data_rate;


    /*
     * Prepare the fsk plan
     */

    fsk_plan *fskp;
    fskp = fsk_plan_new(sample_rate, bfsk_mark_f, bfsk_space_f, band_width);
    if ( !fskp ) {
        fprintf(stderr, "fsk_plan_new() failed\n");
        return 1;
    }

    /*
     * Prepare the input sample buffer.  For 8-bit frames with prev/start/stop
     * we need 11 data-bits worth of samples, and we will scan through one bits
     * worth at a time, hence we need a minimum total input buffer size of 12
     * data-bits.  */
    unsigned int nbits = 0;
    nbits += 1;			// prev stop bit (last whole stop bit)
    nbits += bfsk_nstartbits;	// start bits
    nbits += bfsk_n_data_bits;
    nbits += 1;			// stop bit (first whole stop bit)

    // FIXME EXPLAIN +1 goes with extra bit when scanning
    size_t	samplebuf_size = ceilf(nsamples_per_bit) * (nbits+1);
    samplebuf_size *= 2; // account for the half-buf filling method
#define SAMPLE_BUF_DIVISOR 12
#ifdef SAMPLE_BUF_DIVISOR
    // For performance, use a larger samplebuf_size than necessary
    if ( samplebuf_size < sample_rate / SAMPLE_BUF_DIVISOR )
	samplebuf_size = sample_rate / SAMPLE_BUF_DIVISOR;
#endif
    float	*samplebuf = malloc(samplebuf_size * sizeof(float));
    size_t	samples_nvalid = 0;
    debug_log("samplebuf_size=%zu\n", samplebuf_size);

    /*
     * Run the main loop
     */

    int			ret = 0;

    int			carrier = 0;
    float		confidence_total = 0;
    float		amplitude_total = 0;
    unsigned int	nframes_decoded = 0;
    size_t		carrier_nsamples = 0;

    unsigned int	noconfidence = 0;
    unsigned int	advance = 0;

    // Fraction of nsamples_per_bit that we will "overscan"; range (0.0 .. 1.0)
    float fsk_frame_overscan = 0.5;
    //   should be != 0.0 (only the nyquist edge cases actually require this?)
    // for handling of slightly faster-than-us rates:
    //   should be >> 0.0 to allow us to lag back for faster-than-us rates
    //   should be << 1.0 or we may lag backwards over whole bits
    // for optimal analysis:
    //   should be >= 0.5 (half a bit width) or we may not find the optimal bit
    //   should be <  1.0 (a full bit width) or we may skip over whole bits
    // for encodings without start/stop bits:
    //     MUST be <= 0.5 or we may accidentally skip a bit
    //
    assert( fsk_frame_overscan >= 0.0f && fsk_frame_overscan < 1.0f );

    // ensure that we overscan at least a single sample
    unsigned int nsamples_overscan
			= nsamples_per_bit * fsk_frame_overscan + 0.5f;
    if ( fsk_frame_overscan > 0.0f && nsamples_overscan == 0 )
	nsamples_overscan = 1;
    debug_log("fsk_frame_overscan=%f nsamples_overscan=%u\n",
	    fsk_frame_overscan, nsamples_overscan);

    // n databits plus bfsk_startbit start bits plus bfsk_nstopbit stop bits:
    float frame_n_bits = bfsk_n_data_bits + bfsk_nstartbits + bfsk_nstopbits;
    unsigned int frame_nsamples = nsamples_per_bit * frame_n_bits + 0.5f;

    char expect_data_string_buffer[64];
    if (expect_data_string == NULL) {
        expect_data_string = expect_data_string_buffer;
        expect_n_bits = build_expect_bits_string(expect_data_string, bfsk_nstartbits, bfsk_n_data_bits, bfsk_nstopbits, invert_start_stop, 0, 0);
    }
    debug_log("eds = '%s' (%lu)\n", expect_data_string, strlen(expect_data_string));

    char expect_sync_string_buffer[64];
    if (expect_sync_string == NULL && bfsk_do_rx_sync && (long long) bfsk_sync_byte >= 0) {
        expect_sync_string = expect_sync_string_buffer;
        build_expect_bits_string(expect_sync_string, bfsk_nstartbits, bfsk_n_data_bits, bfsk_nstopbits, invert_start_stop, 1, bfsk_sync_byte);
    } else {
	expect_sync_string = expect_data_string;
    }
    debug_log("ess = '%s' (%lu)\n", expect_sync_string, strlen(expect_sync_string));

	unsigned int expect_nsamples = nsamples_per_bit * expect_n_bits;
    float track_amplitude = 0.0;
    float peak_confidence = 0.0;

    signal(SIGINT, rx_stop_sighandler);

    while ( 1 ) {

	if ( rx_stop )
	    break;

	debug_log("advance=%u\n", advance);

	/* Shift the samples in samplebuf by 'advance' samples */
	assert( advance <= samplebuf_size );
	if ( advance == samplebuf_size ) {
	    samples_nvalid = 0;
	    advance = 0;
	}
	if ( advance ) {
	    if ( advance > samples_nvalid )
		break;
	    memmove(samplebuf, samplebuf+advance,
		    (samplebuf_size-advance)*sizeof(float));
	    samples_nvalid -= advance;
	}

	if ( samples_nvalid < samplebuf_size/2 ) {
	    float	*samples_readptr = samplebuf + samples_nvalid;
	    size_t	read_nsamples = samplebuf_size/2;
	    /* Read more samples into samplebuf (fill it) */
	    assert ( read_nsamples > 0 );
	    assert ( samples_nvalid + read_nsamples <= samplebuf_size );
	    ssize_t r;
	    r = simpleaudio_read(sa, samples_readptr, read_nsamples);
	    debug_log("simpleaudio_read(samplebuf+%td, n=%zu) returns %zd\n",
		    samples_readptr - samplebuf, read_nsamples, r);
	    if ( r < 0 ) {
		fprintf(stderr, "simpleaudio_read: error\n");
		ret = -1;
		break;
	    }
	    samples_nvalid += r;
	}

	if ( samples_nvalid == 0 )
	    break;

	/* Auto-detect carrier frequency */
	static int carrier_band = -1;
	if ( carrier_autodetect_threshold > 0.0f && carrier_band < 0 ) {
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

	    // default negative shift -- reasonable?
	    int b_shift = - (float)(autodetect_shift + fskp->band_width/2.0f)
						/ fskp->band_width;
	    if ( bfsk_inverted_freqs )
		b_shift *= -1;
	    /* only accept a carrier as b_mark if it will not result
	     * in a b_space band which is "too low". */
	    int b_space = carrier_band + b_shift;
	    if ( b_space < 1 || b_space >= fskp->nbands ) {
		debug_log("autodetected space band out of range\n" );
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

	if ( samples_nvalid < expect_nsamples )
	    break;

	// try_max_nsamples
	// serves two purposes
	// 1. avoids finding a non-optimal first frame
	// 2. allows us to track slightly slow signals
	unsigned int try_max_nsamples;
	if ( carrier )
	    try_max_nsamples = nsamples_per_bit * 0.75f + 0.5f;
	else
	    try_max_nsamples = nsamples_per_bit;
	try_max_nsamples += nsamples_overscan;

	// FSK_ANALYZE_NSTEPS Try 3 frame positions across the try_max_nsamples
	// range.  Using a larger nsteps allows for more accurate tracking of
	// fast/slow signals (at decreased performance).  Note also
	// FSK_ANALYZE_NSTEPS_FINE below, which refines the frame
	// position upon first acquiring carrier, or if confidence falls.
#define FSK_ANALYZE_NSTEPS		3
	unsigned int try_step_nsamples = try_max_nsamples / FSK_ANALYZE_NSTEPS;
	if ( try_step_nsamples == 0 )
	    try_step_nsamples = 1;

	float confidence, amplitude;
	unsigned long long bits = 0;
	/* Note: frame_start_sample is actually the sample where the
	 * prev_stop bit begins (since the "frame" includes the prev_stop). */
	unsigned int frame_start_sample = 0;

	unsigned int try_first_sample;
	float try_confidence_search_limit;

	try_confidence_search_limit = fsk_confidence_search_limit;
	try_first_sample = carrier ? nsamples_overscan : 0;

	confidence = fsk_find_frame(fskp, samplebuf, expect_nsamples,
			try_first_sample,
			try_max_nsamples,
			try_step_nsamples,
			try_confidence_search_limit,
			carrier ? expect_data_string : expect_sync_string,
			&bits,
			&amplitude,
			&frame_start_sample
			);

	int do_refine_frame = 0;

	if ( confidence < peak_confidence * 0.75f ) {
	    do_refine_frame = 1;
	    debug_log(" ... do_refine_frame rescan (confidence %.3f << %.3f peak)\n", confidence, peak_confidence);
	    peak_confidence = 0;
	}

	// no-confidence if amplitude drops abruptly to < 25% of the
	// track_amplitude, which follows amplitude with hysteresis
	if ( amplitude < track_amplitude * 0.25f ) {
	    confidence = 0;
	}

#define FSK_MAX_NOCONFIDENCE_BITS	20

	if ( confidence <= fsk_confidence_threshold ) {

	    // FIXME: explain
	    if ( ++noconfidence > FSK_MAX_NOCONFIDENCE_BITS )
	    {
		carrier_band = -1;
		if ( carrier ) {
		    if ( !quiet_mode )
			report_no_carrier(fskp, sample_rate, bfsk_data_rate,
			    frame_n_bits, nframes_decoded,
			    carrier_nsamples, confidence_total, amplitude_total);
		    carrier = 0;
		    carrier_nsamples = 0;
		    confidence_total = 0;
		    amplitude_total = 0;
		    nframes_decoded = 0;
		    track_amplitude = 0.0;

		    if ( rx_one )
			break;
		}
	    }

	    /* Advance the sample stream forward by try_max_nsamples so the
	     * next time around the loop we continue searching from where
	     * we left off this time.		*/
	    advance = try_max_nsamples;
	    debug_log("@ NOCONFIDENCE=%u advance=%u\n", noconfidence, advance);
	    continue;
	}

	// Add a frame's worth of samples to the sample count
	carrier_nsamples += frame_nsamples;

	if ( carrier ) {

	    // If we already had carrier, adjust sample count +start -overscan
	    carrier_nsamples += frame_start_sample;
	    carrier_nsamples -= nsamples_overscan;

	} else {

	    // We just acquired carrier.

	    if ( !quiet_mode ) {
		if ( bfsk_data_rate >= 100 )
		    fprintf(stderr, "### CARRIER %u @ %.1f Hz ",
			    (unsigned int)(bfsk_data_rate + 0.5f),
			    (double)(fskp->b_mark * fskp->band_width));
		else
		    fprintf(stderr, "### CARRIER %.2f @ %.1f Hz ",
			    (double)(bfsk_data_rate),
			    (double)(fskp->b_mark * fskp->band_width));
	    }

	    if ( !quiet_mode )
		fprintf(stderr, "###\n");

	    carrier = 1;
	    bfsk_databits_decode(0, 0, 0, 0); // reset the frame processor

	    do_refine_frame = 1;
	    debug_log(" ... do_refine_frame rescan (acquired carrier)\n");
	}

	if ( do_refine_frame )
	{
	    if ( confidence < INFINITY && try_step_nsamples > 1 ) {
		// FSK_ANALYZE_NSTEPS_FINE:
		// Scan again, but try harder to find the best frame.
		// Since we found a valid confidence frame in the "sloppy"
		// fsk_find_frame() call already, we're sure to find one at
		// least as good this time.
#define FSK_ANALYZE_NSTEPS_FINE		8
		try_step_nsamples = try_max_nsamples / FSK_ANALYZE_NSTEPS_FINE;
		if ( try_step_nsamples == 0 )
		    try_step_nsamples = 1;
		try_confidence_search_limit = INFINITY;
		float confidence2, amplitude2;
		unsigned long long bits2;
		unsigned int frame_start_sample2;
		confidence2 = fsk_find_frame(fskp, samplebuf, expect_nsamples,
			    try_first_sample,
			    try_max_nsamples,
			    try_step_nsamples,
			    try_confidence_search_limit,
			    carrier ? expect_data_string : expect_sync_string,
			    &bits2,
			    &amplitude2,
			    &frame_start_sample2
			    );
		if ( confidence2 > confidence ) {
		    bits = bits2;
		    amplitude = amplitude2;
		    frame_start_sample = frame_start_sample2;
		}
	    }
	}

	track_amplitude = ( track_amplitude + amplitude ) / 2;
	if ( peak_confidence < confidence )
	    peak_confidence = confidence;
	debug_log("@ confidence=%.3f peak_conf=%.3f amplitude=%.3f track_amplitude=%.3f\n",
		confidence, peak_confidence, amplitude, track_amplitude );

	confidence_total += confidence;
	amplitude_total += amplitude;
	nframes_decoded++;
	noconfidence = 0;

	// Advance the sample stream forward past the junk before the
	// frame starts (frame_start_sample), and then past decoded frame
	// (see also NOTE about frame_n_bits and expect_n_bits)...
	// But actually advance just a bit less than that to allow
	// for tracking slightly fast signals, hence - nsamples_overscan.
	advance = frame_start_sample + frame_nsamples - nsamples_overscan;

	debug_log("@ nsamples_per_bit=%.3f n_data_bits=%u "
			" frame_start=%u advance=%u\n",
		    nsamples_per_bit, bfsk_n_data_bits,
		    frame_start_sample, advance);

	// chop off the prev_stop bit
	if ( bfsk_nstopbits != 0.0f )
	    bits = bits >> 1;


	/*
	 * Send the raw data frame bits to the backend frame processor
	 * for final conversion to output data bytes.
	 */

	// chop off framing bits
	bits = bit_window(bits, bfsk_nstartbits, bfsk_n_data_bits);
	if (bfsk_msb_first) {
		bits = bit_reverse(bits, bfsk_n_data_bits);
	}
	debug_log("Input: %08x%08x - Databits: %u - Shift: %i\n", (unsigned int)(bits >> 32), (unsigned int)bits, bfsk_n_data_bits, bfsk_nstartbits);

	unsigned int dataout_size = 4096;
	char dataoutbuf[4096];
	unsigned int dataout_nbytes = 0;

	// suppress printing of bfsk_sync_byte bytes
	if ( bfsk_do_rx_sync ) {
	    if ( dataout_nbytes == 0 && bits == bfsk_sync_byte )
		continue;
	}

	dataout_nbytes += bfsk_databits_decode(dataoutbuf + dataout_nbytes,
						dataout_size - dataout_nbytes,
						bits, (int)bfsk_n_data_bits);

	if ( dataout_nbytes == 0 )
	    continue;

	/*
	 * Print the output buffer to stdout
	 */
	if ( output_print_filter == 0 ) {
	    if ( write(1, dataoutbuf, dataout_nbytes) < 0 )
		perror("write");
	} else {
	    char *p = dataoutbuf;
	    for ( ; dataout_nbytes; p++,dataout_nbytes-- ) {
		char printable_char = isprint(*p)||isspace(*p) ? *p : '.';
		if ( write(1, &printable_char, 1) < 0 )
		    perror("write");
	    }
	}

    } /* end of the main loop */

    free(samplebuf);

    signal(SIGINT, SIG_DFL);

    if ( carrier ) {
	if ( !quiet_mode )
	    report_no_carrier(fskp, sample_rate, bfsk_data_rate,
		frame_n_bits, nframes_decoded,
		carrier_nsamples, confidence_total, amplitude_total);
    }

    simpleaudio_close(sa);

    fsk_plan_destroy(fskp);

    return ret;
}
