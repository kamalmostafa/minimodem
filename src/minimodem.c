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

#include <fftw3.h>

#include "simpleaudio.h"

#include "tscope_print.h"

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

static inline
float
band_mag( fftwf_complex * const cplx, unsigned int band, float scalar )
{
    float re = cplx[band][0];
    float im = cplx[band][1];
    float mag = hypot(re, im) * scalar;
    return mag;
}

int main(int argc, char*argv[]) {

    int ret = 1;

    if ( argc < 2 ) {
	fprintf(stderr, "usage: minimodem [filename] baud_rate [ mark_hz space_hz ]\n");
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
    unsigned int nchannels = simpleaudio_get_channels(sa);


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
    band_width = 10;
    // band_width = 50;	/* close enough? */

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

    unsigned int bfsk_mark_band  = (bfsk_mark_f  +(float)band_width/2) / band_width;
    unsigned int bfsk_space_band = (bfsk_space_f +(float)band_width/2) / band_width;


    if ( bfsk_mark_band == 0 || bfsk_space_band == 0 ) {
        fprintf(stderr, __FILE__": mark or space band is at dsp DC\n");
//	return 1;
    }
    if ( bfsk_mark_band == bfsk_space_band ) {
        fprintf(stderr, __FILE__": inadequate mark/space separation\n");
	return 1;
    }



    /* Create the FFT plan */
    fftwf_plan	fftplan;

    int fftsize = sample_rate / band_width;

    if ( fftsize & 1 )
        fprintf(stderr, __FILE__": WARNING: fftsize %u is not even\n", fftsize);

    unsigned int nbands = fftsize / 2 + 1;

    float *fftin = fftwf_malloc(fftsize * sizeof(float) * nchannels);

    fftwf_complex *fftout = fftwf_malloc(nbands * sizeof(fftwf_complex) * nchannels);

    /*
     * works only for 1 channel:
    fftplan = fftwf_plan_dft_r2c_1d(fftsize, fftin, fftout, FFTW_ESTIMATE);
     */
    /*
     * works for N channels:
     */
    fftplan = fftwf_plan_many_dft_r2c(
	    /*rank*/1, &fftsize, /*howmany*/nchannels,
	    fftin, NULL, /*istride*/nchannels, /*idist*/1,
	    fftout, NULL, /*ostride*/1, /*odist*/nbands,
	    FFTW_ESTIMATE | FFTW_PRESERVE_INPUT );
    /* Nb. FFTW_PRESERVE_INPUT is needed for the "shift the input window" trick */

    if ( !fftplan ) {
        fprintf(stderr, __FILE__": fftwf_plan_dft_r2c_1d() failed\n");
        return 1;
    }


    /*
     * Prepare the input sample chunk rate
     */
    int nsamples = sample_rate / decode_rate;

    /* BLACK MAGIC! Run the decoder 2% fast ... */
    int nsamples_adjust = nsamples * 0.02;
    if ( nsamples_adjust == 0 )
	nsamples_adjust = 1;
    nsamples -= nsamples_adjust;

    /* normalize fftw output */
    float magscalar = 1.0 / ((float)nsamples/2.0);

    /* pulseaudio *adds* when downmixing 2 channels to 1; if we're using
     * only one channel here, we blindly assume that pulseaudio downmixed
     * from 2, and rescale magnitudes accordingly. */
// but sndfile does not do that.
//    if ( nchannels == 1 )
//	magscalar /= 2.0;

    float actual_decode_rate = (float)sample_rate / nsamples;
    fprintf(stderr, "### nsamples=%u ", nsamples);
    // fprintf(stderr, "### magscalar=%f ", magscalar);
    fprintf(stderr, "### baud=%.2f mark=%u space=%u ###\n",
	    actual_decode_rate,
	    bfsk_mark_band * band_width,
	    bfsk_space_band * band_width
	    );


    /* Prepare the text scope output buffer */
    // sadly, COLUMNS is not exported by default (?)
    char *columns_env = getenv("COLUMNS");
    int columns = columns_env ? atoi(columns_env) : 80;
    int show_nbands = ( (columns - 2 - 10) / nchannels ) - 1 - 10;
    if ( show_nbands > nbands )
         show_nbands = nbands;


    /*
     * Run the main loop
     */

    unsigned int bfsk_bits = 0xFFFFFFFF;
    unsigned char carrier_detected = 0;

    unsigned long long carrier_nsamples = 0;
    unsigned long long carrier_nsymbits = 0;

    ret = 0;

    while ( 1 ) {

	bzero(fftin, (fftsize * sizeof(float) * nchannels));

	size_t	nframes = nsamples;
	/* read samples directly into fftin */
	if ((ret=simpleaudio_read(sa, fftin, nframes)) <= 0)
            break;
	carrier_nsamples += nframes;

#define TRICK

#ifdef TRICK
reprocess_audio:
	{
	}
#endif

	fftwf_execute(fftplan);

	/* examine channel 0 only */
	float mag_mark  = band_mag(fftout, bfsk_mark_band, magscalar);
	float mag_space = band_mag(fftout, bfsk_space_band, magscalar);


	static unsigned char lastbit;

#if 0	// TEST -- doesn't help?, sometimes gets it wrong
	// "clarify" mag_mark and mag_space according to whether lastbit
	// was a mark or a space ...  If lastbit was a mark, then enhance
	// space and vice-versa
	float clarify_factor = 2.0;
	if ( lastbit ) {
	    mag_space *= clarify_factor;
	} else {
	    mag_mark  *= clarify_factor;
	}
#endif

	float msdelta = mag_mark - mag_space;


#define CD_MIN_TONEMAG		0.1
#define CD_MIN_MSDELTA_RATIO	0.5

	/* Detect bfsk carrier */
	int carrier_detect /*boolean*/ =
	    1
	    && mag_mark + mag_space > CD_MIN_TONEMAG

//	    && MIN(mag_mark, mag_space) < 0.30
//	    && MAX(mag_mark, mag_space) > 0.80

//	    && fabs(msdelta) > CD_MIN_MSDELTA_RATIO * MAX(mag_mark, mag_space)
	    && fabs(msdelta) > 0.2 * (mag_mark+mag_space)
//	    && fabs(msdelta) > 0.5
	    ;

#ifdef TRICK

	// EXCELLENT trick -- fixes 300 baud perfectly
	// shift the input window to "catch up" if the msdelta is small
	static unsigned int skipped_frames = 0;
	if ( ! carrier_detect )
	{

	    if ( nframes == nsamples ) {
#if  1
		/* shift by a fraction of the width of one data bit
		 * any of these could work ... */
//		nframes = nsamples / 2;
//		nframes = nsamples / 4;
//		nframes = nsamples / 8;
//		nframes = nsamples / 16;
		nframes = 1;
		nframes = nframes ? nframes : 1;
#endif
	    }

	    // clamp the shift to half the bit width
	    if ( skipped_frames + nframes > nsamples/2 )
		nframes = 0;
	    else
		skipped_frames += nframes;

	    if ( nframes ) {
		size_t	framesize = nchannels * sizeof(float);
		size_t	nbytes = nframes * framesize;
		size_t	reuse_bytes = (nsamples-nframes)*framesize;
		void *in = fftin;
		memmove(in, in+nbytes, reuse_bytes);
		in += reuse_bytes;
		/* read samples directly into fftin */
		if ((ret=simpleaudio_read(sa, in, nframes)) <= 0)
		    break;
		carrier_nsamples += nframes;

	if ( textscope ) {
	    int one_line_mode = 0;
	    int show_maxmag = 1;
	    tscope_print(fftout, show_nbands, magscalar,
				one_line_mode, show_maxmag);
	    printf("\n");
	}

		goto reprocess_audio;
	    }
	}

	if ( carrier_detect && textscope ) {
	    if ( skipped_frames )
		fprintf(stderr, "<skipped %u (of %u) frames>\n",
			skipped_frames, nsamples);
	}

#endif

	unsigned char bit;


	if ( carrier_detect )
	{
	    carrier_nsymbits++;
	    bit = signbit(msdelta) ? 0 : 1;

#if 0
	    static unsigned char lastbit_strong = 0;
	    if ( fabs(msdelta) < 0.5 ) {		// TEST
		if ( lastbit_strong )
		    bit = !lastbit;
		lastbit_strong = 0;
	    } else {
		lastbit_strong = 1;
	    }
#endif

	}
	else
	    bit = 1;

	skipped_frames = 0;

	lastbit = bit;

	// save 11 bits:
	//           stop--- v        v--- start bit
	//                   v         v--- prev stop bit
	//                   1dddddddd01
	bfsk_bits = (bfsk_bits>>1) | (bit << 10);

	if ( ! carrier_detect )
	{
	    if ( carrier_detected ) {
		float samples_per_bit =
			    (float)carrier_nsamples / carrier_nsymbits;
		float rx_baud_rate =
			    (float)sample_rate / samples_per_bit;
		fprintf(stderr, "###NOCARRIER (bits=%llu rx=%.2f baud) ###\n",
			carrier_nsymbits,
			rx_baud_rate);
		carrier_detected = 0;
	    }
	    continue;
	}

	if ( ! carrier_detected ) {
	    fprintf(stderr, "###CARRIER###\n");
	    carrier_nsamples = 0;
	    carrier_nsymbits = 0;
	}
	carrier_detected = carrier_detect;

	if ( textscope ) {

//	    printf("%s %c ",
//		    carrier_detected ? "CD" : "  ",
//		    carrier_detected ? ( bit ? '1' : '0' ) : ' ');

	    int one_line_mode = 0;
	    int show_maxmag = 1;
	    tscope_print(fftout, show_nbands, magscalar,
				one_line_mode, show_maxmag);

	    printf(" ");
	    int i;
	    for ( i=(11-1); i>=0; i-- )
		printf("%c", bfsk_bits & (1<<i) ? '1' : '0');
	    printf(" ");
	    fflush(stdout);
	}

	if ( ! carrier_detected ) {
	    if ( textscope ) {
		printf("\n");
		fflush(stdout);
	    }
	    continue;

	}

	//           stop--- v        v--- start bit
	//                   v         v--- prev stop bit
	if ( ( bfsk_bits & 0b10000000011 )
			== 0b10000000001 ) {  // valid frame: start=space, stop=mark
	    unsigned char byte = ( bfsk_bits >> 2) & 0xFF;
	    if ( textscope )
		printf("+");
	    if ( byte == 0xFF ) {

		if ( textscope )
		    printf("idle");

	    } else {

		printf("%c", isspace(byte)||isprint(byte) ? byte : '.');
		fflush(stdout);

	    }
	    bfsk_bits = 1 << 10;
	}
	if ( textscope ) {
	    printf("\n");
	    fflush(stdout);
	}


    }

    if ( ret != 0 )
	fprintf(stderr, "simpleaudio_read: error\n");

    simpleaudio_close(sa);

    fftwf_free(fftin);
    fftwf_free(fftout);
    fftwf_destroy_plan(fftplan);

    return ret;
}
