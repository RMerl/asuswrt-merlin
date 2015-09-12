## Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

AM_CFLAGS = $(PICFLAGS)
AM_LDFLAGS = $(PILDFLAGS)

noinst_LIBRARIES = libbabel.a
sbin_PROGRAMS = babeld

libbabel_a_SOURCES = \
	babel_zebra.c net.c kernel.c util.c source.c neighbour.c	\
	route.c xroute.c message.c resend.c babel_interface.c babeld.c	\
	babel_filter.c

noinst_HEADERS = \
	babel_zebra.h net.h kernel.h util.h source.h neighbour.h	\
	route.h xroute.h message.h resend.h babel_interface.h babeld.h	\
	babel_filter.h babel_main.h

babeld_SOURCES = \
	babel_main.c $(libbabel_a_SOURCES)

babeld_LDADD = ../lib/libzebra.la @LIBCAP@

examplesdir = $(exampledir)
dist_examples_DATA = babeld.conf.sample
