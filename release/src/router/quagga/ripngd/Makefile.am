## Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

AM_CFLAGS = $(PICFLAGS)
AM_LDFLAGS = $(PILDFLAGS)

noinst_LIBRARIES = libripng.a
sbin_PROGRAMS = ripngd

libripng_a_SOURCES = \
	ripng_interface.c ripngd.c ripng_zebra.c ripng_route.c ripng_debug.c \
	ripng_routemap.c ripng_offset.c ripng_peer.c ripng_nexthop.c

noinst_HEADERS = \
	ripng_debug.h ripng_route.h ripngd.h ripng_nexthop.h

ripngd_SOURCES = \
	ripng_main.c $(libripng_a_SOURCES)

ripngd_LDADD = ../lib/libzebra.la @LIBCAP@

examplesdir = $(exampledir)
dist_examples_DATA = ripngd.conf.sample

