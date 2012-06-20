DEFS = -DLOCALEDIR=\"$(datadir)/locale\" @DEFS@
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/lib -I$(top_srcdir)/intl

bin_PROGRAMS = wol
bin_SCRIPTS = wol-dhcpdconf wol-bootptab
wol_SOURCES = wol.c macfile.c net.c magic.c ## proxy.c
wol_LDADD = @LIBINTL@ @EXTRA_LIBS@ $(top_builddir)/lib/libwrappers.a

MAINTAINERCLEANFILES = Makefile.in
