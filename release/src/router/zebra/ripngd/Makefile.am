## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

noinst_LIBRARIES = libripng.a
sbin_PROGRAMS = ripngd

libripng_a_SOURCES = \
	ripng_interface.c ripngd.c ripng_zebra.c ripng_route.c ripng_debug.c \
	ripng_routemap.c ripng_ifrmap.c

noinst_HEADERS = \
	ripng_debug.h ripng_route.h ripngd.h ripng_ifrmap.h

ripngd_SOURCES = \
	ripng_main.c $(libripng_a_SOURCES)

ripngd_LDADD = ../lib/libzebra.a

sysconf_DATA = ripngd.conf.sample

EXTRA_DIST = $(sysconf_DATA)

install-sysconfDATA: $(sysconf_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	@list='$(sysconf_DATA)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_SDATA) $(srcdir)/$$p $(DESTDIR)$(sysconfdir)/$$p"; \
	    $(INSTALL_SDATA) $(srcdir)/$$p $(DESTDIR)$(sysconfdir)/$$p; \
	  else if test -f $$p; then \
	    echo " $(INSTALL_SDATA) $$p $(DESTDIR)$(sysconfdir)/$$p"; \
	    $(INSTALL_SDATA) $$p $(DESTDIR)$(sysconfdir)/$$p; \
	  fi; fi; \
	done
