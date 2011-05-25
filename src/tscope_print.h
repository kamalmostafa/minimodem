/*
 * tscope_print.h
 *
 * Author: Kamal Mostafa <kamal@whence.com>
 *
 * Unpublished work, not licensed for any purpose.
 *
 */

#include <fftw3.h>

extern void
tscope_print( fftwf_complex * const fftout, int nbands, float magscalar,
	int one_line_mode, int show_maxmag );

