## Process this file with automake to produce Makefile.in

CC=$(CC_FOR_BUILD)
LIBTOOL = @LIBTOOL@ --tag=CC

man8_MANS = rpcdebug.man
EXTRA_DIST = $(man8_MANS)

sbin_PROGRAMS = rpcdebug
rpcdebug_SOURCES = rpcdebug.c
rpcdebug_CFLAGS=$(CFLAGS_FOR_BUILD)
rpcdebug_CPPFLAGS=$(CPPFLAGS_FOR_BUILD) -I$(top_srcdir)/support/include
rpcdebug_LDFLAGS=$(LDFLAGS_FOR_BUILD)

MAINTAINERCLEANFILES = Makefile.in
