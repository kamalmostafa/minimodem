project('minimodem', 'c')

# Copyright (C) 2011-2020 Kamal Mostafa <kamal@whence.com>

# -Wall # -Werror -Wdouble-promotion

add_project_arguments('-DUSE_ALSA=1', language: 'c')
add_project_arguments('-DUSE_PULSEAUDIO=1', language: 'c')

fftw3f_dep = dependency('fftw3f', required : true)

alsa_dep = dependency('alsa', required : false)

pa_dep = dependency('libpulse-simple', required : false)

sndfile_dep = dependency('sndfile', required : false)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)

simpleaudio_src = [
    'src/simpleaudio.c',
    'src/simple-tone-generator.c',
    'src/simpleaudio-pulse.c',
    'src/simpleaudio-alsa.c',
    'src/simpleaudio-sndio.c',
    'src/simpleaudio-benchmark.c',
    'src/simpleaudio-sndfile.c',
]

fsk_src = [
    'src/fsk.c',
]

baudot_src = [
    'src/baudot.c',
]

uic_src = [
    'src/uic_codes.c',
]

databits_src = [
    'src/databits_ascii.c',
    'src/databits_binary.c',
    'src/databits_callerid.c',
    'src/databits_baudot.c', baudot_src,
    'src/databits_uic.c', uic_src,
]

minimodem_src = [
    'src/minimodem.c',
    databits_src,
    fsk_src,
    simpleaudio_src,
]

executable('minimodem', minimodem_src, dependencies: [ fftw3f_dep, alsa_dep, pa_dep, sndfile_dep, m_dep ])
