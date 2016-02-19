#
# Makefile.am
#
# Copyright (C) 2011-2016 Kamal Mostafa <kamal@whence.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

AM_CFLAGS = -Wall # -Werror -Wdouble-promotion

AM_CPPFLAGS = $(DEPS_CFLAGS)


bin_PROGRAMS = minimodem

dist_man_MANS = minimodem.1

EXTRA_DIST = minimodem.1.html

SIMPLEAUDIO_SRC = \
	simpleaudio.h		\
	simpleaudio_internal.h	\
	simpleaudio.c		\
	simple-tone-generator.c \
	simpleaudio-pulse.c	\
	simpleaudio-alsa.c	\
	simpleaudio-benchmark.c	\
	simpleaudio-sndfile.c

FSK_SRC = fsk.h fsk.c

BAUDOT_SRC = baudot.h baudot.c

UIC_SRC = uic_codes.h uic_codes.c

DATABITS_SRC = \
	databits.h \
	databits_ascii.c \
	databits_binary.c \
	databits_callerid.c \
	databits_baudot.c $(BAUDOT_SRC) \
	databits_uic.c $(UIC_SRC)

minimodem_LDADD = $(DEPS_LIBS)
minimodem_SOURCES = minimodem.c $(DATABITS_SRC) $(FSK_SRC) $(SIMPLEAUDIO_SRC)


minimodem.1.html: minimodem.1 Makefile
	man --html=cat ./minimodem.1 | sed '5,$$ s:\(http\:[^ &]*\):<A HREF="\1">\1</A>:g' > $@
	[ -s $@ ] || { rm $@; exit 1; }

