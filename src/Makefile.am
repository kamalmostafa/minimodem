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

EXTRA_DIST = self-test # run-test


SIMPLEAUDIO_SRC = \
	simpleaudio.h		\
	simpleaudio_internal.h	\
	simpleaudio.c		\
	simple-tone-generator.c \
	simpleaudio-pulse.c	\
	simpleaudio-sndfile.c

FSK_SRC = fsk.h fsk.c

BAUDOT_SRC = baudot.h baudot.c

minimodem_LDADD = $(DEPS_LIBS)
minimodem_SOURCES = minimodem.c $(BAUDOT_SRC) $(FSK_SRC) $(SIMPLEAUDIO_SRC)


