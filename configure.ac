AC_PREREQ([2.64])
AC_INIT([minimodem], [0.24], [kamal@whence.com])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/Makefile.in])
AM_INIT_AUTOMAKE

VERSION=AC_PACKAGE_VERSION
AC_SUBST(VERSION)

# Program Checks
AC_PROG_CC

# Library Checks
AC_SEARCH_LIBS([lroundf], [m])

deps_packages="fftw3f"

#   ALSA
AC_ARG_WITH([alsa], [AS_HELP_STRING([--without-alsa],
            [build without ALSA support])])
AS_IF([test "x$with_alsa" == "xno"], [
    use_alsa=0
], [
    use_alsa=1
    deps_packages="$deps_packages alsa"
])
AC_DEFINE_UNQUOTED([USE_ALSA], [$use_alsa],
                   [Define to 1 to enable ALSA support])

#   pulseaudio
AC_ARG_WITH([pulseaudio], AS_HELP_STRING([--without-pulseaudio],
            [build without pulseaudio support]))
AS_IF([test "x$with_pulseaudio" == "xno"], [
    use_pulseaudio=0
], [
    use_pulseaudio=1
    deps_packages="$deps_packages libpulse-simple"
])
AC_DEFINE_UNQUOTED([USE_PULSEAUDIO], [$use_pulseaudio],
                   [Define to 1 to enable pulseaudio support])

#   sndfile
AC_ARG_WITH([sndfile], AS_HELP_STRING([--without-sndfile],
            [build without sndfile support]))
AS_IF([test "x$with_sndfile" == "xno"], [
    use_sndfile=0
], [
    use_sndfile=1
    deps_packages="$deps_packages sndfile"
])
AC_DEFINE_UNQUOTED([USE_SNDFILE], [$use_sndfile],
                   [Define to 1 to enable sndfile support])

#   benchmarks
AC_ARG_WITH([benchmarks], AS_HELP_STRING([--disable-benchmarks],
            [build without internal benchmarks]))
AS_IF([test "x$with_benchmarks" == "xno"], [
    use_benchmarks=0
], [
    use_benchmarks=1
])
AC_DEFINE_UNQUOTED([USE_BENCHMARKS], [$use_benchmarks],
                   [Define to 1 to enable internal benchmarks])

AC_MSG_RESULT([
option summary:
    alsa           $with_alsa ($use_alsa)
    benchmarks     $with_benchmarks ($use_benchmarks)
    pulseaudio     $with_pulseaudio ($use_pulseaudio)
    sndfile        $with_sndfile ($use_sndfile)
])

# Checks for libraries.
PKG_CHECK_MODULES(DEPS, [$deps_packages])

AC_SUBST([auto_find_tests], ['$(sort $(wildcard *.test))'])

AC_CONFIG_FILES([Makefile src/Makefile src/minimodem.1 tests/Makefile])
AC_OUTPUT
