AM_CFLAGS = -Wall -Werror

INCLUDES = $(DEPS_CFLAGS)


bin_PROGRAMS = minimodem tscope

SIMPLEAUDIO_SRC=\
	simpleaudio.c		\
	simpleaudio-pulse.c	\
	simpleaudio-sndfile.c

minimodem_LDADD = $(DEPS_LIBS)
minimodem_SOURCES = minimodem.c fsk.c $(SIMPLEAUDIO_SRC)

tscope_LDADD = $(DEPS_LIBS)
tscope_SOURCES = tscope.c tscope_print.c
