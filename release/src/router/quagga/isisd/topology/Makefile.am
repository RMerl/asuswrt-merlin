## Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"

AM_CFLAGS = $(PICFLAGS)
AM_LDFLAGS = $(PILDFLAGS)

noinst_LIBRARIES = libtopology.a

libtopology_a_SOURCES = \
	spgrid.c random.c

libtopology_a_DEPENDENCIES = @LIB_REGEX@

libtopology_a_LIBADD = @LIB_REGEX@ ../../lib/libzebra.la

noinst_HEADERS = \
	spgrid.h

## File dependency.
