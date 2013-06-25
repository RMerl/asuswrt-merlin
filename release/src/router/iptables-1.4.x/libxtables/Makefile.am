# -*- Makefile -*-

AM_CFLAGS   = ${regular_CFLAGS}
AM_CPPFLAGS = ${regular_CPPFLAGS} -I${top_builddir}/include -I${top_srcdir}/include -I${top_srcdir}/iptables ${kinclude_CPPFLAGS}

lib_LTLIBRARIES       = libxtables.la
libxtables_la_SOURCES = xtables.c xtoptions.c
libxtables_la_LDFLAGS = -version-info ${libxtables_vcurrent}:0:${libxtables_vage}
libxtables_la_LIBADD  =
if ENABLE_STATIC
# With --enable-static, shipped extensions are linked into the main executable,
# so we need all the LIBADDs here too
libxtables_la_LIBADD += -lm
endif
if ENABLE_SHARED
libxtables_la_CFLAGS  = ${AM_CFLAGS}
libxtables_la_LIBADD += -ldl
else
libxtables_la_CFLAGS  = ${AM_CFLAGS} -DNO_SHARED_LIBS=1
endif
