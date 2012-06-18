## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib
DEFS = @DEFS@ $(LOCAL_OPTS) -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

noinst_LIBRARIES = libospf.a
sbin_PROGRAMS = ospfd

libospf_a_SOURCES = \
	ospfd.c ospf_zebra.c ospf_interface.c ospf_ism.c ospf_neighbor.c \
	ospf_nsm.c ospf_dump.c ospf_network.c ospf_packet.c ospf_lsa.c \
	ospf_spf.c ospf_route.c ospf_ase.c ospf_abr.c ospf_ia.c ospf_flood.c \
	ospf_lsdb.c ospf_asbr.c ospf_routemap.c ospf_snmp.c \
	ospf_opaque.c ospf_te.c ospf_vty.c

noinst_HEADERS = \
	ospf_dump.h ospf_interface.h ospf_ism.h ospf_neighbor.h \
	ospf_network.h ospf_nsm.h ospf_packet.h ospf_zebra.h ospfd.h \
	ospf_lsa.h ospf_spf.h ospf_route.h ospf_ase.h ospf_abr.h ospf_ia.h \
	ospf_flood.h ospf_lsdb.h ospf_asbr.h ospf_snmp.h ospf_opaque.h \
	ospf_te.h ospf_vty.h

ospfd_SOURCES = \
	ospf_main.c $(libospf_a_SOURCES)

ospfd_LDADD = ../lib/libzebra.a

sysconf_DATA = ospfd.conf.sample

EXTRA_DIST = $(sysconf_DATA) OSPF-MIB.txt OSPF-TRAP-MIB.txt ChangeLog.opaque.txt

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
