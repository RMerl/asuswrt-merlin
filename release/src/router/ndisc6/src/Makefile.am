# Makefile.am - src/ directory Makefile for ndisc6
# $Id: Makefile.am 664 2010-11-16 19:15:36Z remi $

# Copyright © 2006-2008 Rémi Denis-Courmont
# This file is distributed under the same license as the ndisc6 package.

bindir = @bindir@
sbindir = @sbindir@
mysuidexecdir = $(myexecdir)

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\"
AM_LIBADD = @top_builddir@/compat/libcompat.a

mysuidexec_PROGRAMS = ndisc6 rdisc6 rltraceroute6
myexec_PROGRAMS = tcptraceroute6
myexec_SCRIPTS = tracert6
bin_PROGRAMS = tcpspray name2addr
bin_SCRIPTS = tcpspray6 dnssort addr2name
noinst_HEADERS = gettime.h
dist_noinst_SCRIPTS = gen-alias
EXTRA_DIST = dnssort.in
MOSTLYCLEANFILES = $(myexec_SCRIPTS) $(bin_SCRIPTS)

gen_alias = transform="$(transform)" ${SHELL} $(srcdir)/gen-alias

sed_verbose = $(sed_verbose_$(V))
sed_verbose_ = $(sed_verbose_$(AM_DEFAULT_VERBOSITY))
sed_verbose_0 = @echo "  SED    $@";
alias_verbose = $(alias_verbose_$(V))
alias_verbose_ = $(alias_verbose_$(AM_DEFAULT_VERBOSITY))
alias_verbose_0 = @echo "  ALIAS  $@";

# ndisc6
ndisc6_SOURCES = ndisc.c
ndisc6_LDADD = $(LIBRT) $(AM_LIBADD)

# rdisc6
rdisc6_SOURCES = ndisc.c
rdisc6_CPPFLAGS = -DRDISC $(AM_CPPFLAGS)
rdisc6_LDADD = $(LIBRT) $(AM_LIBADD)

# traceroute6
rltraceroute6_SOURCES = traceroute.c traceroute.h \
			trace-tcp.c trace-udp.c trace-icmp.c
rltraceroute6_LDADD = $(LIBRT) $(AM_LIBADD)
tcptraceroute6_SOURCES = tcptraceroute.c
tcptraceroute6_CPPFLAGS = $(AM_CPPFLAGS) \
	-DRLTRACEROUTE6=\"`echo rltraceroute6 | sed '$(transform)'`\"

tracert6: Makefile.am gen-alias
	$(alias_verbose)$(gen_alias) tracert6 rltraceroute6 -I

# tcpspray
tcpspray_SOURCES = tcpspray.c
tcpspray_LDADD = $(LIBRT) $(AM_LIBADD)

tcpspray6: Makefile.am gen-alias
	$(alias_verbose)$(gen_alias) tcpspray6 tcpspray -6

# addr2name
name2addr_SOURCES = addrinfo.c
addr2name: Makefile.am gen-alias
	$(alias_verbose)$(gen_alias) addr2name name2addr -r

subst_script = sed \
	-e 's,[@]PERL[@],$(PERL),g' \
	-e 's,[@]PACKAGE[@],$(PACKAGE),g' \
	-e 's,[@]VERSION[@],$(VERSION),g'

# dnssort
dnssort: dnssort.in Makefile.in $(top_builddir)/config.status
	$(AM_V_at)rm -f $@.tmp $@
	$(sed_verbose)$(subst_script) < $(srcdir)/$@.in > $@.tmp
	$(AM_V_at)chmod +x $@.tmp
	$(AM_V_at)mv -f $@.tmp $@

if NOSUID
myexecdir = $(sbindir)
else
myexecdir = $(bindir)

# IMPORTANT NOTE for packagers:
# If you want to disable setuid installation,
# you can invoke the configure with '--disable-suid-install'.
install-exec-hook:
	@for f in $(mysuidexec_PROGRAMS); do \
		i="`echo "$$f" | sed -e '$(program)'`" ; \
		echo chmod u+s "$(DESTDIR)$(mysuidexecdir)/$$i" ; \
		chmod u+s "$(DESTDIR)$(mysuidexecdir)/$$i" || exit $$? ; \
	done
endif

