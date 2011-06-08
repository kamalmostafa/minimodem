# 
# Makefile.am
# 
# Copyright (C) 2011 Kamal Mostafa <kamal@whence.com>
# 
# NO LICENSE HAS BEEN SPECIFIED OR GRANTED FOR THIS WORK.
# 
# 

AM_CFLAGS = -Wall -Werror

INCLUDES = $(DEPS_CFLAGS)


bin_PROGRAMS = minimodem

EXTRA_DIST = run-test


SIMPLEAUDIO_SRC = \
	simpleaudio.h		\
	simpleaudio_internal.h	\
	simpleaudio.c		\
	simpleaudio-pulse.c	\
	simpleaudio-sndfile.c

FSK_SRC = fsk.h fsk.c

minimodem_LDADD = $(DEPS_LIBS)
minimodem_SOURCES = minimodem.c $(FSK_SRC) $(SIMPLEAUDIO_SRC)


