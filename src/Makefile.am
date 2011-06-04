AM_CFLAGS = -Wall -Werror

INCLUDES = $(DEPS_CFLAGS)


bin_PROGRAMS = minimodem tscope fsk

SIMPLEAUDIO_SRC=\
	simpleaudio.c		\
	simpleaudio-pulse.c	\
	simpleaudio-sndfile.c

minimodem_LDADD = $(DEPS_LIBS)
minimodem_SOURCES = minimodem.c tscope_print.c $(SIMPLEAUDIO_SRC)

fsk_LDADD = $(DEPS_LIBS)
fsk_SOURCES = fsk.c tscope_print.c $(SIMPLEAUDIO_SRC)

tscope_LDADD = $(DEPS_LIBS)
tscope_SOURCES = tscope.c tscope_print.c
