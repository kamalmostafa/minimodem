/*
 * fsk.h
 *
 * Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
 *
 * NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
 *
 */



#define USE_FFT		// leave this enabled; its presently the only choice

#ifdef USE_FFT
#include <fftw3.h>
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
	int		fftsize;
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
	);

void
fsk_plan_destroy( fsk_plan *fskp );

/* returns confidence value [0.0 to 1.0] */
float
fsk_find_frame( fsk_plan *fskp, float *samples, unsigned int frame_nsamples,
	unsigned int try_max_nsamples,
	unsigned int try_step_nsamples,
	unsigned int *bits_outp,
	unsigned int *frame_start_outp
	);

int
fsk_detect_carrier(fsk_plan *fskp, float *samples, unsigned int nsamples,
	float min_mag_threshold );

void
fsk_set_tones_by_bandshift( fsk_plan *fskp, unsigned int b_mark, int b_shift );


// FIXME move this?:
//#define FSK_DEBUG
#ifdef FSK_DEBUG
# define debug_log(format, args...)  fprintf(stderr, format, ## args)
#else
# define debug_log(format, args...)
#endif

