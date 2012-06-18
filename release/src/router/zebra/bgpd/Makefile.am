## Process this file with automake to produce Makefile.in.

INCLUDES = @INCLUDES@ -I.. -I$(top_srcdir) -I$(top_srcdir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600

noinst_LIBRARIES = libbgp.a
sbin_PROGRAMS = bgpd

libbgp_a_SOURCES = \
	bgpd.c bgp_fsm.c bgp_aspath.c bgp_community.c bgp_attr.c \
	bgp_debug.c bgp_route.c bgp_zebra.c bgp_open.c bgp_routemap.c \
	bgp_packet.c bgp_network.c bgp_filter.c bgp_regex.c bgp_clist.c \
	bgp_dump.c bgp_snmp.c bgp_ecommunity.c bgp_mplsvpn.c bgp_nexthop.c \
	bgp_damp.c bgp_table.c bgp_advertise.c bgp_vty.c bgp_tcpsig.c

noinst_HEADERS = \
	bgp_aspath.h bgp_attr.h bgp_community.h bgp_debug.h bgp_fsm.h \
	bgp_network.h bgp_open.h bgp_packet.h bgp_regex.h bgp_route.h \
	bgpd.h bgp_filter.h bgp_clist.h bgp_dump.h bgp_zebra.h \
	bgp_ecommunity.h bgp_mplsvpn.h bgp_nexthop.h bgp_damp.h bgp_table.h \
	bgp_advertise.h bgp_snmp.h bgp_vty.h bgp_tcpsig.h

bgpd_SOURCES = \
	bgp_main.c $(libbgp_a_SOURCES)

bgpd_LDADD = ../lib/libzebra.a

sysconf_DATA = bgpd.conf.sample bgpd.conf.sample2

EXTRA_DIST = $(sysconf_DATA) BGP4-MIB.txt

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
